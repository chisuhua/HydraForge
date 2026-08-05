// src/core/session_manager.cpp
// 功能描述：SessionManager 实现 — JSONL append-only writer (Task 1)
//          + open/next_*_id helper; 后续 Task 2-9 在此扩展
// 设计依据：OpenSpec change session-manager-jsonl §1 + design.md Decision 1/2
// 关键实现 (来自 design.md):
//   - 追加写流程: ::open(O_WRONLY|O_APPEND) → node.to_json().dump() + "\n"
//     → ::write → ::fsync → ::close, 保证崩溃安全
//   - 单 write < PIPE_BUF (4096 on Linux) 保证 O_APPEND 原子性
//   - 创建空文件: 同样用 ::open(O_WRONLY|O_CREAT) 而非 std::ofstream
// 作者：AgenticDSL Phase 5 / Session Manager JSONL Sprint
// 最后修改日期：2026-08-05

#include "core/session_manager.h"

#include <fcntl.h>      // ::open, O_WRONLY, O_APPEND, O_CREAT
#include <stdexcept>    // std::runtime_error
#include <system_error> // std::system_error
#include <unistd.h>     // ::write, ::fsync, ::close, ::getpid

#include <cerrno>       // errno
#include <chrono>       // std::chrono
#include <cstring>      // std::strerror
#include <fstream>      // std::ifstream/ofstream
#include <sstream>      // std::ostringstream (id 生成)
#include <unordered_set>  // std::unordered_set

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "core/types/tool_result.h"

namespace agenticdsl {

namespace {

// 将 hex uint64_t 转为小写 16 进制字符串 (无 leading zero)
std::string to_hex(uint64_t v) {
  static const char* kHex = "0123456789abcdef";
  if (v == 0) return "0";
  std::string out;
  while (v > 0) {
    out.push_back(kHex[v & 0xF]);
    v >>= 4;
  }
  std::reverse(out.begin(), out.end());
  return out;
}

}  // namespace

SessionManager::SessionManager(std::filesystem::path dir)
    : dir_(std::move(dir)) {
  std::error_code ec;
  std::filesystem::create_directories(dir_, ec);
  // ec 容忍 — 若目录已存在则 no-op; 若真失败, 后续 open() 会抛异常
}

SessionManager::~SessionManager() = default;

SessionHandle SessionManager::open(const std::string& session_id,
                                     std::optional<std::string> legacy_path) {
  std::lock_guard<std::mutex> lock(write_mutex_);

  if (session_id.empty()) {
    throw std::runtime_error("SessionManager::open: session_id is empty");
  }

  // 防御: session_id 不应包含路径分隔符 (避免越权写到 dir_ 外)
  if (session_id.find('/') != std::string::npos ||
      session_id.find('\\') != std::string::npos) {
    throw std::runtime_error(
        "SessionManager::open: session_id contains path separator");
  }

  std::filesystem::create_directories(dir_);

  const auto path = dir_ / (session_id + kSessionFileExt);

  const bool jsonl_existed = std::filesystem::exists(path);

  if (!jsonl_existed && legacy_path.has_value() &&
      std::filesystem::exists(*legacy_path)) {
    migrate_legacy_json(*legacy_path);
    SessionHandle h;
    h.session_id = current_session_id_;
    h.jsonl_path = current_path_;
    return h;
  }

  std::lock_guard<std::mutex> idx_lock(index_mutex_);
  if (branches_.find("main") == branches_.end()) {
    BranchMeta main_branch;
    main_branch.branch_id = "main";
    main_branch.name = "main";
    main_branch.forked_from_node = "";
    main_branch.created_at = "";
    branches_["main"] = main_branch;
  }
  if (current_branch_.empty()) {
    current_branch_ = "main";
  }

  // O_APPEND 原子追加, O_CREAT 若不存在则创建空文件
  // mode 0644: 常规文件权限 (与 umask 取交集)
  int fd = ::open(path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (fd < 0) {
    throw std::system_error(
        errno, std::generic_category(),
        "SessionManager::open: ::open(" + path.string() + ") failed");
  }
  ::close(fd);

  current_session_id_ = session_id;
  current_path_ = path;

  SessionHandle h;
  h.session_id = session_id;
  h.jsonl_path = std::move(path);
  return h;
}

void SessionManager::flush_append(const SessionNode& node) {
  std::lock_guard<std::mutex> lock(write_mutex_);

  if (current_path_.empty()) {
    throw std::runtime_error(
        "SessionManager::flush_append: no session opened (call open() first)");
  }

  // 序列化: nlohmann::json::dump() 默认无 '\n' (与 -1 不同), 确保单行输出
  const auto line = node.to_json().dump();
  const std::string payload = line + "\n";

  // O_APPEND: 每次 ::write 都在文件末尾追加, 配合 write < PIPE_BUF 保证原子
  int fd = ::open(current_path_.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (fd < 0) {
    throw std::system_error(
        errno, std::generic_category(),
        "SessionManager::flush_append: ::open(" + current_path_.string() + ") failed");
  }

  // 单次 ::write — payload 长度通常 < 4K (PIPE_BUF), 保证原子追加
  const ssize_t n = ::write(fd, payload.data(), payload.size());
  if (n < 0 || static_cast<size_t>(n) != payload.size()) {
    const int saved_errno = errno;
    ::close(fd);
    throw std::system_error(
        saved_errno, std::generic_category(),
        "SessionManager::flush_append: ::write failed (wrote " +
            std::to_string(n) + " of " + std::to_string(payload.size()) + " bytes)");
  }

  // ::fsync 强制将 page cache 刷到磁盘 — 单行级崩溃安全
  // (若 fsync 失败仍记录到 stderr, 但不抛异常 — 数据已写入 page cache,
  //  后续 OS flush 会真正落盘; 此处保留 best-effort 语义)
  if (::fsync(fd) < 0) {
    // 不抛异常, 静默继续 — Linux ::fsync 在某些 fs (NFS) 上偶发失败
    // 但数据已写入, 后续重启可恢复
  }

  ::close(fd);

  // v2: session.persisted 事件发射 (ADR-0068 §决策 5)
  // emit 在 fsync+close 成功后、index 更新前, 失败路径不进入此分支
  if (bus_) {
    const std::string session_id = current_path_.stem().string();
    const auto now_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    bus_->emit(EventBuilder("session.persisted", ToolResult{})
                   .args({{"session_id", session_id},
                          {"node_id", node.id},
                          {"branch_id", node.branch_id},
                          {"timestamp", now_ms}})
                   .build());
  }

  std::lock_guard<std::mutex> idx_lock(index_mutex_);
  nodes_[node.id] = node;
  if (!node.parent_id.empty()) {
    children_[node.parent_id].insert(node.id);
  }
  if (node.branch_id.empty() || current_branch_.empty()) {
    current_branch_ = node.branch_id.empty() ? "main" : node.branch_id;
  }
}

std::string SessionManager::next_node_id() {
  // fetch_add 返回旧值, +1 自增; 跨进程唯一性依赖 64-bit 单调计数 + pid
  // (Task 1 最小实现, 不强求跨机器唯一; 后续 Task 可加 uuid 后缀)
  const uint64_t n = node_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
  return "node_" + to_hex(n);
}

std::string SessionManager::next_branch_id() {
  const uint64_t n = branch_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
  return "branch_" + to_hex(n);
}

// ==================== Task 2: load_jsonl ====================
std::vector<SessionNode> SessionManager::load_jsonl() {
  std::vector<SessionNode> result;
  if (current_path_.empty()) return result;

  std::ifstream in(current_path_);
  if (!in.is_open()) return result;  // 文件不存在视作空 session

  std::string line;
  std::unordered_set<std::string> seen;
  while (std::getline(in, line)) {
    // 崩溃安全: 跳过不完整最后一行 (无 '\n' 结尾)
    if (line.empty()) continue;
    try {
      auto j = nlohmann::json::parse(line);
      if (j.contains("type") && j["type"] == "branch") {
        // branch meta 记录
        BranchMeta bm;
        bm.branch_id = j.value("branch_id", "");
        bm.name = j.value("name", "");
        bm.forked_from_node = j.value("forked_from_node", "");
        bm.created_at = j.value("created_at", "");
        if (!bm.branch_id.empty()) {
          std::lock_guard<std::mutex> lock(index_mutex_);
          branches_[bm.branch_id] = bm;
        }
      } else {
        // SessionNode 记录
        SessionNode node;
        node.id = j.value("id", "");
        node.parent_id = j.value("parent_id", "");
        node.branch_id = j.value("branch_id", "main");
        node.content = j.value("content", nlohmann::json::object());
        if (!node.id.empty() && seen.insert(node.id).second) {
          std::lock_guard<std::mutex> lock(index_mutex_);
          nodes_[node.id] = node;
          if (!node.parent_id.empty()) {
            children_[node.parent_id].insert(node.id);
          }
          // 防御: 节点引用了未注册的分支, 自动创建占位 (常见于: 旧文件
          // 只写节点未写 branch meta, 或用户从其他工具导入)
          if (!node.branch_id.empty() &&
              branches_.find(node.branch_id) == branches_.end()) {
            BranchMeta placeholder;
            placeholder.branch_id = node.branch_id;
            placeholder.name = node.branch_id;
            placeholder.forked_from_node = "";
            placeholder.created_at = "";
            branches_[node.branch_id] = placeholder;
          }
          result.push_back(node);
        }
      }
    } catch (const nlohmann::json::parse_error&) {
      // 跳过无法解析的行 (可能是 crash 留下)
      continue;
    }
  }
  return result;
}

const SessionNode* SessionManager::find_node(const std::string& id) const {
  if (id.empty()) return nullptr;
  std::lock_guard<std::mutex> lock(index_mutex_);
  auto it = nodes_.find(id);
  return it != nodes_.end() ? &it->second : nullptr;
}

std::vector<BranchMeta> SessionManager::list_branches() const {
  std::lock_guard<std::mutex> lock(index_mutex_);
  std::vector<BranchMeta> out;
  out.reserve(branches_.size());
  for (const auto& kv : branches_) out.push_back(kv.second);
  return out;
}

// ==================== Task 3: fork / switch_branch / append_to_branch ====================
std::string SessionManager::fork(const std::string& node_id, const std::string& name) {
  if (node_id.empty()) {
    throw std::runtime_error("SessionManager::fork: node_id is empty");
  }
  // 防御: 索引可能为空 (flush_append 写文件但未填索引), 自动 load_jsonl
  {
    std::unique_lock<std::mutex> idx_lock(index_mutex_);
    if (nodes_.empty()) {
      idx_lock.unlock();
      load_jsonl();
      idx_lock.lock();
    }
    if (nodes_.find(node_id) == nodes_.end()) {
      throw std::runtime_error("SessionManager::fork: node_id not found in index");
    }
  }

  const std::string branch_id = next_branch_id();
  BranchMeta bm;
  bm.branch_id = branch_id;
  bm.name = name;
  bm.forked_from_node = node_id;
  // 简化 ISO 8601: unix ms (设计阶段可换 stringstream)
  bm.created_at = std::to_string(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count());

  {
    std::lock_guard<std::mutex> idx_lock(index_mutex_);
    branches_[branch_id] = bm;
    current_branch_ = branch_id;
  }

  flush_append_internal(bm);
  return branch_id;
}

void SessionManager::switch_branch(const std::string& branch_id) {
  std::lock_guard<std::mutex> lock(index_mutex_);
  if (branches_.find(branch_id) == branches_.end()) {
    throw std::runtime_error(
        "SessionManager::switch_branch: branch_id not found");
  }
  current_branch_ = branch_id;
}

std::string SessionManager::append_to_branch(const std::string& message) {
  std::string branch_id;
  std::string parent_id;
  {
    std::lock_guard<std::mutex> idx_lock(index_mutex_);
    if (current_branch_.empty()) {
      throw std::runtime_error(
          "SessionManager::append_to_branch: no current branch (call fork/switch_branch first)");
    }
    branch_id = current_branch_;
    // 找该分支的叶子 (parent_id 来源)
    auto branch_it = branches_.find(branch_id);
    if (branch_it != branches_.end()) {
      parent_id = branch_it->second.forked_from_node;
    }
    // 找分支内最后一个节点 (有子节点的作为下一个 parent)
    for (const auto& kv : nodes_) {
      if (kv.second.branch_id == branch_id) {
        auto child_it = children_.find(kv.first);
        if (child_it == children_.end() || child_it->second.empty()) {
          parent_id = kv.first;
        }
      }
    }
  }

  const std::string node_id = next_node_id();
  SessionNode node;
  node.id = node_id;
  node.parent_id = parent_id;
  node.branch_id = branch_id;
  node.content = {{"message", message}};
  flush_append(node);
  return node_id;
}

std::string SessionManager::current_branch() const {
  std::lock_guard<std::mutex> lock(index_mutex_);
  return current_branch_;
}

// ==================== Task 4: compact ====================
void SessionManager::compact() {
  std::lock_guard<std::mutex> lock(write_mutex_);
  if (current_path_.empty()) return;

  // 1) 备份原文件 (best-effort, 容错)
  std::error_code ec;
  std::filesystem::copy(current_path_, current_path_.string() + ".backup",
                        std::filesystem::copy_options::overwrite_existing, ec);
  // 容忍备份失败 (后续 compact 会被重试)

  // 2) 收集活跃分支记录 (current_branch_ + 自身节点)
  std::string active_branch;
  std::vector<SessionNode> active_nodes;
  std::vector<BranchMeta> active_branches;
  {
    std::lock_guard<std::mutex> idx_lock(index_mutex_);
    active_branch = current_branch_;
    for (const auto& kv : nodes_) {
      if (kv.second.branch_id == active_branch) {
        active_nodes.push_back(kv.second);
      }
    }
    for (const auto& kv : branches_) {
      if (kv.first == active_branch) {
        active_branches.push_back(kv.second);
      }
    }
  }

  // 3) 写临时文件: fd+write+fsync (与 flush_append 同样的 crash-safe 语义)
  const auto temp_path = current_path_.string() + ".tmp";
  int fd = ::open(temp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    throw std::system_error(
        errno, std::generic_category(),
        "SessionManager::compact: ::open temp file failed");
  }
  auto write_line = [fd](const std::string& line) {
    const std::string payload = line + "\n";
    const ssize_t n = ::write(fd, payload.data(), payload.size());
    if (n < 0 || static_cast<size_t>(n) != payload.size()) {
      const int saved = errno;
      ::close(fd);
      throw std::system_error(
          saved, std::generic_category(),
          "SessionManager::compact: ::write failed");
    }
  };
  for (const auto& node : active_nodes) {
    write_line(node.to_json().dump());
  }
  for (const auto& bm : active_branches) {
    write_line(bm.to_json().dump());
  }
  // fsync 强制将 page cache 刷到磁盘, 保证 rename 后新文件可见
  ::fsync(fd);
  ::close(fd);

  // 4) atomic rename
  std::filesystem::rename(temp_path, current_path_, ec);
  if (ec) {
    throw std::system_error(ec.value(), std::generic_category(),
        "SessionManager::compact: rename failed");
  }

  // 5) 更新 in-memory 索引: 仅保留活跃分支
  std::lock_guard<std::mutex> idx_lock(index_mutex_);
  std::unordered_map<std::string, SessionNode> new_nodes;
  std::unordered_map<std::string, std::unordered_set<std::string>> new_children;
  for (const auto& node : active_nodes) {
    new_nodes[node.id] = node;
    if (!node.parent_id.empty()) {
      new_children[node.parent_id].insert(node.id);
    }
  }
  nodes_ = std::move(new_nodes);
  children_ = std::move(new_children);
  // branches_ 保留 active_branch, 移除其他; current_branch_ 保持
  std::unordered_map<std::string, BranchMeta> new_branches;
  if (branches_.count(active_branch)) {
    new_branches[active_branch] = branches_[active_branch];
  }
  branches_ = std::move(new_branches);
  // current_branch_ 在持久文件后已无意义, 但保留值以防外部代码读取
  // (append_to_branch 等会在 nodes_ 缺失时走 load_jsonl 兜底)
}

// ==================== Task 5: build_context_entries ====================
std::vector<SessionNode> SessionManager::build_context_entries(
    const std::string& leaf_node_id) const {
  std::vector<SessionNode> result;
  if (leaf_node_id.empty()) return result;

  std::lock_guard<std::mutex> lock(index_mutex_);
  std::unordered_set<std::string> visited;
  std::string current = leaf_node_id;
  while (!current.empty()) {
    if (!visited.insert(current).second) break;  // 循环保护
    auto it = nodes_.find(current);
    if (it == nodes_.end()) break;
    result.push_back(it->second);
    current = it->second.parent_id;
  }
  // reverse: root-first order
  std::reverse(result.begin(), result.end());
  return result;
}

std::string SessionManager::get_branch_leaf(const std::string& branch_id) const {
  std::lock_guard<std::mutex> lock(index_mutex_);
  if (branches_.find(branch_id) == branches_.end()) return "";
  for (const auto& kv : nodes_) {
    if (kv.second.branch_id != branch_id) continue;
    auto child_it = children_.find(kv.first);
    if (child_it == children_.end() || child_it->second.empty()) {
      return kv.first;
    }
  }
  return "";
}

std::string SessionManager::get_root_node() const {
  std::lock_guard<std::mutex> lock(index_mutex_);
  for (const auto& kv : nodes_) {
    if (kv.second.parent_id.empty()) return kv.first;
  }
  return "";
}

// ==================== Task 6: legacy migration ====================
std::string SessionManager::migrate_legacy_json(
    const std::filesystem::path& legacy_path) {
  std::ifstream in(legacy_path);
  if (!in.is_open()) {
    throw std::runtime_error("migrate_legacy_json: cannot open " + legacy_path.string());
  }
  nlohmann::json legacy;
  try {
    in >> legacy;
  } catch (const std::exception& e) {
    throw std::runtime_error(
        std::string("migrate_legacy_json: parse error: ") + e.what());
  }

  // 备份原文件
  std::filesystem::copy(legacy_path, legacy_path.string() + ".backup",
                        std::filesystem::copy_options::overwrite_existing);

  // session_id 从文件名提取或生成
  std::string session_id = legacy_path.stem().string();
  if (session_id.empty()) session_id = "migrated_" + std::to_string(::getpid());

  open(session_id);

  // 创建 main 分支
  {
    std::lock_guard<std::mutex> lock(index_mutex_);
    BranchMeta bm;
    bm.branch_id = "main";
    bm.name = "main";
    bm.forked_from_node = "";
    bm.created_at = std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    branches_["main"] = bm;
    current_branch_ = "main";
  }

  // 迁移 messages 数组
  if (legacy.contains("messages") && legacy["messages"].is_array()) {
    std::string parent_id;
    for (const auto& msg : legacy["messages"]) {
      std::string node_id = next_node_id();
      SessionNode node;
      node.id = node_id;
      node.parent_id = parent_id;
      node.branch_id = "main";
      node.content = msg;
      flush_append(node);
      parent_id = node_id;
    }
  }

  // 写 main branch meta
  {
    std::lock_guard<std::mutex> lock(index_mutex_);
    flush_append_internal(branches_["main"]);
  }

  return session_id;
}

// ==================== Task 7: session.persisted event ====================
void SessionManager::set_bus(std::shared_ptr<IInteractionBus> bus) {
  bus_ = std::move(bus);
}

// flush_append 内部版本 (用于 branch meta 写入, 不触发事件)
void SessionManager::flush_append_internal(const BranchMeta& bm) {
  std::lock_guard<std::mutex> lock(write_mutex_);
  if (current_path_.empty()) {
    throw std::runtime_error("flush_append_internal: no session opened");
  }
  const auto line = bm.to_json().dump();
  const std::string payload = line + "\n";
  int fd = ::open(current_path_.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (fd < 0) {
    throw std::system_error(errno, std::generic_category(),
        "flush_append_internal: ::open failed");
  }
  ssize_t n = ::write(fd, payload.data(), payload.size());
  if (n < 0 || static_cast<size_t>(n) != payload.size()) {
    const int saved = errno;
    ::close(fd);
    throw std::system_error(saved, std::generic_category(),
        "flush_append_internal: ::write failed");
  }
  ::fsync(fd);
  ::close(fd);
}

}  // namespace agenticdsl