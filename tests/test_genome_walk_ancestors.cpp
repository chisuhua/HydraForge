// 2026-09-20-ig-genome-registry-walk-ancestors — Phase 6c MetaRSI-v1 C3 follow-up
// Tests: walk_ancestors + judge_data_freshness + cross-name + cycle + perf (≥6 cases, AC-12)
// Spec: openspec/changes/2026-09-20-ig-genome-registry-walk-ancestors/specs/ig-genome-registry-walk-ancestors/spec.md

#include "catch_amalgamated.hpp"

#include "agenticdsl/genome/genome.h"
#include "agenticdsl/types/attribution_version_pair_diff.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace {

// Set HMAC key for sandbox CI (HOME may be read-only).
// Per Oracle O-3 Major: prevents ~/.hydraforge/genome.key write failures.
void set_test_hmac_key() {
    static const char* kTestKey = "0123456789abcdef0123456789abcdef"
                                  "0123456789abcdef0123456789abcdef";
    ::setenv("HYDRAFORGE_GENOME_KEY", kTestKey, 1);
}

// Build a unique tmpdir per test case (avoids cross-test interference).
fs::path make_tmpdir(const std::string& name) {
    fs::path base = fs::temp_directory_path() /
                    ("hf-genome-walk-test-" + name + "-" +
                     std::to_string(::getpid()) + "-" +
                     std::to_string(std::rand()));
    fs::create_directories(base);
    return base;
}

// Build a basic Genome with given name@version + parent + harness.
agenticdsl::genome::Genome make_genome(const std::string& name, uint64_t version,
                                       const std::string& parent,
                                       const std::string& harness,
                                       const std::string& capture_mode = "mock") {
    agenticdsl::genome::Genome g;
    g.metadata.name = name;
    g.metadata.version = version;
    g.metadata.parent = parent;
    g.metadata.created_by = "test";
    g.metadata.created_at = "2026-09-20T00:00:00Z";
    g.metadata.capture_mode = capture_mode;
    g.spec.harness = harness;
    g.spec.budget = 1000;
    g.spec.model_routing = "default";
    return g;
}

}  // namespace

// ============================================================================
// Case 1: D9 default impl (ADR-0088 D9 + AGENTS.md pattern #9 ITimerService precedent)
// MockGenomeRegistry 不 override walk_ancestors → 走 D9 默认实现 → NotImplemented
// ============================================================================
TEST_CASE("walk_ancestors D9 default impl returns NotImplemented",
          "[walk-ancestors][d9-default]") {
    struct MockGenomeRegistry : agenticdsl::genome::IGenomeRegistry {
        agenticdsl::genome::Result<agenticdsl::genome::Genome, agenticdsl::genome::GenomeError>
        load(const std::string&, uint64_t) override {
            return agenticdsl::genome::Result<agenticdsl::genome::Genome, agenticdsl::genome::GenomeError>::failure(
                agenticdsl::genome::GenomeError::NotFound);
        }
        agenticdsl::genome::Result<agenticdsl::genome::CommitResult, agenticdsl::genome::GenomeError>
        commit(const agenticdsl::genome::Genome&) override {
            return agenticdsl::genome::Result<agenticdsl::genome::CommitResult, agenticdsl::genome::GenomeError>::failure(
                agenticdsl::genome::GenomeError::NotFound);
        }
        agenticdsl::genome::Result<agenticdsl::genome::CommitResult, agenticdsl::genome::GenomeError>
        fork(const std::string&, uint64_t, const agenticdsl::genome::GenomeSpec&) override {
            return agenticdsl::genome::Result<agenticdsl::genome::CommitResult, agenticdsl::genome::GenomeError>::failure(
                agenticdsl::genome::GenomeError::NotFound);
        }
        agenticdsl::genome::Result<std::vector<uint64_t>, agenticdsl::genome::GenomeError>
        list_versions(const std::string&) override {
            return agenticdsl::genome::Result<std::vector<uint64_t>, agenticdsl::genome::GenomeError>::failure(
                agenticdsl::genome::GenomeError::NotFound);
        }
        agenticdsl::genome::Result<agenticdsl::genome::GenomeDiff, agenticdsl::genome::GenomeError>
        diff(const std::string&, uint64_t, uint64_t) override {
            return agenticdsl::genome::Result<agenticdsl::genome::GenomeDiff, agenticdsl::genome::GenomeError>::failure(
                agenticdsl::genome::GenomeError::NotFound);
        }
        // walk_ancestors 不 override → D9 默认实现 → NotImplemented
    };

    MockGenomeRegistry reg;
    auto result = reg.walk_ancestors("g", 1);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == agenticdsl::genome::GenomeError::NotImplemented);
}

// ============================================================================
// Case 2: linear chain walk 3 versions, closest-first + self-inclusive
// Per spec §ig-genome-registry-walk-ancestors-extension Scenario "walk_ancestors signature"
// + Oracle Q4: LineageWalk.intermediate_metadata[i] = Genome (NOT GenomeMetadata)
// ============================================================================
TEST_CASE("walk_ancestors linear chain 3 versions closest-first self-inclusive",
          "[walk-ancestors][filesystem][chain]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("chain");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);

    // g@1 root → g@2 → g@3 (linear chain)
    REQUIRE(reg->commit(make_genome("g", 0, "", "harness-v1")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@1", "harness-v1")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@2", "harness-v2")).has_value());

    auto walk = reg->walk_ancestors("g", 3);
    REQUIRE(walk.has_value());

    // intermediate_versions: closest-first, self-inclusive → [3, 2, 1]
    REQUIRE(walk.value().intermediate_versions.size() == 3);
    REQUIRE(walk.value().intermediate_versions[0] == 3);
    REQUIRE(walk.value().intermediate_versions[1] == 2);
    REQUIRE(walk.value().intermediate_versions[2] == 1);

    // intermediate_metadata: same count + correct harness (NOT GenomeMetadata per Oracle Q4)
    REQUIRE(walk.value().intermediate_metadata.size() == 3);
    REQUIRE(walk.value().intermediate_metadata[0].metadata.version == 3);
    REQUIRE(walk.value().intermediate_metadata[0].spec.harness == "harness-v2");
    REQUIRE(walk.value().intermediate_metadata[1].metadata.version == 2);
    REQUIRE(walk.value().intermediate_metadata[1].spec.harness == "harness-v1");
    REQUIRE(walk.value().intermediate_metadata[2].metadata.version == 1);
    REQUIRE(walk.value().intermediate_metadata[2].spec.harness == "harness-v1");

    fs::remove_all(root);
}

// ============================================================================
// Case 3: walk with to_version stops at to_version (inclusive)
// Per spec §walk-ancestors-extension Scenario "FilesystemGenomeRegistry override"
// + AC-11: to_version 提供时 inclusive 停止
// ============================================================================
TEST_CASE("walk_ancestors with to_version stops inclusively",
          "[walk-ancestors][filesystem][to-version]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("to-version");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg->commit(make_genome("g", 0, "", "h")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@1", "h")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@2", "h")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@3", "h")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@4", "h")).has_value());

    auto walk = reg->walk_ancestors("g", 5, /*to_version=*/2);
    REQUIRE(walk.has_value());

    // inclusive stop: [5, 4, 3, 2] (to_version=2 included)
    REQUIRE(walk.value().intermediate_versions.size() == 4);
    REQUIRE(walk.value().intermediate_versions[0] == 5);
    REQUIRE(walk.value().intermediate_versions[1] == 4);
    REQUIRE(walk.value().intermediate_versions[2] == 3);
    REQUIRE(walk.value().intermediate_versions[3] == 2);

    fs::remove_all(root);
}

// ============================================================================
// Case 4: from_version not exists → NotFound
// Per spec §ig-genome-registry-walk-ancestors-extension Scenario "FilesystemGenomeRegistry override"
// ============================================================================
TEST_CASE("walk_ancestors from_version not exists returns NotFound",
          "[walk-ancestors][filesystem][not-found]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("not-found");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg->commit(make_genome("g", 0, "", "h")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@1", "h")).has_value());

    auto walk = reg->walk_ancestors("g", 99);
    REQUIRE_FALSE(walk.has_value());
    REQUIRE(walk.error() == agenticdsl::genome::GenomeError::NotFound);

    fs::remove_all(root);
}

// ============================================================================
// Case 5: 跨名 lineage → BrokenLineage + reason "cross-name lineage not supported in v1"
// Per spec §ig-genome-registry-walk-ancestors-extension Scenario "cross-name lineage semantics"
// + AC-8: data.name != current.name → Confounded (v1 仅同名)
// ============================================================================
TEST_CASE("walk_ancestors cross-name parent returns BrokenLineage",
          "[walk-ancestors][filesystem][cross-name]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("cross-name");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);
    // g@1 has parent "f@1" (different name)
    REQUIRE(reg->commit(make_genome("f", 0, "", "h")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "f@1", "h")).has_value());

    auto walk = reg->walk_ancestors("g", 1);
    REQUIRE_FALSE(walk.has_value());
    REQUIRE(walk.error() == agenticdsl::genome::GenomeError::BrokenLineage);

    fs::remove_all(root);
}

// ============================================================================
// Case 6: broken lineage parent → BrokenLineage (tampered files)
// Per spec Scenario "FilesystemGenomeRegistry override" + lineage handling
// ============================================================================
TEST_CASE("walk_ancestors broken lineage parent returns BrokenLineage",
          "[walk-ancestors][filesystem][broken-lineage]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("broken-lineage");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);
    REQUIRE(reg->commit(make_genome("g", 0, "", "h")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@1", "h")).has_value());

    // Tamper: rewrite g@2 to have non-existent parent g@99 (commit validation
    // prevents cycles; we exercise the BrokenLineage path via dangling parent).
    fs::path g2_yaml = root / "g/2/genome.yaml";
    {
        std::ofstream ofs(g2_yaml);
        ofs << "metadata:\n"
               "  name: g\n"
               "  version: 2\n"
               "  parent: \"g@99\"\n"
               "  created_by: tampered\n"
               "  created_at: \"2026-09-20T00:00:00Z\"\n"
               "  capture_mode: mock\n"
               "spec:\n"
               "  harness: h\n"
               "  tools: []\n"
               "  budget: 1000\n"
               "  model_routing: default\n"
               "  prompt_cache_prefix: \"\"\n";
    }

    auto walk = reg->walk_ancestors("g", 2);
    REQUIRE_FALSE(walk.has_value());
    REQUIRE(walk.error() == agenticdsl::genome::GenomeError::BrokenLineage);

    fs::remove_all(root);
}

// ============================================================================
// Case 10: real cycle detection via tampering (cycle: g@1 parent g@2 + g@2 parent g@1)
// (P2 fix from Oracle: exercises visited-set cycle path; commit-time M5 validation
//  prevents normal-path cycle creation, so test must bypass via direct file write)
// ============================================================================
TEST_CASE("walk_ancestors cycle via tampering returns BrokenLineage",
          "[walk-ancestors][filesystem][cycle]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("cycle");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);
    // Commit g@1 + g@2 normally (linear chain, no cycle)
    REQUIRE(reg->commit(make_genome("g", 0, "", "h")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@1", "h")).has_value());

    // Tamper: rewrite g@1/genome.yaml so its parent becomes "g@2" instead of ""
    // Result: g@2 → g@1 → g@2 cycle (M5 commit validation bypassed)
    fs::path g1_yaml = root / "g/1/genome.yaml";
    {
        std::ofstream ofs(g1_yaml);
        ofs << "metadata:\n"
               "  name: g\n"
               "  version: 1\n"
               "  parent: \"g@2\"\n"
               "  created_by: tampered\n"
               "  created_at: \"2026-09-20T00:00:00Z\"\n"
               "  capture_mode: mock\n"
               "spec:\n"
               "  harness: h\n"
               "  tools: []\n"
               "  budget: 1000\n"
               "  model_routing: default\n"
               "  prompt_cache_prefix: \"\"\n";
    }

    // Walk from g@2 → visits g@2 (parent=g@1) → visits g@1 (parent=g@2 per tampered)
    // → revisit g@2 → cycle detected
    auto walk = reg->walk_ancestors("g", 2);
    REQUIRE_FALSE(walk.has_value());
    REQUIRE(walk.error() == agenticdsl::genome::GenomeError::BrokenLineage);

    fs::remove_all(root);
}

// ============================================================================
// Case 7: judge_data_freshness integration — data in lineage, no Harness change → Attributed
// (Critical: covers D6 Case 5 + readiness gate condition 1, P0 fix from Oracle SHIP-with-fixes)
// ============================================================================
TEST_CASE("judge_data_freshness in-lineage no harness change returns Attributed",
          "[walk-ancestors][judge-data-freshness][integration]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("judge-attributed");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);
    // g@1 (h=v1) → g@2 (h=v1) → g@3 (h=v1) — same harness throughout
    REQUIRE(reg->commit(make_genome("g", 0, "", "v1")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@1", "v1")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@2", "v1")).has_value());

    auto verdict = agenticdsl::evolution::VersionPairDiff::judge_data_freshness(
        agenticdsl::evolution::GenomeVersion{"g", 1},
        agenticdsl::evolution::GenomeVersion{"g", 3},
        *reg);
    REQUIRE(verdict == agenticdsl::evolution::AttributionVerdict::Attributed);

    fs::remove_all(root);
}

// ============================================================================
// Case 8: judge_data_freshness integration — Harness changed after data → Confounded
// (Critical: covers D6 Case 4 + AC-8 harness detection, P0 fix from Oracle SHIP-with-fixes)
// ============================================================================
TEST_CASE("judge_data_freshness harness changed after data returns Confounded",
          "[walk-ancestors][judge-data-freshness][integration]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("judge-confounded");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);
    // g@1 (h=v1) → g@2 (h=v1) → g@3 (h=v2) — Harness changes at g@2
    REQUIRE(reg->commit(make_genome("g", 0, "", "v1")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@1", "v1")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@2", "v2")).has_value());

    auto verdict = agenticdsl::evolution::VersionPairDiff::judge_data_freshness(
        agenticdsl::evolution::GenomeVersion{"g", 1},
        agenticdsl::evolution::GenomeVersion{"g", 3},
        *reg);
    REQUIRE(verdict == agenticdsl::evolution::AttributionVerdict::Confounded);

    fs::remove_all(root);
}

// ============================================================================
// Case 9: judge_data_freshness integration — data not in lineage → Confounded
// (Critical: covers D6 Case 3 + AC-8 lineage lookup miss, P0 fix from Oracle SHIP-with-fixes)
// ============================================================================
TEST_CASE("judge_data_freshness data not in lineage returns Confounded",
          "[walk-ancestors][judge-data-freshness][integration]") {
    set_test_hmac_key();
    fs::path root = make_tmpdir("judge-not-in-lineage");

    auto reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(root);
    // g@1 → g@2 (no g@3 in lineage; data=99 → Confounded)
    REQUIRE(reg->commit(make_genome("g", 0, "", "v1")).has_value());
    REQUIRE(reg->commit(make_genome("g", 0, "g@1", "v1")).has_value());

    auto verdict = agenticdsl::evolution::VersionPairDiff::judge_data_freshness(
        agenticdsl::evolution::GenomeVersion{"g", 99},
        agenticdsl::evolution::GenomeVersion{"g", 2},
        *reg);
    REQUIRE(verdict == agenticdsl::evolution::AttributionVerdict::Confounded);

    fs::remove_all(root);
}