// src/core/genome/canonical_yaml.cpp
// canonical YAML serialization for HMAC consistency (per Oracle pitfall #2)
// 字段顺序固定 (deterministic order), 跨平台一致性
// 作者: C2 Sprint 35
// 日期: 2026-09-18

#include "agenticdsl/genome/genome.h"

#include <yaml-cpp/yaml.h>

#include <sstream>

namespace agenticdsl::genome {

namespace {

void emit_metadata(YAML::Emitter& out, const GenomeMetadata& m) {
    out << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << m.name;
    out << YAML::Key << "version" << YAML::Value << m.version;
    // parent "" 仍写入 (HMAC 覆盖)
    out << YAML::Key << "parent" << YAML::Value << m.parent;
    out << YAML::Key << "created_by" << YAML::Value << m.created_by;
    out << YAML::Key << "created_at" << YAML::Value << m.created_at;
    out << YAML::Key << "capture_mode" << YAML::Value << m.capture_mode;
    out << YAML::EndMap;
}

void emit_spec(YAML::Emitter& out, const GenomeSpec& s) {
    out << YAML::BeginMap;
    out << YAML::Key << "harness" << YAML::Value << s.harness;
    out << YAML::Key << "tools" << YAML::Value << YAML::BeginSeq;
    for (const auto& t : s.tools) out << t;
    out << YAML::EndSeq;
    out << YAML::Key << "budget" << YAML::Value << s.budget;
    out << YAML::Key << "model_routing" << YAML::Value << s.model_routing;
    out << YAML::Key << "prompt_cache_prefix" << YAML::Value << s.prompt_cache_prefix;
    out << YAML::EndMap;
}

}  // namespace

std::string canonical_yaml_serialize(const Genome& g) {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "metadata" << YAML::Value;
    emit_metadata(out, g.metadata);
    out << YAML::Key << "spec" << YAML::Value;
    emit_spec(out, g.spec);
    out << YAML::EndMap;
    return out.c_str();
}

}  // namespace agenticdsl::genome