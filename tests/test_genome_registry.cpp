// tests/test_genome_registry.cpp
// C2 genome-registry — 12 test cases (RED phase)
// Per design.md §Test Cases + Oracle bg_a818a6a1 推荐.
//
// 测试分组:
//   - Roundtrip & Schema (4 cases): 1-4
//   - Atomicity & Fork Semantics (4 cases): 5-8
//   - HMAC & Lineage Integrity (3 cases): 9-11
//   - Performance & Diff (1 case): 12

#include "catch_amalgamated.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace agenticdsl::genome {

enum class GenomeError {
    NotFound,
    SchemaViolation,
    IntegrityViolation,
    BrokenLineage,
    CycleDetected,
    IOError,
};

template <typename T, typename E>
class Result {
public:
    bool has_value() const { return has_val_; }
    const T& value() const { return val_; }
    const E& error() const { return err_; }

    static Result success(T v) {
        Result r;
        r.has_val_ = true;
        r.val_ = std::move(v);
        return r;
    }
    static Result failure(E e) {
        Result r;
        r.has_val_ = false;
        r.err_ = std::move(e);
        return r;
    }
private:
    Result() = default;
    bool has_val_ = false;
    T val_;
    E err_;
};

struct GenomeMetadata {
    std::string name;
    uint64_t version = 0;
    std::string parent;
    std::string created_by;
    std::string created_at;
    std::string capture_mode;
};

struct GenomeSpec {
    std::string harness;
    std::vector<std::string> tools;
    uint64_t budget = 0;
    std::string model_routing;
    std::string prompt_cache_prefix;
};

struct Genome {
    GenomeMetadata metadata;
    GenomeSpec spec;
};

struct CommitResult {
    uint64_t version;
};

struct GenomeDiff {
    std::vector<std::string> changed_fields;
};

class IGenomeRegistry {
public:
    virtual ~IGenomeRegistry() = default;

    virtual Result<Genome, GenomeError> load(const std::string& name, uint64_t version) = 0;
    virtual Result<CommitResult, GenomeError> commit(const Genome& genome) = 0;
    virtual Result<CommitResult, GenomeError> fork(const std::string& name, uint64_t parent_version,
                                                    const GenomeSpec& mutations) = 0;
    virtual Result<std::vector<uint64_t>, GenomeError> list_versions(const std::string& name) = 0;
    virtual Result<GenomeDiff, GenomeError> diff(const std::string& name,
                                                  uint64_t v1, uint64_t v2) = 0;

    static std::unique_ptr<IGenomeRegistry> create_filesystem(const fs::path& root);
};

inline bool is_valid_capture_mode(const std::string& mode) {
    return mode == "mock" || mode == "real" || mode == "hybrid";
}

// RED stub factory — GREEN impl will replace this with FilesystemGenomeRegistry.
inline std::unique_ptr<IGenomeRegistry> IGenomeRegistry::create_filesystem(const fs::path& /*root*/) {
    return nullptr;
}

}  // namespace agenticdsl::genome

// Helper: create isolated test root under /tmp or build dir
static fs::path make_test_root() {
    static int counter = 0;
    fs::path root = fs::temp_directory_path() /
        ("hydraforge_genome_test_" + std::to_string(getpid()) +
         "_" + std::to_string(++counter));
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

// =====================================================================
// Roundtrip & Schema (4 cases)
// =====================================================================

// Case 1: schema_roundtrip
// 构造全字段 Genome → commit → load → 字段级相等（含 nested harness/tools）
TEST_CASE("genome registry schema_roundtrip",
          "[genome_registry][schema]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome g;
    g.metadata.name = "alpha";
    g.metadata.created_by = "solo-dev";
    g.metadata.created_at = "2026-09-18T00:00:00Z";
    g.metadata.capture_mode = "mock";
    g.spec.harness = "lib/loop/react.agent.md";
    g.spec.tools = {"echo", "shell", "fs_read"};
    g.spec.budget = 5000;
    g.spec.model_routing = "default";
    g.spec.prompt_cache_prefix = "v1_";

    auto commit_res = reg->commit(g);
    REQUIRE(commit_res.has_value());
    auto loaded_res = reg->load("alpha", 1);

    REQUIRE(loaded_res.has_value());
    const auto& loaded = loaded_res.value();
    REQUIRE(loaded.metadata.name == g.metadata.name);
    REQUIRE(loaded.metadata.version == 1);
    REQUIRE(loaded.spec.harness == g.spec.harness);
    REQUIRE(loaded.spec.tools == g.spec.tools);
    REQUIRE(loaded.spec.budget == g.spec.budget);
    REQUIRE(loaded.spec.model_routing == g.spec.model_routing);
    REQUIRE(loaded.spec.prompt_cache_prefix == g.spec.prompt_cache_prefix);
}

// Case 2: schema_violation_missing_field
// 缺 parent 字段的 YAML → load 返回 SchemaViolation (注: 当前 v1 允许 parent="")
// 此 case 验证 YAML 字段验证 (catch2 hand-crafted YAML missing fields)
TEST_CASE("genome registry schema_violation_missing_field",
          "[genome_registry][schema]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    // Manually craft a malformed YAML (missing required field `created_at`)
    fs::path bad_yaml = root / "alpha" / "1" / "genome.yaml";
    fs::create_directories(bad_yaml.parent_path());
    std::ofstream(bad_yaml) << "metadata:\n  name: alpha\n  version: 1\nspec:\n  harness: x\n";

    // Manually craft valid signature (production code would sign, here we bypass)
    fs::path sig = bad_yaml.parent_path() / "signature.hmac";
    std::ofstream(sig) << "deadbeef";  // placeholder

    auto res = reg->load("alpha", 1);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::SchemaViolation);
}

// Case 3: schema_violation_unknown_version_format
// version='abc' 非规范格式 → commit 拒绝
TEST_CASE("genome registry schema_violation_unknown_version_format",
          "[genome_registry][schema]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome g;
    g.metadata.name = "beta";
    g.metadata.created_by = "solo-dev";
    g.metadata.created_at = "2026-09-18T00:00:00Z";
    g.metadata.capture_mode = "mock";

    // First commit to establish v1
    REQUIRE(reg->commit(g).has_value());

    // Manually craft v2 with non-numeric version in YAML
    fs::path bad_yaml = root / "beta" / "2" / "genome.yaml";
    fs::create_directories(bad_yaml.parent_path());
    std::ofstream(bad_yaml) << "metadata:\n  name: beta\n  version: abc\n  created_at: 2026-09-18T00:00:00Z\n  capture_mode: mock\nspec:\n  harness: x\n";
    fs::path sig = bad_yaml.parent_path() / "signature.hmac";
    std::ofstream(sig) << "deadbeef";

    auto res = reg->load("beta", 2);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::SchemaViolation);
}

// Case 4: schema_violation_invalid_capture_mode
// capture_mode 不在 enum → SchemaViolation
TEST_CASE("genome registry schema_violation_invalid_capture_mode",
          "[genome_registry][schema]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome g;
    g.metadata.name = "gamma";
    g.metadata.created_by = "solo-dev";
    g.metadata.created_at = "2026-09-18T00:00:00Z";
    g.metadata.capture_mode = "invalid_mode";  // not in {mock, real, hybrid}

    auto res = reg->commit(g);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::SchemaViolation);
}

// =====================================================================
// Atomicity & Fork Semantics (4 cases)
// =====================================================================

// Case 5: commit_atomicity
// commit 模拟崩溃（写到 .tmp 未 rename）→ list_versions 不出现半成品
TEST_CASE("genome registry commit_atomicity",
          "[genome_registry][atomicity]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    // Simulate crash mid-commit: write to .tmp but never rename
    fs::path alpha_dir = root / "alpha";
    fs::create_directories(alpha_dir / "1");
    fs::path tmp = alpha_dir / "1" / "genome.yaml.tmp";
    std::ofstream(tmp) << "metadata:\n  name: alpha\nspec: {}\n";
    // NOTE: no rename happens, simulating crash

    auto res = reg->list_versions("alpha");
    // Atomic guarantee: half-written version MUST NOT appear
    REQUIRE(res.has_value());
    REQUIRE(res.value().empty());
}

// Case 6: fork_creates_new_version
// fork(parent@v1, mutations) → 新版本 parent 字段 == v1 hash, mutation 字段已应用
TEST_CASE("genome registry fork_creates_new_version",
          "[genome_registry][fork]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    // Seed v1
    Genome v1;
    v1.metadata.name = "alpha";
    v1.metadata.created_by = "solo-dev";
    v1.metadata.created_at = "2026-09-18T00:00:00Z";
    v1.metadata.capture_mode = "mock";
    v1.spec.harness = "old.agent.md";
    v1.spec.tools = {"echo"};
    REQUIRE(reg->commit(v1).has_value());

    // Fork with mutations
    GenomeSpec mutations;
    mutations.harness = "new.agent.md";
    mutations.tools = {"echo", "shell"};

    auto fork_res = reg->fork("alpha", 1, mutations);
    REQUIRE(fork_res.has_value());
    REQUIRE(fork_res.value().version == 2);

    // Load v2 and verify deep-merge + parent ref
    auto loaded_res = reg->load("alpha", 2);
    REQUIRE(loaded_res.has_value());
    REQUIRE(loaded_res.value().metadata.parent == "alpha@1");
    REQUIRE(loaded_res.value().spec.harness == "new.agent.md");  // mutation applied
    REQUIRE(loaded_res.value().spec.tools == std::vector<std::string>{"echo", "shell"});  // mutation applied
}

// Case 7: fork_lineage_chain
// v1→v2→v3 链式 fork → walk_ancestors(v3) 返回 [v2, v1] 顺序正确
TEST_CASE("genome registry fork_lineage_chain",
          "[genome_registry][fork]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    auto seed = [&](uint64_t v) {
        Genome g;
        g.metadata.name = "alpha";
        g.metadata.created_by = "solo-dev";
        g.metadata.created_at = "2026-09-18T00:00:00Z";
        g.metadata.capture_mode = "mock";
        g.spec.harness = "v" + std::to_string(v) + ".md";
        REQUIRE(reg->commit(g).has_value());
    };

    seed(1);
    GenomeSpec mut2; mut2.harness = "v2.md";
    REQUIRE(reg->fork("alpha", 1, mut2).has_value());
    GenomeSpec mut3; mut3.harness = "v3.md";
    REQUIRE(reg->fork("alpha", 2, mut3).has_value());

    // Verify lineage via list + manual walk (walk_ancestors is internal in v1)
    auto list = reg->list_versions("alpha");
    REQUIRE(list.has_value());
    REQUIRE(list.value() == std::vector<uint64_t>{1, 2, 3});

    auto v3 = reg->load("alpha", 3);
    REQUIRE(v3.has_value());
    REQUIRE(v3.value().metadata.parent == "alpha@2");

    auto v2 = reg->load("alpha", 2);
    REQUIRE(v2.has_value());
    REQUIRE(v2.value().metadata.parent == "alpha@1");
}

// Case 8: fork_invalid_parent
// fork(non-existent@v99) → NotFound
TEST_CASE("genome registry fork_invalid_parent",
          "[genome_registry][fork]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    GenomeSpec mut;
    mut.harness = "x";

    auto res = reg->fork("alpha", 99, mut);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::NotFound);
}

// =====================================================================
// HMAC & Lineage Integrity (3 cases)
// =====================================================================

// Case 9: hmac_tamper_detection
// commit 后手动改 genome.yaml 一字节 → IntegrityViolation
TEST_CASE("genome registry hmac_tamper_detection",
          "[genome_registry][hmac]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome g;
    g.metadata.name = "alpha";
    g.metadata.created_by = "solo-dev";
    g.metadata.created_at = "2026-09-18T00:00:00Z";
    g.metadata.capture_mode = "mock";
    g.spec.harness = "original.md";
    REQUIRE(reg->commit(g).has_value());

    // Tamper genome.yaml — change harness value
    fs::path yaml = root / "alpha" / "1" / "genome.yaml";
    std::ofstream yaml_ofs(yaml, std::ios::trunc);
    yaml_ofs << "metadata:\n  name: alpha\n  version: 1\n  created_at: 2026-09-18T00:00:00Z\n  capture_mode: mock\nspec:\n  harness: tampered.md\n";

    auto res = reg->load("alpha", 1);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::IntegrityViolation);
}

// Case 10: hmac_covers_parent_field
// 篡改 parent 字段（不改其他）→ IntegrityViolation
TEST_CASE("genome registry hmac_covers_parent_field",
          "[genome_registry][hmac]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    // Seed v1 (no parent)
    Genome v1;
    v1.metadata.name = "alpha";
    v1.metadata.created_by = "solo-dev";
    v1.metadata.created_at = "2026-09-18T00:00:00Z";
    v1.metadata.capture_mode = "mock";
    v1.spec.harness = "x";
    REQUIRE(reg->commit(v1).has_value());

    // Tamper only parent field — set to "alpha@2" (non-existent)
    fs::path yaml = root / "alpha" / "1" / "genome.yaml";
    std::string content;
    {
        std::ifstream ifs(yaml);
        std::getline(ifs, content, '\0');  // read all
    }
    // Inject parent field (replace metadata block to add parent)
    // Production code must sign over this — tampering should fail HMAC
    std::ofstream yaml_ofs(yaml, std::ios::trunc);
    yaml_ofs << "metadata:\n  name: alpha\n  version: 1\n  parent: alpha@2\n  created_at: 2026-09-18T00:00:00Z\n  capture_mode: mock\nspec:\n  harness: x\n";

    auto res = reg->load("alpha", 1);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::IntegrityViolation);
}

// Case 11: cycle_detection
// 手工构造 A.parent=B, B.parent=A → load/walk 返回 BrokenLineage
TEST_CASE("genome registry cycle_detection",
          "[genome_registry][lineage]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    // Construct A and B with mutual parent refs (manual YAML crafting)
    auto write_genome = [&](const std::string& name, uint64_t version,
                             const std::string& parent) {
        fs::path dir = root / name / std::to_string(version);
        fs::create_directories(dir);
        std::ofstream yaml(dir / "genome.yaml");
        yaml << "metadata:\n"
             << "  name: " << name << "\n"
             << "  version: " << version << "\n"
             << "  parent: " << parent << "\n"
             << "  created_at: 2026-09-18T00:00:00Z\n"
             << "  capture_mode: mock\n"
             << "spec:\n  harness: x\n";
        std::ofstream(dir / "signature.hmac") << "deadbeef";
    };

    write_genome("alpha", 1, "beta@1");
    write_genome("beta", 1, "alpha@1");

    auto res = reg->load("alpha", 1);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::BrokenLineage);
}

// =====================================================================
// Performance & Diff (1 case)
// =====================================================================

// Case 12: list_versions_scale + diff_field_level
// commit 100 个版本 → list_versions < 50ms 且按版本序排列
// diff 字段级精确性（v1 vs v50 仅 system_prompt 不同）
TEST_CASE("genome registry list_versions_scale and diff_field_level",
          "[genome_registry][perf][diff]") {
    using namespace agenticdsl::genome;
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    // Commit 100 versions
    for (uint64_t v = 1; v <= 100; ++v) {
        Genome g;
        g.metadata.name = "scale";
        g.metadata.created_by = "solo-dev";
        g.metadata.created_at = "2026-09-18T00:00:00Z";
        g.metadata.capture_mode = "mock";
        g.spec.harness = "v" + std::to_string(v) + ".md";
        REQUIRE(reg->commit(g).has_value());
    }

    auto start = std::chrono::steady_clock::now();
    auto list = reg->list_versions("scale");
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    REQUIRE(list.has_value());
    REQUIRE(list.value().size() == 100);
    REQUIRE(elapsed_ms < 50);

    // Verify ordering
    const auto& versions = list.value();
    for (size_t i = 0; i < versions.size() - 1; ++i) {
        REQUIRE(versions[i] < versions[i + 1]);
    }

    // diff_field_level: fork v1 with single mutation, verify diff
    GenomeSpec mut;
    mut.harness = "mutated.md";
    REQUIRE(reg->fork("scale", 1, mut).has_value());
    auto diff_res = reg->diff("scale", 1, 2);
    REQUIRE(diff_res.has_value());
    REQUIRE(diff_res.value().changed_fields.size() == 1);
    REQUIRE(diff_res.value().changed_fields[0] == "spec.harness");
}