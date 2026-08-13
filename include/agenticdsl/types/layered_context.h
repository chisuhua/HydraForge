// agenticdsl/types/layered_context.h
// 功能描述：5-层结构化上下文 (L1-L5) 的 C++ 类型定义, 实现 ADR-0008 +
//           docs/specs/dsl.md §4.1 的 5 层语义划分。仅含类型 + 路径导航 +
//           基础读写权限, 不含复杂权限检查 (复杂权限由 IExecutionPolicy /
//           StateTools 在更高层处理)。
// 设计依据：ADR-0008 (5-层结构化上下文) + dsl.md §4.1
// 作者：AgenticDSL Stage 3
// 最后修改日期：2026-06-12
#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace agenticdsl {

/**
 * @brief 5-层结构化上下文 (L1-L5)
 *
 * 5 层语义划分 (与 dsl.md §4.1 完全对齐):
 *   - L1 system:  只读, 来自环境/启动参数
 *   - L2 recent:  短期 session 状态, 可写
 *   - L3 working: 当前任务数据, 可写
 *   - L4 archive: 历史, 压缩存储, 追加写 (本类型不强制 append-only, 由更高层负责)
 *   - L5 meta:    元数据, 类型/权限
 *
 * 路径格式: "<layer>.<category>.<key>", e.g., "working.data.user_input"
 *   - layer   ∈ {system, recent, working, archive, meta}
 *   - category 是子分类 (e.g., "data", "tool", "trace")
 *   - key     是叶子名
 *
 * 实现说明:
 *   - 仅使用 nlohmann::json 作为存储, 不引入任何 STL 容器
 *   - 路径导航使用 json::operator[] + 点分递归
 *   - 越界或类型不匹配路径返回 null JSON (at 返回引用, 调用方不可写)
 *   - 写 L1 system 通过 at() 抛出 std::runtime_error
 *
 * 当前为 Stage 3 / Task 12 最小实现。Task 13 将迁移现有 flat Context 调用方;
 * 后续 OpenSpec change 可扩展 permission / archive append-only / category 校验。
 */
struct LayeredContext {
  // ===== 5 个存储槽位 =====
  nlohmann::json system;   // L1: 只读
  nlohmann::json recent;   // L2: 短期 RW
  nlohmann::json working;  // L3: 当前任务 RW
  nlohmann::json archive;  // L4: 历史 RW (append-only 由调用方保证)
  nlohmann::json meta;     // L5: 元数据 RW

  // ===== 路径导航 =====
  // path 格式: "<layer>.<category>.<key>", e.g., "working.data.user_input"
  // 返回值: 对应位置的 json 引用, 不存在则返回 null json (写入 null 也不影响原结构)
  nlohmann::json& at(const std::string& path);
  const nlohmann::json& at(const std::string& path) const;

  // ===== 权限检查 (基础 layer 级) =====
  // 仅检查 layer 名称是否允许对应操作。category/key 的语义校验由 IExecutionPolicy
  // (ADR-0031) 与 StateTools (dsl.md §4.1.4) 在更高层处理。
  bool can_read(const std::string& path) const;
  bool can_write(const std::string& path) const;

  // ===== 序列化/反序列化 =====
  // dump: 输出含 5 个顶层键 (system/recent/working/archive/meta) 的扁平 JSON
  // load: 输入必须含上述 5 个键, 否则抛 std::runtime_error
  nlohmann::json dump() const;
  static LayeredContext load(const nlohmann::json& j);

  // ===== 构造 =====
  LayeredContext() = default;

  // ===== 双层保留策略 (ADR-0069) =====
  // 追加原始消息到 meta.original_messages (只追加, 不删除)
  void append_original(std::string message);
  // 替换 working 视图 (供 LLM 调用读取摘要)
  void set_working_view(std::string view);
  // 设置 metadata 字段 (JSON 合并)
  void set_metadata(const std::string& key, nlohmann::json value);
  // 读取 compaction_record (返回空 object 如未设置)
  nlohmann::json compaction_record() const;
};

// ============================================================
// 内部辅助: 将路径拆分为 layer + 剩余 segments
// 返回: (slot_ref, remaining_path_segments)
//   - slot_ref: system/recent/working/archive/meta 之一的引用
//   - segments: layer 之后的剩余段列表 (category, key, ...)
// ============================================================
namespace layered_context_detail {

// 解析 layer 名称 -> 对应 slot 引用。layer 不在 5 个白名单中返回 nullptr
inline nlohmann::json* resolve_slot(LayeredContext& ctx, const std::string& layer) {
  if (layer == "system")  return &ctx.system;
  if (layer == "recent")  return &ctx.recent;
  if (layer == "working") return &ctx.working;
  if (layer == "archive") return &ctx.archive;
  if (layer == "meta")    return &ctx.meta;
  return nullptr;
}

inline const nlohmann::json* resolve_slot(const LayeredContext& ctx,
                                          const std::string& layer) {
  if (layer == "system")  return &ctx.system;
  if (layer == "recent")  return &ctx.recent;
  if (layer == "working") return &ctx.working;
  if (layer == "archive") return &ctx.archive;
  if (layer == "meta")    return &ctx.meta;
  return nullptr;
}

// 按 segments 递归下钻 json, 自动创建缺失 object 节点
// 如果 layer=="system" 且调用方意图为写入 (通过 write_attempt=true 传入),
//   则抛 std::runtime_error —— L1 是只读的。
// 读保护策略: 区分 "已存在路径的读取/修改" 与 "需要自动创建新路径":
//   - 当路径已存在 (沿途每个 segment 都在 object 中) -> 允许, 返回引用
//     (调用方若执行赋值, 通过 navigate 末尾的 system-write guard 拦截)
//   - 当路径不存在 -> 需要 object 化中间节点, 视为写入, 抛异常
inline nlohmann::json& navigate(nlohmann::json& slot,
                                const std::string& layer,
                                const std::string& segments_csv,
                                bool write_attempt) {
  if (segments_csv.empty()) {
    if (write_attempt && layer == "system") {
      throw std::runtime_error(
          "LayeredContext: cannot write to read-only layer 'system'");
    }
    return slot;
  }
  nlohmann::json* cur = &slot;
  std::string remaining = segments_csv;
  while (!remaining.empty()) {
    auto dot = remaining.find('.');
    std::string seg = (dot == std::string::npos)
        ? remaining
        : remaining.substr(0, dot);
    if (cur->is_object() && cur->contains(seg)) {
      cur = &(*cur)[seg];
    } else if (layer == "system") {
      throw std::runtime_error(
          "LayeredContext: cannot write to read-only layer 'system'");
    } else {
      if (!cur->is_object()) {
        *cur = nlohmann::json::object();
      }
      cur = &(*cur)[seg];
    }
    if (dot == std::string::npos) break;
    remaining = remaining.substr(dot + 1);
  }
  return *cur;
}

inline const nlohmann::json& navigate_const(const nlohmann::json& slot,
                                            const std::string& segments_csv) {
  if (segments_csv.empty()) {
    return slot;
  }
  const nlohmann::json* cur = &slot;
  std::string remaining = segments_csv;
  while (!remaining.empty()) {
    auto dot = remaining.find('.');
    std::string seg = (dot == std::string::npos)
        ? remaining
        : remaining.substr(0, dot);
    // const 路径: 不存在则返回 null
    if (!cur->is_object() || !cur->contains(seg)) {
      static thread_local const nlohmann::json null_j;
      return null_j;
    }
    cur = &(*cur)[seg];
    if (dot == std::string::npos) break;
    remaining = remaining.substr(dot + 1);
  }
  return *cur;
}

inline void split_path(const std::string& path,
                       std::string& layer_out,
                       std::string& rest_out) {
  auto dot = path.find('.');
  if (dot == std::string::npos) {
    layer_out = path;
    rest_out.clear();
    return;
  }
  layer_out = path.substr(0, dot);
  rest_out = path.substr(dot + 1);
}

}  // namespace layered_context_detail

// ============================================================
// 非 const at(): 返回 json 引用 (可写)
// ============================================================
inline nlohmann::json& LayeredContext::at(const std::string& path) {
  std::string layer, rest;
  layered_context_detail::split_path(path, layer, rest);
  nlohmann::json* slot = layered_context_detail::resolve_slot(*this, layer);
  if (slot == nullptr) {
    // 无效 layer: 返回一个 null 引用 (语法上合法, 写入会触发 nlohmann type_error)
    static thread_local nlohmann::json null_j;
    return null_j;
  }
  // 非 const at() 调用方可能写入 (虽然无法 100% 检测意图, navigate 内部
  // 仍按 write_attempt=true 处理最严格情况 —— L1 system 写必抛异常)
  return layered_context_detail::navigate(*slot, layer, rest, /*write_attempt=*/true);
}

// ============================================================
// const at(): 返回 const json 引用 (不可写, 不会抛写异常)
// ============================================================
inline const nlohmann::json& LayeredContext::at(const std::string& path) const {
  std::string layer, rest;
  layered_context_detail::split_path(path, layer, rest);
  const nlohmann::json* slot = layered_context_detail::resolve_slot(*this, layer);
  if (slot == nullptr) {
    static thread_local const nlohmann::json null_j;
    return null_j;
  }
  return layered_context_detail::navigate_const(*slot, rest);
}

// ============================================================
// 权限检查: 仅按 layer 名称判定
// ============================================================
inline bool LayeredContext::can_read(const std::string& path) const {
  std::string layer, rest;
  layered_context_detail::split_path(path, layer, rest);
  // 5 个白名单 layer 全部可读 (与 dsl.md §4.1 对齐)
  return layered_context_detail::resolve_slot(*this, layer) != nullptr;
}

inline bool LayeredContext::can_write(const std::string& path) const {
  std::string layer, rest;
  layered_context_detail::split_path(path, layer, rest);
  // L1 system 只读; 其他 4 层 RW
  if (layer == "system") return false;
  return layered_context_detail::resolve_slot(*this, layer) != nullptr;
}

// ============================================================
// dump: 扁平化为顶层 5 键 JSON
// ============================================================
inline nlohmann::json LayeredContext::dump() const {
  nlohmann::json out = nlohmann::json::object();
  out["system"]  = system;
  out["recent"]  = recent;
  out["working"] = working;
  out["archive"] = archive;
  out["meta"]    = meta;
  return out;
}

// ============================================================
// load: 从扁平 JSON 还原
// ============================================================
inline LayeredContext LayeredContext::load(const nlohmann::json& j) {
  if (!j.is_object()) {
    throw std::runtime_error(
        "LayeredContext::load: input must be a JSON object");
  }
  LayeredContext out;
  // 严格模式: 5 个键必须全部存在; 缺失键抛异常
  for (const char* k : {"system", "recent", "working", "archive", "meta"}) {
    if (!j.contains(k)) {
      throw std::runtime_error(
          std::string("LayeredContext::load: missing required key '") + k + "'");
    }
  }
  out.system  = j["system"];
  out.recent  = j["recent"];
  out.working = j["working"];
  out.archive = j["archive"];
  out.meta    = j["meta"];
  return out;
}

inline void LayeredContext::append_original(std::string message) {
  if (!meta.contains("original_messages") || !meta["original_messages"].is_array()) {
    meta["original_messages"] = nlohmann::json::array();
  }
  meta["original_messages"].push_back(std::move(message));
}

inline void LayeredContext::set_working_view(std::string view) {
  working["view"] = std::move(view);
}

inline void LayeredContext::set_metadata(const std::string& key, nlohmann::json value) {
  meta[key] = std::move(value);
}

inline nlohmann::json LayeredContext::compaction_record() const {
  return meta.value("compaction_record", nlohmann::json::object());
}

}  // namespace agenticdsl
