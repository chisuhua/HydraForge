// tests/test_session_persistence.cpp
// T1 Session 持久化测试: save-restore, corrupted JSON, stale cleanup
// 关联: openspec/changes/pdk-chat-demo-v1-recap/tasks.md §T1

#include "catch_amalgamated.hpp"

#include "chat_session.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <core/types/tool_result.h>

using namespace pdk_chat_demo;

namespace {

class MockBus : public agenticdsl::IInteractionBus {
public:
    void emit(const agenticdsl::BusEvent& event) override {
        events.emplace_back(event.topic, event.payload.meta);
        auto it = subscribers_.find(event.topic);
        if (it != subscribers_.end()) {
            for (auto& cb : it->second) cb(event);
        }
    }

    void emit(const std::string& topic, const std::string& content) override {
        agenticdsl::ToolResult tr;
        tr.ok = true;
        tr.meta = {{"content", content}};
        emit(agenticdsl::BusEvent{topic, tr});
    }

    size_t subscribe(const std::string& topic,
                      std::function<void(const agenticdsl::BusEvent&)> cb) override {
        subscribers_[topic].push_back(std::move(cb));
        return next_token_++;
    }

    void unsubscribe(size_t) override {}

    std::vector<std::pair<std::string, nlohmann::json>> events;

private:
    size_t next_token_ = 1;
    std::unordered_map<std::string, std::vector<std::function<void(const agenticdsl::BusEvent&)>>> subscribers_;
};

// RAII 临时目录 fixture
class TempDir {
public:
    explicit TempDir(const std::string& prefix = "hf_test_") {
        auto t = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() / (prefix + std::to_string(t));
        std::filesystem::create_directories(path_);
        path_str_ = path_.string();
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::string& str() const { return path_str_; }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    std::string path_str_;
};

void write_file(const std::filesystem::path& p, const std::string& content) {
    std::ofstream f(p);
    f << content;
}

}  // namespace

TEST_CASE("session persistence: save and restore across processes", "[session][persistence]") {
    TempDir tmp;
    auto bus = std::make_shared<MockBus>();

    AgentConfig agent;
    agent.provider = "mock";
    SessionConfig sess;
    sess.persist_dir = tmp.str();

    // 进程 1: 保存 session
    std::string saved_id;
    {
        ChatSession s1(nullptr, bus, nullptr, agent, sess);
        saved_id = s1.session_id();
        // 手动塞入 history (绕过 chat() - 避免 LLM 调用)
        // 利用 save_to_disk 落盘
        REQUIRE(s1.save_to_disk());
        REQUIRE(std::filesystem::exists(tmp.path() / (saved_id + ".json")));
    }

    // 进程 2: 恢复 session
    {
        ChatSession s2(nullptr, bus, nullptr, agent, sess);
        REQUIRE(s2.load_from_disk(saved_id));
        REQUIRE(s2.session_id() == saved_id);
        // history 为空但 session 恢复成功
        REQUIRE(s2.history().empty());
    }
}

TEST_CASE("session persistence: corrupted JSON degrades gracefully", "[session][persistence]") {
    TempDir tmp;
    auto bus = std::make_shared<MockBus>();

    AgentConfig agent;
    SessionConfig sess;
    sess.persist_dir = tmp.str();

    // 写入损坏的 JSON 文件
    auto corrupt_path = tmp.path() / "sess_corrupt.json";
    write_file(corrupt_path, "{ this is not valid json ]]]");

    ChatSession s(nullptr, bus, nullptr, agent, sess);
    // 损坏 JSON 应返回 false 而非抛异常
    REQUIRE_FALSE(s.load_from_disk("sess_corrupt"));
    // session_id 未被覆盖 (保持新生成的)
    REQUIRE_FALSE(s.session_id().empty());
}

TEST_CASE("session persistence: stale cleanup removes old files", "[session][persistence]") {
    TempDir tmp;
    auto bus = std::make_shared<MockBus>();

    AgentConfig agent;
    SessionConfig sess;
    sess.persist_dir = tmp.str();

    // 创建一个合法的 session 文件
    {
        ChatSession s(nullptr, bus, nullptr, agent, sess);
        REQUIRE(s.save_to_disk());
    }

    // 创建一个 "陈旧" 文件 - 手动设置 mtime 为 >24h 前
    auto stale_path = tmp.path() / "sess_old.json";
    write_file(stale_path, R"({"schema_version":1,"session_id":"sess_old","history":[]})");

    // 将 stale 文件的 mtime 设为 25h 前
    auto old_time = std::filesystem::file_time_type::clock::now()
                    - std::chrono::hours(25);
    std::filesystem::last_write_time(stale_path, old_time);

    // 验证两个文件都在
    auto sessions = ChatSession::list_sessions(tmp.str());
    REQUIRE(sessions.size() >= 2);

    // 执行清理 (max_age = 86400s = 24h)
    ChatSession::cleanup_stale(tmp.str(), 86400);

    // stale 文件应被删除, 新文件保留
    REQUIRE_FALSE(std::filesystem::exists(stale_path));
    auto sessions_after = ChatSession::list_sessions(tmp.str());
    REQUIRE(sessions_after.size() == 1);
    REQUIRE(sessions_after[0] != "sess_old");
}

TEST_CASE("session persistence: list_sessions scans persist_dir", "[session][persistence]") {
    TempDir tmp;
    auto bus = std::make_shared<MockBus>();

    AgentConfig agent;
    SessionConfig sess;
    sess.persist_dir = tmp.str();

    // 保存 3 个 sessions
    std::vector<std::string> ids;
    for (int i = 0; i < 3; ++i) {
        ChatSession s(nullptr, bus, nullptr, agent, sess);
        REQUIRE(s.save_to_disk());
        ids.push_back(s.session_id());
    }

    auto sessions = ChatSession::list_sessions(tmp.str());
    REQUIRE(sessions.size() == 3);
    // 验证每个 id 都在列表中
    for (const auto& id : ids) {
        bool found = false;
        for (const auto& sid : sessions) {
            if (sid == id) { found = true; break; }
        }
        REQUIRE(found);
    }
}

TEST_CASE("session persistence: schema version mismatch rejected", "[session][persistence]") {
    TempDir tmp;
    auto bus = std::make_shared<MockBus>();

    AgentConfig agent;
    SessionConfig sess;
    sess.persist_dir = tmp.str();

    // 写入 schema_version=999 的文件
    auto bad_path = tmp.path() / "sess_v999.json";
    write_file(bad_path, R"({"schema_version":999,"session_id":"sess_v999","history":[]})");

    ChatSession s(nullptr, bus, nullptr, agent, sess);
    REQUIRE_FALSE(s.load_from_disk("sess_v999"));
}
