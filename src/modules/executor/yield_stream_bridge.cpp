#include "yield_stream_bridge.h"

namespace agenticdsl {

std::optional<std::string>
YieldStreamBridge::pull_single(IGenerationStream& stream) {
    if (!stream.is_active()) {
        return std::nullopt;
    }
    if (stop_token_.stop_requested()) {
        return std::nullopt;
    }
    return stream.next(stop_token_);
}

std::vector<std::string>
YieldStreamBridge::pull_loop(IGenerationStream& stream,
                              BudgetChecker budget_checker,
                              std::size_t max_iter) {
    std::vector<std::string> consumed;

    for (std::size_t i = 0; i < max_iter; ++i) {
        if (stop_token_.stop_requested()) {
            break;
        }
        if (!budget_checker()) {
            throw BudgetExceededException(
                "Budget exceeded during YIELD CONTINUE mode after " +
                std::to_string(consumed.size()) + " tokens",
                consumed
            );
        }
        if (!stream.is_active()) {
            break;
        }

        auto token = stream.next(stop_token_);
        if (!token.has_value()) {
            break;
        }
        consumed.push_back(std::move(*token));
    }

    return consumed;
}

} // namespace agenticdsl