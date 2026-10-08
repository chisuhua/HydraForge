// src/core/genome/registry_filesystem.h
// FilesystemGenomeRegistry impl (D9 per Oracle bg_a818a6a1)
// 作者: C2 Sprint 35
// 日期: 2026-09-18
#pragma once

#include "agenticdsl/genome/genome.h"

#include <filesystem>
#include <mutex>

namespace agenticdsl::genome {

class FilesystemGenomeRegistry : public IGenomeRegistry {
public:
    explicit FilesystemGenomeRegistry(std::filesystem::path root);

    std::expected<Genome, GenomeError> load(const std::string& name, uint64_t version) override;
    std::expected<CommitResult, GenomeError> commit(const Genome& genome) override;
    std::expected<CommitResult, GenomeError> fork(const std::string& name,
                                            uint64_t parent_version,
                                            const GenomeSpec& mutations) override;
    std::expected<std::vector<uint64_t>, GenomeError> list_versions(const std::string& name) override;
    std::expected<GenomeDiff, GenomeError> diff(const std::string& name,
                                          uint64_t v1, uint64_t v2) override;
    std::expected<LineageWalk, GenomeError> walk_ancestors(
        const std::string& name, uint64_t from_version,
        std::optional<uint64_t> to_version = std::nullopt) override;

private:
    std::filesystem::path root_;
    std::mutex commit_mutex_;  // M4 fix: serialize concurrent commits (Oracle review)
};

}  // namespace agenticdsl::genome