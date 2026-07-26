// src/common/contract/causal_clock.h
// 功能描述：单进程逻辑时钟（ADR-0037，Change C）。
//          atomic<uint64_t> 单调递增，用于 happens-before 判定。
//          emit() 时自动 tick + attach 到 BusEvent.causal_time。
//          跨进程时升级为 Lamport 时间戳。
// 设计依据：ADR-0037 §决策 1 + Oracle 评审 (2026-07-26)。
// 作者：HydraForge Phase 6a / EventBus Chain C
// 最后修改日期：2026-07-26
#pragma once

#include <atomic>
#include <cstdint>

namespace agenticdsl::event {

class CausalClock {
public:
    using TimePoint = uint64_t;

    TimePoint tick() { return clock_.fetch_add(1, std::memory_order_relaxed); }
    TimePoint now() const { return clock_.load(std::memory_order_relaxed); }

    void merge(TimePoint external) {
        auto current = now();
        while (external > current && !clock_.compare_exchange_weak(current, external, std::memory_order_relaxed))
            ;
    }

    static bool happens_before(TimePoint a, TimePoint b) { return a < b; }

private:
    std::atomic<TimePoint> clock_{0};
};

}  // namespace agenticdsl::event