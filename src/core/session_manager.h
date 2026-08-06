// src/core/session_manager.h
// 功能描述：SessionManager — JSONL 树状会话存储核心组件
//           替代 chat_session.cpp 线性 JSON 单文件原子写,
//           支持 open/fork/branch/compact API + 叶子到根上下文重建,
//           每行 fsync 保证崩溃安全 (Decision 1/2 in design.md)
// 设计依据：OpenSpec change session-manager-jsonl
//          + ADR-0033 三层 Session 执行模型 (UserSession/TaskSession/SubtaskSession)
//          + ADR-0068 session.persisted 生命周期事件 (Task 7 引入)
// 关键决策 (来自 design.md):
//   - JSONL append-only: 每条记录一行, 末尾换行, 崩溃时丢弃不完整最后一行
//   - parent 指针 + branch 元数据: 字段直接嵌入每条记录 (避免独立索引)
//   - 线程安全: std::mutex write_mutex_ 保护 flush_append (line-level atomic)
//   - PIMPL-lite: dir_/mutex_/counters_ 通过前向声明 + 简单值成员, 析构 out-of-line
// 实施状态：Task 1 (JSONL record types + append-only writer),
//          后续 Task 2-9 逐步实现 open/load_jsonl/fork/branch/compact/build_context_entries
//          + session.persisted 事件发射
// 作者：AgenticDSL Phase 5 / Session Manager JSONL Sprint
// 最后修改日期：2026-08-05

#ifndef AGENTICDSL_CORE_SESSION_MANAGER_H
#define AGENTICDSL_CORE_SESSION_MANAGER_H

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nlohmann/json.hpp"

namespace agenticdsl {

/**
 * @brief SessionNode — JSONL 单条记录 (Decision 2 in design.md)
 *
 * 每条记录自描述, append-only 友好:
 *   - id:        节点唯一标识 (next_node_id() 生成)
 *   - parent_id: 父节点 id; 空字符串表示根
 *   - branch_id: 当前分支标识 (Decision 2: 嵌入每条记录, 避免单独索引)
 *   - content:   业务载荷 (nlohmann::json — message / tool result / metadata 等)
 */
struct SessionNode {
  std::string id;
  std::string parent_id;
  std::string branch_id;
  nlohmann::json content;

  /// 序列化为单行 JSON (不含换行符, 由 writer 负责添加 "\n")
  nlohmann::json to_json() const {
    return nlohmann::json{
        {"id", id},
        {"parent_id", parent_id},
        {"branch_id", branch_id},
        {"content", content}};
  }
};

/**
 * @brief BranchMeta — 分支元数据 (Decision 2 in design.md)
 *
 * 描述一次 fork 的元信息; 由 SessionManager::fork() 写入 JSONL
 * (序列化为 {"type":"branch", ...} meta record, 与 SessionNode 共享文件)
 *
 *  - branch_id:       分支唯一标识 (next_branch_id() 生成)
 *  - name:            用户友好的分支名 (e.g. "explore")
 *  - forked_from_node: fork 时的父节点 id
 *  - created_at:      ISO 8601 时间戳字符串
 */
struct BranchMeta {
  std::string branch_id;
  std::string name;
  std::string forked_from_node;
  std::string created_at;

  /// 序列化为 type=branch 的 JSONL meta 记录
  nlohmann::json to_json() const {
    return nlohmann::json{
        {"type", "branch"},
        {"branch_id", branch_id},
        {"name", name},
        {"forked_from_node", forked_from_node},
        {"created_at", created_at}};
  }
};

/**
 * @brief SessionHandle — 打开的 session 句柄 (Task 2 扩展, Task 1 仅占位)
 *
 * Task 1 仅声明最小字段, Task 2 (open/load_jsonl) 补全 in-memory node index
 * + branch index + leaf pointer。保留 forward-declared style 是为了不让
 * SessionManager::open 返回值在 Task 1 即固定为单一对象, 给后续设计留余地。
 */
struct SessionHandle {
  std::string session_id;
  std::filesystem::path jsonl_path;
};

/// @brief 全局 session JSONL 文件后缀 (.jsonl append-only 文件)
inline constexpr const char* kSessionFileExt = ".jsonl";

/**
 * @brief SessionManager — JSONL 树状会话存储核心组件
 *
 * Task 1 提供的最小 API:
 *   - 构造: SessionManager(dir) — dir 为 session 文件存储根目录
 *   - open(session_id): 创建/打开 <dir>/<session_id>.jsonl 文件, 空文件即视为新 session
 *   - flush_append(node): 追加单条 SessionNode 到当前 session 文件, 末尾 "\n"
 *   - next_node_id(): 生成唯一 node_id ("node_<hex_counter>")
 *   - next_branch_id(): 生成唯一 branch_id ("branch_<hex_counter>")
 *
 * 线程安全: 所有公开方法由 write_mutex_ 保护, 调用方可并发调用 flush_append
 *           而无需额外同步。
 *
 * 崩溃安全: flush_append 在 write() 之后调用 ::fsync(fd) 强制刷盘, 然后 ::close()。
 *           若进程在 write 与 fsync 之间崩溃, 该行被丢弃 (下次读取时检测到非 '\n'
 *           结尾即可跳过)。单行内不会出现部分内容 (write < PIPE_BUF 保证原子写)。
 *
 * 锁顺序约定 (CP.22):
 *   - 仅持有一把 write_mutex_, 无嵌套获取, 无死锁风险
 *   - 持锁期间 MUST NOT 调用外部回调 (避免持锁递归)
 */
class SessionManager {
 public:
  /// @brief 构造 — dir 为 session 文件存储根目录, 自动 create_directories
  explicit SessionManager(std::filesystem::path dir);

  /// @brief 析构 — 关闭当前打开的文件 (如有) — 头文件外定义 (PIMPL-lite)
  ~SessionManager();

  // 禁止拷贝/移动 (mutex + 文件句柄不可复制)
  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;
  SessionManager(SessionManager&&) = delete;
  SessionManager& operator=(SessionManager&&) = delete;

  /**
   * @brief 创建或打开指定 session
   * @param session_id session 标识 (作为 <dir>/<session_id>.jsonl 文件名)
   * @param legacy_path 可选 — 旧版线性 JSON 文件路径; 当 JSONL 文件不存在且
   *                    legacy_path 已提供时, 自动调用 migrate_legacy_json
   *                    从 legacy_path 迁移内容到新建的 JSONL 文件
   *                    (Task 6 完整迁移逻辑已实现, 本参数仅是 open-time 钩子)
   * @return SessionHandle — 描述打开的 session
   *
   * 行为:
   *   1. 若 <dir> 不存在, std::filesystem::create_directories(dir)
   *   2. 若 <dir>/<session_id>.jsonl 不存在, 以 O_WRONLY|O_CREAT 模式 touch 空文件
   *   3. 若文件不存在 且 legacy_path 已提供 且 legacy_path 文件存在 → 调 migrate_legacy_json
   *   4. 记录 current_session_id_ + current_path_ 供后续 flush_append 使用
   *
   * 线程安全: 与 flush_append 共用 write_mutex_, 避免 open 与并发 append 冲突
   */
  SessionHandle open(const std::string& session_id,
                     std::optional<std::string> legacy_path = std::nullopt);

  /**
   * @brief 追加单条 SessionNode 到当前 session JSONL 文件
   * @param node 要追加的节点 (id 必须由 next_node_id() 生成)
   * @throw std::runtime_error 当前未打开任何 session (open 未调用)
   *
   * 行为:
   *   1. 加锁 write_mutex_
   *   2. ::open() 文件以 O_WRONLY|O_APPEND (若 O_CREAT 也设置, 兼容空文件)
   *   3. node.to_json().dump() 序列化 (无换行)
   *   4. 追加 "\n" 末尾
   *   5. ::write() 单次写入 (PIPE_BUF <= 4K 保证原子性)
   *   6. ::fsync() 强制刷盘
   *   7. ::close() 关闭 fd
   *
   * 注: 单次 ::write 写入长度 < PIPE_BUF (4096 on Linux) 保证原子性,
   *     即使多个进程同时 O_APPEND 追加也不会交叉。
   */
  void flush_append(const SessionNode& node);

  /// @brief 生成唯一 node_id — "node_<hex_counter>", 跨进程单调递增
  std::string next_node_id();

  /// @brief 生成唯一 branch_id — "branch_<hex_counter>", 跨进程单调递增
  std::string next_branch_id();

  // ==================== Task 2: load_jsonl ====================
  /// @brief 从 JSONL 文件加载所有记录到内存索引
  /// 跳过不完整最后一行 (崩溃安全: 检测 `line.back() != '\n'`)
  /// 解析每行为 SessionNode (type=branch 行转为 BranchMeta)
  /// 非 const: 填充 nodes_/branches_/children_ 索引
  std::vector<SessionNode> load_jsonl();

  /// @brief 从 in-memory 索引查找节点
  /// @return 找到返回节点指针, 否则 nullptr
  const SessionNode* find_node(const std::string& id) const;

  /// @brief 从 in-memory 索引获取所有分支
  std::vector<BranchMeta> list_branches() const;

  /// @brief 从 in-memory 索引获取所有节点 (O(N))
  std::vector<SessionNode> list_all_nodes() const;

  /// @brief 短前缀匹配节点 (8 字符), 歧义返回 nullopt
  std::optional<SessionNode> get_node_by_short_id(const std::string& short_id) const;

  /// @brief 返回分支元信息 + 该分支最新 leaf 节点
  std::optional<std::pair<BranchMeta, SessionNode>> get_branch_leaf_node(
      const std::string& branch_id) const;

  // ==================== Task 3: fork / switch_branch / append_to_branch ====================
  /// @brief 从指定节点 fork 新分支
  /// @param node_id fork 起始节点 (必须存在于索引)
  /// @param name 分支名 (e.g. "explore")
  /// @return 新分支的 branch_id
  /// @throw std::runtime_error 节点不存在时
  std::string fork(const std::string& node_id, const std::string& name);

  /// @brief 切换当前分支
  /// @throw std::runtime_error branch_id 不存在
  void switch_branch(const std::string& branch_id);

  /// @brief 追加节点到当前分支
  /// @return 生成的 node_id
  /// @throw std::runtime_error 无当前分支或 ID 生成失败
  std::string append_to_branch(const std::string& message);

  /// @brief 获取当前分支
  std::string current_branch() const;

  // ==================== Task 4: compact ====================
  /// @brief 压缩: 仅保留当前活跃分支链, 其余分支移除
  /// 步骤: 1) 复制原文件到 .backup; 2) 重写 JSONL 仅含活跃分支记录
  /// (atomic: 临时文件 + rename)
  void compact();

  // ==================== Task 5: build_context_entries ====================
  /// @brief 叶到根上下文重建: 从 leaf_node_id 沿 parent_id 链收集到根
  /// @return root-first 顺序的节点列表
  /// 防御: visited-set 防止循环 parent 引用导致死循环
  std::vector<SessionNode> build_context_entries(const std::string& leaf_node_id) const;

  /// @brief 获取分支的叶子节点 (该分支无子节点的节点)
  /// @return 叶子节点 ID, 空字符串表示分支为空
  std::string get_branch_leaf(const std::string& branch_id) const;

  /// @brief 获取根节点 (parent_id 为空的节点)
  std::string get_root_node() const;

  // ==================== Task 6: legacy migration ====================
  /// @brief 从旧版线性 JSON 文件迁移到 JSONL
  /// @param legacy_path 旧版 .json 文件路径
  /// @return 迁移用的 session_id
  /// 行为: 读取 {messages: [...]} 数组, 逐条创建 SessionNode (parent chain),
  ///       复制原文件到 <legacy_path>.backup
  std::string migrate_legacy_json(const std::filesystem::path& legacy_path);

  // ==================== Task 7: session.persisted event ====================
  /// @brief 注入事件总线 (flush_append 成功后发射 session.persisted)
  /// @param bus IInteractionBus shared_ptr (nullptr = 禁用事件发射)
  void set_bus(std::shared_ptr<class IInteractionBus> bus);

 private:
  // flush_append 内部版本 (用于 branch meta 写入, 不触发 session.persisted 事件)
  void flush_append_internal(const BranchMeta& bm);

  // 公共成员:
 public:
  std::filesystem::path dir_;          // session 文件根目录
  std::string current_session_id_;      // open() 设置的当前 session
  std::filesystem::path current_path_;  // 当前 session 的 .jsonl 完整路径
  std::atomic<uint64_t> node_counter_{0};
  std::atomic<uint64_t> branch_counter_{0};
  mutable std::mutex write_mutex_;      // 保护 flush_append (line-level atomic)

  // Task 2-5: in-memory 索引 (load_jsonl 填充)
  mutable std::mutex index_mutex_;      // 保护 index 读写
  std::unordered_map<std::string, SessionNode> nodes_;  // id → node
  std::unordered_map<std::string, BranchMeta> branches_;  // branch_id → meta
  std::string current_branch_;          // switch_branch 设置, 默认 "main"
  // children: parent_id → set of child node_ids (用于 build_context_entries + branch_leaf)
  std::unordered_map<std::string, std::unordered_set<std::string>> children_;

  // Task 7: 事件总线 (flush_append 成功后发射 session.persisted)
  std::shared_ptr<class IInteractionBus> bus_;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_CORE_SESSION_MANAGER_H