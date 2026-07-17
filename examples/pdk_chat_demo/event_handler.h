// event_handler.h - 事件订阅 + 终端渲染 + OTel 导出
// 关联: docs/adr/adr-0019-iinteraction-bus-mvp.md
//      docs/adr/adr-0063-opentelemetry-tracing.md

#pragma once

#include <iostream>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace agenticdsl {
    class IInteractionBus;
}

namespace pdk_chat_demo {

// EventHandler: 订阅 IInteractionBus 事件 → 终端输出
// 可选: 导出到 OTel Collector (ADR-0063)
class EventHandler {
public:
    explicit EventHandler(
        std::shared_ptr<agenticdsl::IInteractionBus> bus,
        std::ostream* out = &std::cout
    );

    ~EventHandler();

    // 启用 OTel 导出（Phase 2，v1 仅占位）
    void enable_otel_export(const std::string& endpoint);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace pdk_chat_demo