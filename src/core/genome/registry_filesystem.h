// src/core/genome/registry_filesystem.h
// FilesystemGenomeRegistry impl (D9 per Oracle bg_a818a6a1)
// 作者: C2 Sprint 35
// 日期: 2026-09-18
#pragma once

#include "agenticdsl/genome/genome.h"

#include <filesystem>

namespace agenticdsl::genome {

class FilesystemGenomeRegistry : public IGenomeRegistry {
public:
    explicit FilesystemGenomeRegistry(std::filesystem::path root);

    Result<Genome, GenomeError> load(const std::string& name, uint64_t version) override;
    Result<CommitResult, GenomeError> commit(const Genome& genome) override;
    Result<CommitResult, GenomeError> fork(const std::string& name,
                                            uint64_t parent_version,
                                            const GenomeSpec& mutations) override;
    Result<std::vector<uint64_t>, GenomeError> list_versions(const std::string& name) override;
    Result<GenomeDiff, GenomeError> diff(const std::string& name,
                                          uint64_t v1, uint64_t v2) override;

private:
    std::filesystem::path root_;
};

}  // namespace agenticdsl::genome