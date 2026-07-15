// pdk/g3_knowledge_base/src/g3_state.h
// 功能描述：G3 SessionStore — 多轮会话状态管理
//          使用 std::shared_mutex 保护并发访问 (ADR-0020 逻辑隔离警告)
//          支持 get_or_create / append / build_context 操作
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §2.4.1 (R2 风险缓解)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#pragma once

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace agenticdsl::pdk::g3 {

struct SessionState {
  /// Q/A 历史对 (问题, 答案)
  std::vector<std::pair<std::string, std::string>> history;
};

/// 硬编码知识片段 (3-5 条, 不依赖外部向量数据库)
inline const std::vector<std::string>& g3_snippets() {
  static const std::vector<std::string> s = {
    "HydraForge is an AgenticDSL execution engine that processes Markdown DSL workflow graphs (DAG).",
    "The PDK (Plugin Development Kit) allows registering tools via IToolRegistry::register_tool_function().",
    "MockLLMProvider is a single-threaded test stub that supports queue-mode and fixed-response modes.",
    "SessionStore uses std::shared_mutex for concurrent read/write access to session state.",
    "G3 Knowledge Base Plugin provides multi-turn Q&A with hardcoded retrieval and LLM generation.",
  };
  return s;
}

/// SessionStore: 线程安全的多轮会话状态容器
/// 读写锁策略: get_or_create 用写锁, build_context 用读锁
class SessionStore {
public:
  /// 查找或创建 session, 返回引用 (写锁: 可能插入新条目)
  SessionState& get_or_create(const std::string& id) {
    std::unique_lock lock(mutex_);
    return sessions_[id];
  }

  /// 追加 Q/A 对到 session 历史 (写锁: 修改已有条目)
  void append(const std::string& id,
              const std::string& question,
              const std::string& answer) {
    std::unique_lock lock(mutex_);
    sessions_[id].history.emplace_back(question, answer);
  }

  /// 构建 LLM 上下文: snippets + 会话历史 (读锁: 只读已有条目)
  std::string build_context(const std::string& id) const {
    std::shared_lock lock(mutex_);
    std::string ctx;
    // 1) 知识片段
    for (const auto& s : g3_snippets())
      ctx += "KNOWLEDGE: " + s + "\n";
    // 2) 会话历史
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
      for (const auto& [q, a] : it->second.history) {
        ctx += "Q: " + q + "\nA: " + a + "\n";
      }
    }
    return ctx;
  }

  /// 获取会话总数 (§6.3 escalation trigger: >1K → flag)
  size_t size() const {
    std::shared_lock lock(mutex_);
    return sessions_.size();
  }

  /// 清空所有会话 (测试用)
  void clear() {
    std::unique_lock lock(mutex_);
    sessions_.clear();
  }

private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, SessionState> sessions_;
};

} // namespace agenticdsl::pdk::g3