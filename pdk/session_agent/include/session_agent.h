// pdk/session_agent/include/session_agent.h
// Session Store - 多轮会话持久化与分支
// 关联: docs/adr/adr-0033-session-hierarchy.md

#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace pdk_session_agent {

// 一条消息
struct SessionMessage {
    std::string role;       // user | assistant | system | tool
    std::string content;
    long long timestamp_ms = 0;
    nlohmann::json meta;     // 额外元数据 (tokens, steps, etc.)
};

// 一个 Session 的完整状态
struct Session {
    std::string session_id;
    std::vector<SessionMessage> messages;
    std::map<std::string, std::string> meta;  // 任意元数据
    long long created_at_ms = 0;
    long long updated_at_ms = 0;
    size_t persisted_count_ = 0;  // 已落盘游标（ADR-0079 v1.1 append-only fix）
};

// 全局 SessionStore（线程安全）
class SessionStore {
public:
    static SessionStore& instance();

    // 显式设置持久化目录（默认 ~/.hydraforge/sessions/）
    void set_persist_dir(const std::filesystem::path& dir);

    // 获取/创建 session
    Session& get_or_create(const std::string& session_id);

    // 检查是否存在
    bool exists(const std::string& session_id) const;

    // 列出所有 session_id
    std::vector<std::string> list() const;

    // 从磁盘加载
    bool load(const std::string& session_id);

    // 持久化到磁盘（JSONL 格式）
    bool persist(const std::string& session_id);

    // 持久化所有 dirty sessions
    void persist_all();

    // 删除 session
    bool remove(const std::string& session_id);

    // Branch: 从指定 message index fork 出新 session
    std::string branch(const std::string& src_session_id, size_t message_index);

    // Compact: 用 placeholder 替换早期消息（保留最近 N 条）
    Session compact(const std::string& session_id, size_t keep_recent = 10);

    // Search: 关键词搜索消息内容
    std::vector<size_t> search(const std::string& session_id, const std::string& query);

private:
    SessionStore();

    std::filesystem::path file_path(const std::string& session_id) const;

    mutable std::mutex mutex_;
    std::map<std::string, Session> sessions_;
    std::map<std::string, bool> dirty_;       // 需要持久化
    std::filesystem::path persist_dir_;
};

}  // namespace pdk_session_agent