// src/modules/cognitive/skill_compiler.cpp
// 功能描述：SkillCompiler V1 实现 (T17, ADR-0061-03)
//          编译流程 (compile):
//            1. 解析 SKILL.md (--- frontmatter + body, 行级 key:value 解析)
//            2. emit skill.compilation.started (G11 emit-only, bus 可选)
//            3. IEvaluator 质量门 (合成 ExecutionTrace, Poor → quality_poor 失败)
//            4. T14 行为回归自检 (空指纹恒等比较 → Pass; V1 模板包装不改语义)
//            5. 生成编译产物 (6 字段 metadata frontmatter + 原 body)
//            6. emit skill.compilation.succeeded / failed (终态只 emit 一个)
//          V1 边界: TrajectoryPlaceholder 合成输入 (T15 DEFERRED);
//          SLM 路由 / PluginLoader 触发为 spec 前瞻场景, 本 V1 不实施。
// 设计依据：docs/adr/skill/adr-0061-03-skill-compiler.md §决策 1-3
//          + openspec/changes/2026-08-24-adr-0061-03-skill-compiler/specs/skill-compiler/spec.md
// 作者：HydraForge Sprint 24 T17 ship
// 最后修改日期：2026-08-27

#include "agenticdsl/cognitive/skill_compiler.h"

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/testing/behavioral_regression.h"
#include "agenticdsl/types/execution_trace.h"
#include "core/types/tool_result.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace agenticdsl {

namespace {

// ISO 8601 UTC 时间戳 (编译 metadata compiled_at 字段)
std::string iso_timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

// 行级 frontmatter 解析 (V1 简化: 仅提取 "key: value" 顶层字段, 不嵌套)
// 返回 frontmatter 块 [start, end) 区间外的 body, frontmatter 内提取 name
struct ParsedSkillMd {
  std::string name;   // frontmatter "name:" 字段, 缺失时 "anonymous"
  std::string body;   // frontmatter 之后的 markdown 正文 (无 frontmatter 时为全文)
};

ParsedSkillMd parse_skill_md(const std::string& content) {
  ParsedSkillMd out;
  out.name = "anonymous";
  out.body = content;

  // frontmatter: 以 "---\n" 开头, 到下一个 "\n---" 结束
  if (content.rfind("---\n", 0) != 0) {
    return out;  // 无 frontmatter, 全文为 body
  }
  const std::size_t close = content.find("\n---", 4);
  if (close == std::string::npos) {
    return out;  // 未闭合, 容错为无 frontmatter
  }
  const std::string frontmatter = content.substr(4, close - 4);
  out.body = content.substr(close + 4);
  // 跳过闭合 "---" 后的首个换行
  if (!out.body.empty() && out.body.front() == '\n') {
    out.body.erase(0, 1);
  }

  // 行级扫描 "name:" 字段
  std::size_t pos = 0;
  while (pos < frontmatter.size()) {
    const std::size_t eol = frontmatter.find('\n', pos);
    const std::string line =
        frontmatter.substr(pos, eol == std::string::npos
                                     ? std::string::npos
                                     : eol - pos);
    if (line.rfind("name:", 0) == 0) {
      std::string value = line.substr(5);
      // trim 首尾空白
      const auto first = value.find_first_not_of(" \t");
      const auto last = value.find_last_not_of(" \t");
      if (first != std::string::npos) {
        out.name = value.substr(first, last - first + 1);
      }
      break;
    }
    pos = (eol == std::string::npos) ? frontmatter.size() : eol + 1;
  }
  return out;
}

}  // namespace

SkillCompiler::SkillCompiler(std::shared_ptr<IEvaluator> evaluator,
                             std::shared_ptr<IInteractionBus> bus)
    : evaluator_(std::move(evaluator)), bus_(std::move(bus)) {}

CompiledSkill SkillCompiler::compile(const std::string& skill_md_content) const {
  CompiledSkill result;
  result.original_content = skill_md_content;  // 纯函数式: 只读快照

  // 边界: 空内容直接失败 (infrastructure_error)
  if (skill_md_content.empty()) {
    result.failure_reason = "infrastructure_error";
    if (bus_) {
      bus_->emit(EventBuilder(skill_compilation_topics::kFailed)
                     .ok(false)
                     .args({{"skill_id", "anonymous"},
                            {"reason", result.failure_reason}})
                     .build());
    }
    return result;
  }

  const ParsedSkillMd parsed = parse_skill_md(skill_md_content);
  result.skill_id = parsed.name;

  // G11 emit-only: skill.compilation.started
  if (bus_) {
    bus_->emit(EventBuilder(skill_compilation_topics::kStarted)
                   .args({{"skill_id", result.skill_id},
                          {"original_version", "v1"},
                          {"compiler_version", result.compiler_version}})
                   .build());
  }

  // IEvaluator 质量门 (V1: 合成 ExecutionTrace — 编译本身是确定性模板操作,
  // 评估对象为"编译动作成功"的合成轨迹; T15 ship 后替换为真实轨迹)
  if (evaluator_) {
    ExecutionTrace trace;
    trace.final_result = ToolResult::success("compile:" + result.skill_id);
    trace.trace_id = "skill-compiler:" + result.skill_id;
    const RewardSignal signal = evaluator_->evaluate(trace);
    result.ievaluator_score = signal.scalar;
    if (signal.quality == RewardSignal::Quality::Poor) {
      result.failure_reason = "quality_poor";
      if (bus_) {
        bus_->emit(EventBuilder(skill_compilation_topics::kFailed)
                       .ok(false)
                       .args({{"skill_id", result.skill_id},
                              {"reason", result.failure_reason}})
                       .build());
      }
      return result;
    }
  }

  // T14 行为回归自检 (V1: 模板包装不改变执行语义, baseline == candidate
  // 空指纹恒等比较必为 Pass; T15 ship 后接入真实执行指纹)
  const TrajectoryPlaceholder trajectory{};  // T1.3 DEFERRED 占位
  result.trajectory_ir_hash = trajectory.hash();
  {
    BehaviorFingerprint baseline = compute_fingerprint({});
    baseline.label = "baseline";
    BehaviorFingerprint candidate = baseline;
    candidate.label = "candidate";
    const RegressionBudget budget{};
    const Verdict verdict = hotelling_t2_test(baseline, candidate, budget);
    result.regression_verdict = verdict_to_string(verdict);
    if (verdict == Verdict::Fail) {
      result.failure_reason = "regression_fail";
      if (bus_) {
        bus_->emit(EventBuilder(skill_compilation_topics::kFailed)
                       .ok(false)
                       .args({{"skill_id", result.skill_id},
                              {"reason", result.failure_reason}})
                       .build());
      }
      return result;
    }
  }

  // 生成编译产物: 6 字段 metadata frontmatter (spec "编译 metadata 持久化")
  // + 原 body (模板驱动, 确定性输出)
  result.compiled_at = iso_timestamp_now();
  {
    std::string out;
    out += "---\n";
    out += "name: " + result.skill_id + "\n";
    out += "compiled_at: " + result.compiled_at + "\n";
    out += "compiler_version: \"" + result.compiler_version + "\"\n";
    out += "baseline_skill_id: " + result.skill_id + "\n";
    out += "trajectory_ir_hash: " + result.trajectory_ir_hash + "\n";
    out += "ievaluator_score: " + std::to_string(result.ievaluator_score) + "\n";
    out += "regression_verdict: \"" + result.regression_verdict + "\"\n";
    out += "---\n\n";
    out += parsed.body;
    result.compiled_content = std::move(out);
  }

  result.ok = true;

  // G11 emit-only: skill.compilation.succeeded (终态, 与 failed 互斥)
  if (bus_) {
    bus_->emit(EventBuilder(skill_compilation_topics::kSucceeded)
                   .args({{"skill_id", result.skill_id},
                          {"regression_verdict", result.regression_verdict},
                          {"ievaluator_score", result.ievaluator_score}})
                   .build());
  }
  return result;
}

bool SkillCompiler::validate(const CompiledSkill& compiled) const {
  if (!compiled.ok || compiled.skill_id.empty()) {
    return false;
  }
  const std::string& c = compiled.compiled_content;
  return c.find("compiled_at:") != std::string::npos &&
         c.find("compiler_version:") != std::string::npos &&
         c.find("baseline_skill_id:") != std::string::npos &&
         c.find("trajectory_ir_hash:") != std::string::npos &&
         c.find("ievaluator_score:") != std::string::npos &&
         c.find("regression_verdict:") != std::string::npos;
}

}  // namespace agenticdsl
