// event_handler.cpp - 事件订阅实现
// 关联: event_handler.h

#include "event_handler.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <core/types/tool_result.h>

namespace pdk_chat_demo {

class EventHandler::Impl {
public:
    std::shared_ptr<agenticdsl::IInteractionBus> bus;
    std::ostream* out;

    // Bus subscription handles（用于 unsubscribe）
    std::vector<size_t> sub_ids;

    Impl(std::shared_ptr<agenticdsl::IInteractionBus> b, std::ostream* o)
        : bus(std::move(b)), out(o) {}

    std::string now_str() {
        auto t = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(t);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }

    void print_event(const std::string& topic, const nlohmann::json& payload) {
        if (!out) return;

        std::string summary;
        if (topic == "user.input") {
            summary = payload.value("input", "");
            // 截断过长的输入
            if (summary.size() > 80) summary = summary.substr(0, 77) + "...";
        } else if (topic == "loop.turn.start") {
            summary = "turn=" + std::to_string(payload.value("turn", 0)) +
                      ", step=" + std::to_string(payload.value("step", 0));
        } else if (topic == "llm.request") {
            summary = "model=" + payload.value("model", "?");
        } else if (topic == "llm.response") {
            const auto tokens = payload.value("tokens", 0);
            const auto duration = payload.value("duration_ms", 0);
            const bool ok = payload.value("ok", true);
            summary = "tokens=" + std::to_string(tokens) +
                      ", completion=" + std::to_string(payload.value("completion_tokens", 0)) +
                      ", duration=" + std::to_string(duration) + "ms" +
                      ", ok=" + std::string(ok ? "true" : "false");
            if (!ok && payload.contains("error_message")) {
                summary += ", err=" + payload.value("error_message", std::string{});
            }
        } else if (topic == "loop.decision") {
            summary = payload.value("decision", "?");
            if (payload.contains("tool")) {
                summary += " (tool=" + payload.value("tool", std::string{"?"}) + ")";
            }
        } else if (topic == "tool.execution.start") {
            summary = payload.value("name", "?");
        } else if (topic == "tool.execution.end") {
            summary = "ok=" + std::string(payload.value("ok", false) ? "true" : "false") +
                      ", duration=" + std::to_string(payload.value("duration_ms", 0)) + "ms";
        } else if (topic == "loop.done") {
            summary = "total_steps=" + std::to_string(payload.value("total_steps", 0)) +
                      ", total_tokens=" + std::to_string(payload.value("total_tokens", 0));
        } else if (topic == "loop.error") {
            summary = payload.value("error", "?");
        } else if (topic == "session.persisted") {
            summary = "ok=" + std::string(payload.value("ok", false) ? "true" : "false");
        } else if (topic == "budget.checked") {
            summary = "remaining=$" + std::to_string(payload.value("remaining_usd", 0.0));
        } else if (topic == "session.persist_request") {
            return;
        }
        std::string trace_suffix;
        if (payload.contains("trace_id")) {
            trace_suffix = " [trace=" + payload.value("trace_id", std::string{}) + "]";
        }
        (*out) << "[" << now_str() << "] " << topic << ": " << summary << trace_suffix << std::endl;
    }
};

EventHandler::EventHandler(
    std::shared_ptr<agenticdsl::IInteractionBus> bus,
    std::ostream* out
) : impl_(std::make_unique<Impl>(std::move(bus), out)) {
    auto topics = {
        "user.input",
        "loop.turn.start",
        "loop.turn.end",
        "llm.request",
        "llm.response",
        "loop.decision",
        "tool.execution.start",
        "tool.execution.end",
        "loop.done",
        "loop.error",
        "session.persisted",
        "budget.checked"
    };

    for (const auto& topic : topics) {
        auto id = impl_->bus->subscribe(topic, [this, topic](const agenticdsl::BusEvent& event) {
            nlohmann::json merged = event.payload.data;
            if (event.payload.meta.is_object()) {
                for (auto it = event.payload.meta.begin(); it != event.payload.meta.end(); ++it) {
                    if (!merged.contains(it.key())) merged[it.key()] = it.value();
                }
            }
            impl_->print_event(topic, merged);
        });
        impl_->sub_ids.push_back(id);
    }
}

EventHandler::~EventHandler() {
    for (auto id : impl_->sub_ids) {
        impl_->bus->unsubscribe(id);
    }
}

void EventHandler::enable_otel_export(const std::string& endpoint) {
    // Phase 2: 实现 ADR-0063 OpenTelemetryExporter
    (void)endpoint;
}

}  // namespace pdk_chat_demo