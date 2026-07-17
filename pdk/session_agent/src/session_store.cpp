// pdk/session_agent/src/session_store.cpp
// Session Store 实现 - JSONL 持久化 + 内存索引
// 关联: docs/adr/adr-0033-session-hierarchy.md

#include "session_agent.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;
namespace pdk_session_agent {

namespace {

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string random_id() {
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream oss;
    oss << "sess_" << std::hex << dis(gen) << dis(gen);
    return oss.str();
}

}  // namespace

SessionStore& SessionStore::instance() {
    static SessionStore inst;
    return inst;
}

SessionStore::SessionStore() {
    // 默认持久化目录
    const char* env = std::getenv("HYDRAFORGE_SESSION_DIR");
    if (env) {
        persist_dir_ = env;
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            persist_dir_ = fs::path(home) / ".hydraforge" / "sessions";
        } else {
            persist_dir_ = "/tmp/.hydraforge/sessions";
        }
    }
}

void SessionStore::set_persist_dir(const fs::path& dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    persist_dir_ = dir;
}

fs::path SessionStore::file_path(const std::string& session_id) const {
    return persist_dir_ / (session_id + ".jsonl");
}

Session& SessionStore::get_or_create(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        // 尝试从磁盘加载
        fs::path path = file_path(session_id);
        if (fs::exists(path)) {
            Session s;
            s.session_id = session_id;
            std::ifstream f(path);
            std::string line;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                try {
                    auto j = nlohmann::json::parse(line);
                    SessionMessage m;
                    m.role = j.value("role", "");
                    m.content = j.value("content", "");
                    m.timestamp_ms = j.value("timestamp_ms", 0LL);
                    if (j.contains("meta")) m.meta = j["meta"];
                    s.messages.push_back(std::move(m));
                } catch (...) {}
            }
            s.updated_at_ms = now_ms();
            if (!s.messages.empty()) s.created_at_ms = s.messages.front().timestamp_ms;
            sessions_[session_id] = std::move(s);
            it = sessions_.find(session_id);
        } else {
            Session s;
            s.session_id = session_id;
            s.created_at_ms = now_ms();
            s.updated_at_ms = s.created_at_ms;
            sessions_[session_id] = std::move(s);
            it = sessions_.find(session_id);
        }
    }
    return it->second;
}

bool SessionStore::exists(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessions_.find(session_id) != sessions_.end()) return true;
    return fs::exists(file_path(session_id));
}

std::vector<std::string> SessionStore::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    for (const auto& [id, _] : sessions_) ids.push_back(id);
    if (fs::exists(persist_dir_)) {
        for (const auto& entry : fs::directory_iterator(persist_dir_)) {
            if (entry.path().extension() == ".jsonl") {
                std::string id = entry.path().stem().string();
                if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
                    ids.push_back(id);
                }
            }
        }
    }
    return ids;
}

bool SessionStore::load(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    fs::path path = file_path(session_id);
    if (!fs::exists(path)) return false;

    Session s;
    s.session_id = session_id;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            SessionMessage m;
            m.role = j.value("role", "");
            m.content = j.value("content", "");
            m.timestamp_ms = j.value("timestamp_ms", 0LL);
            if (j.contains("meta")) m.meta = j["meta"];
            s.messages.push_back(std::move(m));
        } catch (...) {}
    }
    if (!s.messages.empty()) s.created_at_ms = s.messages.front().timestamp_ms;
    s.updated_at_ms = now_ms();
    sessions_[session_id] = std::move(s);
    dirty_.erase(session_id);
    return true;
}

bool SessionStore::persist(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return false;

    const auto& s = it->second;
    fs::create_directories(persist_dir_);

    fs::path path = file_path(session_id);
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return false;

    for (const auto& m : s.messages) {
        nlohmann::json j;
        j["role"] = m.role;
        j["content"] = m.content;
        j["timestamp_ms"] = m.timestamp_ms;
        if (!m.meta.is_null()) j["meta"] = m.meta;
        f << j.dump() << "\n";
    }
    dirty_.erase(session_id);
    return true;
}

void SessionStore::persist_all() {
    std::unique_lock<std::mutex> lock(mutex_);
    std::vector<std::string> to_persist;
    for (const auto& [id, _] : dirty_) to_persist.push_back(id);
    lock.unlock();
    for (const auto& id : to_persist) persist(id);
}

bool SessionStore::remove(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session_id);
    dirty_.erase(session_id);
    fs::path path = file_path(session_id);
    if (fs::exists(path)) {
        return fs::remove(path);
    }
    return true;
}

std::string SessionStore::branch(
    const std::string& src_session_id,
    size_t message_index
) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(src_session_id);
    if (it == sessions_.end()) return "";
    const auto& src = it->second;
    if (message_index >= src.messages.size()) return "";

    std::string new_id = random_id();
    Session s;
    s.session_id = new_id;
    s.messages.assign(src.messages.begin(), src.messages.begin() + message_index + 1);
    s.created_at_ms = now_ms();
    s.updated_at_ms = s.created_at_ms;
    sessions_[new_id] = std::move(s);
    dirty_[new_id] = true;
    return new_id;
}

Session SessionStore::compact(const std::string& session_id, size_t keep_recent) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return {};

    Session copy = it->second;
    if (copy.messages.size() <= keep_recent) return copy;

    // 用 placeholder 替换早期消息
    SessionMessage placeholder;
    placeholder.role = "system";
    placeholder.content = "[compacted: " +
                          std::to_string(copy.messages.size() - keep_recent) +
                          " earlier messages]";
    placeholder.timestamp_ms = now_ms();

    std::vector<SessionMessage> compacted;
    compacted.push_back(placeholder);
    compacted.insert(
        compacted.end(),
        copy.messages.end() - keep_recent,
        copy.messages.end()
    );
    copy.messages = std::move(compacted);
    copy.updated_at_ms = now_ms();
    return copy;
}

std::vector<size_t> SessionStore::search(
    const std::string& session_id,
    const std::string& query
) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<size_t> hits;
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return hits;
    for (size_t i = 0; i < it->second.messages.size(); ++i) {
        if (it->second.messages[i].content.find(query) != std::string::npos) {
            hits.push_back(i);
        }
    }
    return hits;
}

}  // namespace pdk_session_agent