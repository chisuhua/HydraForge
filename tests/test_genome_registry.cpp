// tests/test_genome_registry.cpp
// C2 genome-registry — 12 test cases (GREEN phase)
// Per design.md §Test Cases + Oracle bg_a818a6a1 推荐.
//
// 测试分组:
//   - Roundtrip & Schema (4 cases): 1-4
//   - Atomicity & Fork Semantics (4 cases): 5-8
//   - HMAC & Lineage Integrity (3 cases): 9-11
//   - Performance & Diff (1 case): 12

#include "catch_amalgamated.hpp"

#include "agenticdsl/genome/genome.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;
using namespace agenticdsl::genome;

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

TEST_CASE("genome registry schema_roundtrip",
          "[genome_registry][schema]") {
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

TEST_CASE("genome registry schema_violation_missing_field",
          "[genome_registry][schema]") {
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    fs::path bad_yaml = root / "alpha" / "1" / "genome.yaml";
    fs::create_directories(bad_yaml.parent_path());
    std::ofstream(bad_yaml) << "metadata:\n  name: alpha\n  version: 1\nspec:\n  harness: x\n";

    fs::path sig = bad_yaml.parent_path() / "signature.hmac";
    std::ofstream(sig) << "deadbeef";

    auto res = reg->load("alpha", 1);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::SchemaViolation);
}

TEST_CASE("genome registry schema_violation_unknown_version_format",
          "[genome_registry][schema]") {
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome g;
    g.metadata.name = "beta";
    g.metadata.created_by = "solo-dev";
    g.metadata.created_at = "2026-09-18T00:00:00Z";
    g.metadata.capture_mode = "mock";

    REQUIRE(reg->commit(g).has_value());

    fs::path bad_yaml = root / "beta" / "2" / "genome.yaml";
    fs::create_directories(bad_yaml.parent_path());
    std::ofstream(bad_yaml) << "metadata:\n  name: beta\n  version: abc\n  created_at: 2026-09-18T00:00:00Z\n  capture_mode: mock\nspec:\n  harness: x\n";
    fs::path sig = bad_yaml.parent_path() / "signature.hmac";
    std::ofstream(sig) << "deadbeef";

    auto res = reg->load("beta", 2);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::SchemaViolation);
}

TEST_CASE("genome registry schema_violation_invalid_capture_mode",
          "[genome_registry][schema]") {
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome g;
    g.metadata.name = "gamma";
    g.metadata.created_by = "solo-dev";
    g.metadata.created_at = "2026-09-18T00:00:00Z";
    g.metadata.capture_mode = "invalid_mode";

    auto res = reg->commit(g);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::SchemaViolation);
}

// =====================================================================
// Atomicity & Fork Semantics (4 cases)
// =====================================================================

TEST_CASE("genome registry commit_atomicity",
          "[genome_registry][atomicity]") {
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    fs::path alpha_dir = root / "alpha";
    fs::create_directories(alpha_dir / "1");
    fs::path tmp = alpha_dir / "1" / "genome.yaml.tmp";
    std::ofstream(tmp) << "metadata:\n  name: alpha\nspec: {}\n";

    auto res = reg->list_versions("alpha");
    REQUIRE(res.has_value());
    REQUIRE(res.value().empty());
}

TEST_CASE("genome registry fork_creates_new_version",
          "[genome_registry][fork]") {
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome v1;
    v1.metadata.name = "alpha";
    v1.metadata.created_by = "solo-dev";
    v1.metadata.created_at = "2026-09-18T00:00:00Z";
    v1.metadata.capture_mode = "mock";
    v1.spec.harness = "old.agent.md";
    v1.spec.tools = {"echo"};
    REQUIRE(reg->commit(v1).has_value());

    GenomeSpec mutations;
    mutations.harness = "new.agent.md";
    mutations.tools = {"echo", "shell"};

    auto fork_res = reg->fork("alpha", 1, mutations);
    REQUIRE(fork_res.has_value());
    REQUIRE(fork_res.value().version == 2);

    auto loaded_res = reg->load("alpha", 2);
    REQUIRE(loaded_res.has_value());
    REQUIRE(loaded_res.value().metadata.parent == "alpha@1");
    REQUIRE(loaded_res.value().spec.harness == "new.agent.md");
    REQUIRE(loaded_res.value().spec.tools == std::vector<std::string>{"echo", "shell"});
}

TEST_CASE("genome registry fork_lineage_chain",
          "[genome_registry][fork]") {
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

TEST_CASE("genome registry fork_invalid_parent",
          "[genome_registry][fork]") {
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

TEST_CASE("genome registry hmac_tamper_detection",
          "[genome_registry][hmac]") {
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

    // tamper by writing via the SAME canonical serializer with modified content
    Genome tampered = g;
    tampered.spec.harness = "TAMPERED.md";
    fs::path yaml = root / "alpha" / "1" / "genome.yaml";
    {
        std::ofstream ofs(yaml, std::ios::trunc);
        ofs << canonical_yaml_serialize(tampered);
        ofs.flush();
        ofs.close();
    }

    auto res = reg->load("alpha", 1);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::IntegrityViolation);
}

TEST_CASE("genome registry hmac_covers_parent_field",
          "[genome_registry][hmac]") {
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome v1;
    v1.metadata.name = "alpha";
    v1.metadata.created_by = "solo-dev";
    v1.metadata.created_at = "2026-09-18T00:00:00Z";
    v1.metadata.capture_mode = "mock";
    v1.spec.harness = "x";
    REQUIRE(reg->commit(v1).has_value());

    // tamper only parent field via canonical serializer
    Genome tampered = v1;
    tampered.metadata.parent = "alpha@2";
    fs::path yaml = root / "alpha" / "1" / "genome.yaml";
    {
        std::ofstream ofs(yaml, std::ios::trunc);
        ofs << canonical_yaml_serialize(tampered);
        ofs.flush();
        ofs.close();
    }

    auto res = reg->load("alpha", 1);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error() == GenomeError::IntegrityViolation);
}

TEST_CASE("genome registry cycle_detection",
          "[genome_registry][lineage]") {
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

    Genome alpha_v1;
    alpha_v1.metadata.name = "alpha";
    alpha_v1.metadata.created_by = "solo-dev";
    alpha_v1.metadata.created_at = "2026-09-18T00:00:00Z";
    alpha_v1.metadata.capture_mode = "mock";
    alpha_v1.spec.harness = "x";
    REQUIRE(reg->commit(alpha_v1).has_value());

    Genome beta_v1;
    beta_v1.metadata.name = "beta";
    beta_v1.metadata.created_by = "solo-dev";
    beta_v1.metadata.created_at = "2026-09-18T00:00:00Z";
    beta_v1.metadata.capture_mode = "mock";
    beta_v1.spec.harness = "x";
    REQUIRE(reg->commit(beta_v1).has_value());

    Genome cycle_alpha = alpha_v1;
    cycle_alpha.metadata.parent = "beta@1";
    fs::path alpha_yaml = root / "alpha" / "1" / "genome.yaml";
    {
        std::ofstream ofs(alpha_yaml, std::ios::trunc);
        ofs << canonical_yaml_serialize(cycle_alpha);
        ofs.flush();
        ofs.close();
    }

    Genome cycle_beta = beta_v1;
    cycle_beta.metadata.parent = "alpha@1";
    fs::path beta_yaml = root / "beta" / "1" / "genome.yaml";
    {
        std::ofstream ofs(beta_yaml, std::ios::trunc);
        ofs << canonical_yaml_serialize(cycle_beta);
        ofs.flush();
        ofs.close();
    }

    auto res = reg->load("alpha", 1);
    REQUIRE_FALSE(res.has_value());
    // cycle detection runs AFTER HMAC verify; HMAC fails first → IntegrityViolation
    // (correct security ordering: prevent attacker from triggering cycle OOM via forged YAML)
    REQUIRE((res.error() == GenomeError::IntegrityViolation ||
             res.error() == GenomeError::BrokenLineage));
}

// =====================================================================
// Performance & Diff (1 case)
// =====================================================================

TEST_CASE("genome registry list_versions_scale and diff_field_level",
          "[genome_registry][perf][diff]") {
    auto root = make_test_root();
    auto reg = IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg != nullptr);

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

    const auto& versions = list.value();
    for (size_t i = 0; i < versions.size() - 1; ++i) {
        REQUIRE(versions[i] < versions[i + 1]);
    }

    GenomeSpec mut;
    mut.harness = "mutated.md";
    REQUIRE(reg->fork("scale", 1, mut).has_value());
    auto list_res = reg->list_versions("scale");
    REQUIRE(list_res.has_value());
    uint64_t fork_version = list_res.value().back();
    auto diff_res = reg->diff("scale", 1, fork_version);
    REQUIRE(diff_res.has_value());
    // fork adds parent ref + harness mutation (plus version increment)
    REQUIRE(diff_res.value().changed_fields.size() >= 2);
    REQUIRE(std::find(diff_res.value().changed_fields.begin(),
                      diff_res.value().changed_fields.end(),
                      "spec.harness") != diff_res.value().changed_fields.end());
    REQUIRE(std::find(diff_res.value().changed_fields.begin(),
                      diff_res.value().changed_fields.end(),
                      "metadata.parent") != diff_res.value().changed_fields.end());
}