#ifndef AGENTICDSL_CONTRACT_IEVALUATOR_H
#define AGENTICDSL_CONTRACT_IEVALUATOR_H

#include "agenticdsl/types/reward_signal.h"
#include "agenticdsl/types/execution_trace.h"

namespace agenticdsl {

/**
 * @brief Pure virtual interface for evaluating execution traces.
 * 
 * Implementations MUST be thread-safe and side-effect-free.
 * Multiple workers may call evaluate() concurrently.
 */
class IEvaluator {
public:
    virtual ~IEvaluator() = default;

    /**
     * @brief Evaluate a single execution trace.
     * @param trace High-level task completion summary
     * @return RewardSignal with quality/scalar/confidence
     */
    virtual RewardSignal evaluate(const ExecutionTrace& trace) const = 0;

    /**
     * @brief Compare two execution traces.
     * @param a First trace
     * @param b Second trace
     * @return <0 if a worse than b, 0 if equal, >0 if a better than b
     */
    virtual int compare(const ExecutionTrace& a, const ExecutionTrace& b) const = 0;
};

} // namespace agenticdsl

#endif // AGENTICDSL_CONTRACT_IEVALUATOR_H
