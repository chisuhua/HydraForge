// include/agenticdsl/genome/genome.h
// C2 genome-registry — Genome + Registry interface + Result + ErrorCode
// 设计依据: openspec/changes/2026-09-16-genome-registry/{design,spec}.md
// Oracle review: bg_a818a6a1 (session ses_f478a3e49fferX6CU6U4tJTIiB)
// 作者: C2 Sprint 35
// 日期: 2026-09-18
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace agenticdsl::genome {

// D7 GenomeError enum — 独立定义 (per Oracle obs #1, 不对齐 ToolResult::ErrorCode)
enum class GenomeError {
    NotFound,
    SchemaViolation,
    IntegrityViolation,
    BrokenLineage,
    CycleDetected,
    IOError,
};

// Result<T, E> — 复用 llm_types.h pattern
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

// D2 Genome metadata — 单调整数 version (no semver, Oracle pitfall #5)
struct GenomeMetadata {
    std::string name;
    uint64_t version = 0;
    std::string parent;        // "name@version" or "" (root)
    std::string created_by;
    std::string created_at;    // RFC3339 UTC
    std::string capture_mode;  // "mock" | "real" | "hybrid"
};

// D3 Genome spec
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

// D4 IGenomeRegistry interface — 5 public + 1 internal (walk_ancestors)
class IGenomeRegistry {
public:
    virtual ~IGenomeRegistry() = default;

    virtual Result<Genome, GenomeError> load(const std::string& name, uint64_t version) = 0;
    virtual Result<CommitResult, GenomeError> commit(const Genome& genome) = 0;
    virtual Result<CommitResult, GenomeError> fork(const std::string& name,
                                                    uint64_t parent_version,
                                                    const GenomeSpec& mutations) = 0;
    virtual Result<std::vector<uint64_t>, GenomeError> list_versions(const std::string& name) = 0;
    virtual Result<GenomeDiff, GenomeError> diff(const std::string& name,
                                                  uint64_t v1, uint64_t v2) = 0;

    // Factory: filesystem backend with custom root (per D9 + design.md)
    static std::unique_ptr<IGenomeRegistry> create_filesystem(const std::filesystem::path& root);
};

// capture_mode enum validation (per design.md D2)
inline bool is_valid_capture_mode(const std::string& mode) {
    return mode == "mock" || mode == "real" || mode == "hybrid";
}

// canonical serialization (per Oracle pitfall #2) — 用于 HMAC 签名
std::string canonical_yaml_serialize(const Genome& g);

}  // namespace agenticdsl::genome