#ifndef AGENTICDSL_MODULES_EXECUTOR_YIELD_STREAM_BRIDGE_H
#define AGENTICDSL_MODULES_EXECUTOR_YIELD_STREAM_BRIDGE_H

// C12 Phase 5 Stage 1 Step 2 §6a: 桥接 IGenerationStream pull-based → YieldNode consume
// Oracle Risk 9 mitigation: 封装 next() 调用 + budget check + 累积 consumed_tokens

#include "common/llm/llm_types.h"
#include "scheduler/execution_session.h" // BudgetExceededException
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <stop_token>

namespace agenticdsl {

class YieldStreamBridge {
public:
    using BudgetChecker = std::function<bool()>;

    explicit YieldStreamBridge(std::stop_token stop_token = {})
        : stop_token_(stop_token) {}

    [[nodiscard]] std::optional<std::string>
    pull_single(IGenerationStream& stream);

    [[nodiscard]] std::vector<std::string>
    pull_loop(IGenerationStream& stream,
              BudgetChecker budget_checker,
              std::size_t max_iter = 10000);

private:
    std::stop_token stop_token_;
};

} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_EXECUTOR_YIELD_STREAM_BRIDGE_H