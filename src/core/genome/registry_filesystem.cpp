// src/core/genome/registry_filesystem.cpp
// FilesystemGenomeRegistry impl
// 设计: D9 filesystem + D10 HMAC + D11 unlimited lineage
// Oracle bg_a818a6a1 reviewed, 5 pitfalls avoided:
//   #2 canonical bytes for HMAC
//   #3 tmp + rename atomic write
//   #5 monotonic integer version (no semver)
//   不暴露 walk_ancestors 到 public API (per #4)
// Oracle dual-agent review (bg_9ade564d + bg_89293120) — post-review fixes:
//   C1: visited set keyed on (name, version) pair, depth cap 10000
//   C2: walk wrapped in try/catch → IOError/SchemaViolation; IOError returned on fs failures
//   M2: HMAC key uses OpenSSL RAND_bytes (not mt19937_64)
//   M4: commit guarded by std::mutex
//   M5: cycle/dangling-parent validation also at commit (not only load)
// 作者: C2 Sprint 35
// 日期: 2026-09-18

#include "registry_filesystem.h"
#include "agenticdsl/genome/genome.h"
#include "agenticdsl/genome/hmac.h"

#include <openssl/rand.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <unordered_set>
#include <variant>

namespace fs = std::filesystem;

namespace agenticdsl::genome {

namespace {

// Default HMAC key path (per D10)
const std::string kDefaultKeyPath = "~/.hydraforge/genome.key";
const std::string kEnvKeyOverride = "HYDRAFORGE_GENOME_KEY";
constexpr size_t kMaxLineageDepth = 10000;  // C1 defense-in-depth cap

// Manual tilde expansion (C++17 filesystem lacks expand_user for arbitrary paths)
fs::path expand_tilde(const std::string& p) {
    if (!p.empty() && p[0] == '~') {
        if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
            return fs::path(std::string(home) + p.substr(1));
        }
    }
    return fs::path(p);
}

// Load HMAC key from env or default path; auto-generate if missing.
// M2 fix: use OpenSSL RAND_bytes (CSPRNG) not mt19937_64 (only 32-bit seed entropy).
std::string load_or_generate_hmac_key() {
    if (const char* env = std::getenv(kEnvKeyOverride.c_str()); env && env[0] != '\0') {
        return env;
    }
    fs::path key_path = expand_tilde(kDefaultKeyPath);
    if (fs::exists(key_path)) {
        std::ifstream ifs(key_path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string key = ss.str();
        if (!key.empty()) return key;
    }
    unsigned char raw[32];
    if (RAND_bytes(raw, sizeof(raw)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    static const char* hex = "0123456789abcdef";
    std::string key;
    key.reserve(64);
    for (int i = 0; i < 32; ++i) {
        key.push_back(hex[raw[i] >> 4]);
        key.push_back(hex[raw[i] & 0xF]);
    }
    fs::create_directories(key_path.parent_path());
    std::ofstream ofs(key_path);
    ofs << key;
    ofs.close();
    fs::permissions(key_path, fs::perms::owner_read | fs::perms::owner_write);
    return key;
}

std::string format_version_path(const std::string& name, uint64_t version) {
    return name + "/" + std::to_string(version);
}

Genome parse_genome_yaml(const fs::path& yaml_path) {
    YAML::Node root = YAML::LoadFile(yaml_path.string());
    Genome g;
    const auto& md = root["metadata"];
    g.metadata.name = md["name"].as<std::string>();
    g.metadata.version = md["version"].as<uint64_t>();
    g.metadata.parent = md["parent"] ? md["parent"].as<std::string>() : std::string{};
    g.metadata.created_by = md["created_by"].as<std::string>();
    g.metadata.created_at = md["created_at"].as<std::string>();
    g.metadata.capture_mode = md["capture_mode"].as<std::string>();

    const auto& sp = root["spec"];
    g.spec.harness = sp["harness"].as<std::string>();
    if (sp["tools"]) {
        for (const auto& t : sp["tools"]) {
            g.spec.tools.push_back(t.as<std::string>());
        }
    }
    g.spec.budget = sp["budget"].as<uint64_t>();
    g.spec.model_routing = sp["model_routing"].as<std::string>();
    g.spec.prompt_cache_prefix = sp["prompt_cache_prefix"].as<std::string>();
    return g;
}

bool is_valid_version_field(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

// C1 fix: cycle detection keyed on (name, version) pairs + depth cap.
// Previous version-only key missed cross-name cycles (e.g. x@1 → a@1 → b@1 → a@1)
// and could infinite-loop on adversarial/tampered lineage.
Result<std::monostate, GenomeError> validate_lineage_no_cycle(
    const fs::path& root, const std::string& name, uint64_t version,
    const Genome& child) {
    std::unordered_set<std::string> visited;
    std::string cur_name = child.metadata.parent.empty() ? "" : name;
    uint64_t cur_version = 0;
    {
        auto p = child.metadata.parent.find('@');
        if (p != std::string::npos) {
            cur_name = child.metadata.parent.substr(0, p);
            cur_version = std::stoull(child.metadata.parent.substr(p + 1));
        }
    }
    size_t depth = 0;
    while (!cur_name.empty()) {
        if (++depth > kMaxLineageDepth) {
            return Result<std::monostate, GenomeError>::failure(GenomeError::BrokenLineage);
        }
        std::string key = cur_name + "@" + std::to_string(cur_version);
        if (visited.count(key)) {
            return Result<std::monostate, GenomeError>::failure(GenomeError::BrokenLineage);
        }
        visited.insert(key);
        fs::path yaml_path = root / cur_name / std::to_string(cur_version) / "genome.yaml";
        if (!fs::exists(yaml_path)) {
            return Result<std::monostate, GenomeError>::failure(GenomeError::NotFound);
        }
        Genome parent_g;
        try {
            parent_g = parse_genome_yaml(yaml_path);
        } catch (const std::exception&) {
            return Result<std::monostate, GenomeError>::failure(GenomeError::SchemaViolation);
        }
        if (parent_g.metadata.parent.empty()) break;
        auto pp = parent_g.metadata.parent.find('@');
        if (pp == std::string::npos) {
            return Result<std::monostate, GenomeError>::failure(GenomeError::SchemaViolation);
        }
        cur_name = parent_g.metadata.parent.substr(0, pp);
        try {
            cur_version = std::stoull(parent_g.metadata.parent.substr(pp + 1));
        } catch (const std::exception&) {
            return Result<std::monostate, GenomeError>::failure(GenomeError::SchemaViolation);
        }
    }
    return Result<std::monostate, GenomeError>::success(std::monostate{});
}

GenomeDiff diff_genomes(const Genome& a, const Genome& b) {
    GenomeDiff diff;
    if (a.metadata.name != b.metadata.name) diff.changed_fields.push_back("metadata.name");
    if (a.metadata.version != b.metadata.version) diff.changed_fields.push_back("metadata.version");
    if (a.metadata.parent != b.metadata.parent) diff.changed_fields.push_back("metadata.parent");
    if (a.metadata.created_by != b.metadata.created_by) diff.changed_fields.push_back("metadata.created_by");
    if (a.metadata.created_at != b.metadata.created_at) diff.changed_fields.push_back("metadata.created_at");
    if (a.metadata.capture_mode != b.metadata.capture_mode) diff.changed_fields.push_back("metadata.capture_mode");
    if (a.spec.harness != b.spec.harness) diff.changed_fields.push_back("spec.harness");
    if (a.spec.tools != b.spec.tools) diff.changed_fields.push_back("spec.tools");
    if (a.spec.budget != b.spec.budget) diff.changed_fields.push_back("spec.budget");
    if (a.spec.model_routing != b.spec.model_routing) diff.changed_fields.push_back("spec.model_routing");
    if (a.spec.prompt_cache_prefix != b.spec.prompt_cache_prefix) diff.changed_fields.push_back("spec.prompt_cache_prefix");
    return diff;
}

GenomeSpec deep_merge_spec(const GenomeSpec& parent, const GenomeSpec& mutations) {
    GenomeSpec result = parent;
    if (!mutations.harness.empty()) result.harness = mutations.harness;
    if (!mutations.tools.empty()) result.tools = mutations.tools;
    if (mutations.budget != 0) result.budget = mutations.budget;
    if (!mutations.model_routing.empty()) result.model_routing = mutations.model_routing;
    if (!mutations.prompt_cache_prefix.empty()) result.prompt_cache_prefix = mutations.prompt_cache_prefix;
    return result;
}

// C2/M3 fix: atomic write returns success/failure; check stream state + handle exceptions
Result<std::monostate, GenomeError> atomic_write(const fs::path& final_path, const std::string& content) {
    fs::path tmp_path = final_path;
    tmp_path += ".tmp";
    try {
        {
            std::ofstream ofs(tmp_path, std::ios::trunc | std::ios::binary);
            if (!ofs.is_open()) {
                return Result<std::monostate, GenomeError>::failure(GenomeError::IOError);
            }
            ofs << content;
            ofs.flush();
            if (!ofs.good()) {
                fs::remove(tmp_path);
                return Result<std::monostate, GenomeError>::failure(GenomeError::IOError);
            }
        }
        fs::rename(tmp_path, final_path);
    } catch (const std::filesystem::filesystem_error&) {
        return Result<std::monostate, GenomeError>::failure(GenomeError::IOError);
    }
    return Result<std::monostate, GenomeError>::success(std::monostate{});
}

// m1 fix: fork generates fresh RFC3339 UTC timestamp (not inherit parent)
std::string current_rfc3339_utc() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
    gmtime_r(&t, &tm_utc);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    return std::string(buf);
}

}  // namespace

FilesystemGenomeRegistry::FilesystemGenomeRegistry(fs::path root)
    : root_(std::move(root)) {
    std::error_code ec;
    fs::create_directories(root_, ec);
}

Result<Genome, GenomeError> FilesystemGenomeRegistry::load(const std::string& name, uint64_t version) {
    fs::path version_dir = root_ / format_version_path(name, version);
    fs::path yaml_path = version_dir / "genome.yaml";
    fs::path sig_path = version_dir / "signature.hmac";

    // M1 fix: require BOTH files (atomicity half-state excluded)
    std::error_code ec;
    if (!fs::exists(yaml_path, ec) || !fs::exists(sig_path, ec)) {
        return Result<Genome, GenomeError>::failure(GenomeError::NotFound);
    }

    std::string sig_hex;
    try {
        std::ifstream sig_ifs(sig_path);
        std::stringstream sig_ss;
        sig_ss << sig_ifs.rdbuf();
        sig_hex = sig_ss.str();
    } catch (const std::exception&) {
        return Result<Genome, GenomeError>::failure(GenomeError::IOError);
    }

    Genome g;
    try {
        g = parse_genome_yaml(yaml_path);
    } catch (const std::exception&) {
        return Result<Genome, GenomeError>::failure(GenomeError::SchemaViolation);
    }

    if (!is_valid_capture_mode(g.metadata.capture_mode)) {
        return Result<Genome, GenomeError>::failure(GenomeError::SchemaViolation);
    }
    // version consistency check (loaded version must match dir name)
    if (g.metadata.version != version) {
        return Result<Genome, GenomeError>::failure(GenomeError::SchemaViolation);
    }

    std::string canonical = canonical_yaml_serialize(g);
    std::string key;
    try {
        key = load_or_generate_hmac_key();
    } catch (const std::exception&) {
        return Result<Genome, GenomeError>::failure(GenomeError::IOError);
    }
    if (!hmac_verify(key, canonical, sig_hex)) {
        return Result<Genome, GenomeError>::failure(GenomeError::IntegrityViolation);
    }

    if (!g.metadata.parent.empty()) {
        auto cv = validate_lineage_no_cycle(root_, name, version, g);
        if (!cv.has_value()) {
            return Result<Genome, GenomeError>::failure(cv.error());
        }
    }
    return Result<Genome, GenomeError>::success(g);
}

Result<CommitResult, GenomeError> FilesystemGenomeRegistry::commit(const Genome& g) {
    // M4 fix: serialize concurrent commits via internal mutex
    std::lock_guard<std::mutex> lock(commit_mutex_);

    if (g.metadata.name.empty()) {
        return Result<CommitResult, GenomeError>::failure(GenomeError::SchemaViolation);
    }
    if (!is_valid_capture_mode(g.metadata.capture_mode)) {
        return Result<CommitResult, GenomeError>::failure(GenomeError::SchemaViolation);
    }

    uint64_t next_version = 1;
    fs::path name_dir = root_ / g.metadata.name;
    std::error_code ec;
    if (fs::exists(name_dir, ec)) {
        for (const auto& entry : fs::directory_iterator(name_dir, ec)) {
            if (entry.is_directory()) {
                std::string vname = entry.path().filename().string();
                if (!is_valid_version_field(vname)) continue;
                uint64_t v = std::stoull(vname);
                if (v >= next_version) next_version = v + 1;
            }
        }
    }

    Genome to_write = g;
    to_write.metadata.version = next_version;

    // M5 fix: validate lineage at commit (not only load) — prevent persisting broken lineage
    if (!to_write.metadata.parent.empty()) {
        auto cv = validate_lineage_no_cycle(root_, g.metadata.name, next_version, to_write);
        if (!cv.has_value()) {
            return Result<CommitResult, GenomeError>::failure(cv.error());
        }
    }

    std::string canonical = canonical_yaml_serialize(to_write);
    std::string key;
    try {
        key = load_or_generate_hmac_key();
    } catch (const std::exception&) {
        return Result<CommitResult, GenomeError>::failure(GenomeError::IOError);
    }
    std::string sig = hmac_sign(key, canonical);

    fs::path version_dir = root_ / format_version_path(g.metadata.name, next_version);
    std::error_code ec2;
    fs::create_directories(version_dir, ec2);
    if (ec2) {
        return Result<CommitResult, GenomeError>::failure(GenomeError::IOError);
    }

    // M1 fix: write signature FIRST, genome.yaml SECOND (signature.hmac is last to rename)
    // → commit succeeds iff BOTH files exist; crash between creates half-state that
    //   load()/list_versions rejects (atomicity guarantee strengthened).
    auto sig_res = atomic_write(version_dir / "signature.hmac", sig);
    if (!sig_res.has_value()) {
        std::error_code rmec;
        fs::remove_all(version_dir, rmec);
        return Result<CommitResult, GenomeError>::failure(sig_res.error());
    }
    auto yaml_res = atomic_write(version_dir / "genome.yaml", canonical);
    if (!yaml_res.has_value()) {
        std::error_code rmec;
        fs::remove_all(version_dir, rmec);
        return Result<CommitResult, GenomeError>::failure(yaml_res.error());
    }

    return Result<CommitResult, GenomeError>::success({next_version});
}

Result<CommitResult, GenomeError> FilesystemGenomeRegistry::fork(
    const std::string& name, uint64_t parent_version, const GenomeSpec& mutations) {
    auto parent_res = load(name, parent_version);
    if (!parent_res.has_value()) {
        return Result<CommitResult, GenomeError>::failure(parent_res.error());
    }
    Genome parent = parent_res.value();
    Genome child;
    child.metadata.name = name;
    child.metadata.parent = name + "@" + std::to_string(parent_version);
    child.metadata.created_by = "fork";
    child.metadata.created_at = current_rfc3339_utc();  // m1 fix: fresh timestamp
    child.metadata.capture_mode = parent.metadata.capture_mode;
    child.spec = deep_merge_spec(parent.spec, mutations);
    return commit(child);
}

Result<std::vector<uint64_t>, GenomeError> FilesystemGenomeRegistry::list_versions(
    const std::string& name) {
    fs::path name_dir = root_ / name;
    std::vector<uint64_t> versions;
    std::error_code ec;
    if (!fs::exists(name_dir, ec)) {
        return Result<std::vector<uint64_t>, GenomeError>::success(versions);
    }
    for (const auto& entry : fs::directory_iterator(name_dir, ec)) {
        if (entry.is_directory()) {
            fs::path version_dir = entry.path();
            std::string vname = entry.path().filename().string();
            if (!is_valid_version_field(vname)) continue;
            // M1 fix: require BOTH genome.yaml and signature.hmac (atomicity)
            fs::path yaml_p = version_dir / "genome.yaml";
            fs::path sig_p = version_dir / "signature.hmac";
            if (!fs::exists(yaml_p, ec) || !fs::exists(sig_p, ec)) continue;
            if (fs::exists(version_dir / "genome.yaml.tmp", ec)) continue;
            versions.push_back(std::stoull(vname));
        }
    }
    std::sort(versions.begin(), versions.end());
    return Result<std::vector<uint64_t>, GenomeError>::success(versions);
}

Result<GenomeDiff, GenomeError> FilesystemGenomeRegistry::diff(
    const std::string& name, uint64_t v1, uint64_t v2) {
    auto a = load(name, v1);
    if (!a.has_value()) return Result<GenomeDiff, GenomeError>::failure(a.error());
    auto b = load(name, v2);
    if (!b.has_value()) return Result<GenomeDiff, GenomeError>::failure(b.error());
    return Result<GenomeDiff, GenomeError>::success(diff_genomes(a.value(), b.value()));
}

std::unique_ptr<IGenomeRegistry> IGenomeRegistry::create_filesystem(const fs::path& root) {
    return std::make_unique<FilesystemGenomeRegistry>(root);
}

}  // namespace agenticdsl::genome