// src/modules/cognitive/cognitive_tools.cpp
// T5 cognitive-specialists-as-tools: GEPA/MCTS/SkillCompiler tool 注册
#include "agenticdsl/cognitive/cognitive_tools.h"
#include "common/tools/registry.h"
#include "common/policy/execution_policy.h"
#include "agenticdsl/cognitive/gepa_loop.h"
#include "agenticdsl/cognitive/mcts_workflow_search.h"
#include "agenticdsl/cognitive/skill_compiler.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace agenticdsl::cognitive {

using nlohmann::json;
using agenticdsl::ToolMetadata;
using agenticdsl::ToolCategory;
using agenticdsl::LayerProfile;
using agenticdsl::ApprovalPolicy;

namespace {

// L3+Worker 限定 (Cognitive L4 禁止 tool_call per ADR-0004 §8)
json build_input_schema_string(const std::string& desc) {
    return json{
        {"type", "object"},
        {"properties", {{"failed_trace", {{"type", "string"}, {"description", desc}}}}},
        {"required", json::array({"failed_trace"})}
    };
}

json build_input_schema_object(const std::string& desc) {
    return json{
        {"type", "object"},
        {"properties", {{"skill_md_content", {{"type", "string"}, {"description", desc}}}}},
        {"required", json::array({"skill_md_content"})}
    };
}

json build_input_schema_task(const std::string& desc) {
    return json{
        {"type", "object"},
        {"properties", {{"task_id", {{"type", "string"}, {"description", desc}}}}},
        {"required", json::array({"task_id"})}
    };
}

}  // namespace

void register_cognitive_tools(
    ToolRegistry& registry,
    std::shared_ptr<GEPALoop> gepa_loop,
    std::shared_ptr<MCTSWorkflowSearch> mcts_searcher,
    std::shared_ptr<SkillCompiler> skill_compiler) {

    // 1. evolution::reflect (GEPALoop::reflect_and_commit)
    if (gepa_loop) {
        ToolMetadata meta;
        meta.name = "evolution::reflect";
        meta.description = "Reflect on failed execution trace and propose mutation";
        meta.domain = "cognitive";
        meta.category = ToolCategory::StateModify;  // 修改 prompt/skill, 视为状态变更
        meta.min_layer = agenticdsl::LayerProfile::Thinking;
        meta.approval = ApprovalPolicy{true, true, false, false};  // plan+agent 审批, yolo 放行
        meta.allowed_layers = {agenticdsl::LayerProfile::Workflow, agenticdsl::LayerProfile::Thinking};
        meta.cost_estimate = 0.05;  // 单次 LLM reflect 估算 (T6 plan §风险)
        meta.timeout_ms = 30000;
        meta.input_schema = build_input_schema_string("Execution trace ID to reflect on");
        meta.output_schema = json{
            {"type", "object"},
            {"properties", {{"new_prompt", {{"type", "string"}}}}}
        };

        registry.register_tool("evolution::reflect", meta,
            [gepa_loop](const std::unordered_map<std::string, std::string>& args) -> json {
                // T5 plan: evolve args map → fake trace (real impl wires ExecutionTrace parsing)
                std::string trace_id = args.count("failed_trace") ? args.at("failed_trace") : "t5_unnamed";
                agenticdsl::ExecutionTrace trace;
                trace.trace_id = trace_id;
                auto result = gepa_loop->reflect_and_commit(trace);
                json out;
                out["ok"] = result.success;
                if (!result.success && !result.failure_mode.empty()) {
                    out["failure_mode"] = result.failure_mode;
                }
                if (!result.candidate_skills.empty()) {
                    out["new_prompt"] = result.candidate_skills.front();
                }
                return out;
            });
    }

    // 2. evolution::search (MCTSWorkflowSearch::search)
    if (mcts_searcher) {
        ToolMetadata meta;
        meta.name = "evolution::search";
        meta.description = "MCTS workflow search for optimal cognitive domain chain";
        meta.domain = "cognitive";
        meta.category = ToolCategory::Execute;  // 调用 MCTS 搜索算法
        meta.min_layer = agenticdsl::LayerProfile::Thinking;
        meta.approval = ApprovalPolicy{true, true, false, false};
        meta.allowed_layers = {agenticdsl::LayerProfile::Workflow, agenticdsl::LayerProfile::Thinking};
        meta.cost_estimate = 0.10;  // 完整 MCTS 搜索 (含嵌套预算 per T2 decision 5)
        meta.timeout_ms = 60000;
        meta.input_schema = build_input_schema_task("Task spec ID for MCTS search");
        meta.output_schema = json{
            {"type", "object"},
            {"properties", {{"best_workflow", {{"type", "object"}}}}}
        };

        registry.register_tool("evolution::search", meta,
            [mcts_searcher](const std::unordered_map<std::string, std::string>& args) -> json {
                std::string task_id = args.count("task_id") ? args.at("task_id") : "t5_default";
                agenticdsl::TaskSpec spec;
                spec.task_id = task_id;
                auto result = mcts_searcher->search(spec);
                json out;
                out["ok"] = result.success;
                if (!result.success) {
                    out["failure_mode"] = result.failure_mode;
                }
                // best_workflow 字段映射 (per T2 commit_chain semantics)
                out["best_workflow"] = {{"task_id", task_id}, {"result", result.success}};
                return out;
            });
    }

    // 3. evolution::compile (SkillCompiler::compile)
    if (skill_compiler) {
        ToolMetadata meta;
        meta.name = "evolution::compile";
        meta.description = "Compile SKILL.md content into executable skill form";
        meta.domain = "cognitive";
        meta.category = ToolCategory::StateModify;  // 修改 SKILL.md 内容
        meta.min_layer = agenticdsl::LayerProfile::Thinking;
        meta.approval = ApprovalPolicy{true, true, false, false};
        meta.allowed_layers = {agenticdsl::LayerProfile::Workflow, agenticdsl::LayerProfile::Thinking};
        meta.cost_estimate = 0.02;  // 本地编译, 无 LLM
        meta.timeout_ms = 10000;
        meta.input_schema = build_input_schema_object("Raw SKILL.md content to compile");
        meta.output_schema = json{
            {"type", "object"},
            {"properties", {{"compiled_skill_path", {{"type", "string"}}}}}
        };

        registry.register_tool("evolution::compile", meta,
            [skill_compiler](const std::unordered_map<std::string, std::string>& args) -> json {
                std::string content = args.count("skill_md_content") ? args.at("skill_md_content") : "";
                auto result = skill_compiler->compile(content);
                json out;
                out["ok"] = result.ok;
                if (!result.ok && !result.failure_reason.empty()) {
                    out["failure_reason"] = result.failure_reason;
                }
                out["compiled_content"] = result.compiled_content;
                return out;
            });
    }
}

}  // namespace agenticdsl::cognitive
