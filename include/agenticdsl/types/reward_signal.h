#ifndef AGENTICDSL_TYPES_REWARD_SIGNAL_H
#define AGENTICDSL_TYPES_REWARD_SIGNAL_H

#include <stdexcept>

namespace agenticdsl {

/**
 * @brief Three-valued quality assessment with scalar reward.
 * 
 * Used for RLHF/DPO gradient weighting and policy optimization.
 */
struct RewardSignal {
    enum class Quality {
        Excellent,  // High-quality execution
        Acceptable, // Meets baseline requirements
        Poor        // Failed or low-quality execution
    };

    Quality quality;
    double scalar;      // in [-1.0, 1.0], validated
    double confidence;  // in [0.0, 1.0], for gradient weighting

    /**
     * @brief Factory for Excellent quality.
     * @param confidence Confidence level (default 1.0)
     * @return RewardSignal with scalar=1.0
     * @throws std::out_of_range if confidence not in [0.0, 1.0]
     */
    static RewardSignal excellent(double confidence = 1.0) {
        if (confidence < 0.0 || confidence > 1.0) {
            throw std::out_of_range("confidence must be in [0.0, 1.0]");
        }
        return {Quality::Excellent, 1.0, confidence};
    }

    /**
     * @brief Factory for Acceptable quality.
     * @param confidence Confidence level (default 1.0)
     * @return RewardSignal with scalar=0.0
     * @throws std::out_of_range if confidence not in [0.0, 1.0]
     */
    static RewardSignal acceptable(double confidence = 1.0) {
        if (confidence < 0.0 || confidence > 1.0) {
            throw std::out_of_range("confidence must be in [0.0, 1.0]");
        }
        return {Quality::Acceptable, 0.0, confidence};
    }

    /**
     * @brief Factory for Poor quality.
     * @param confidence Confidence level (default 1.0)
     * @return RewardSignal with scalar=-1.0
     * @throws std::out_of_range if confidence not in [0.0, 1.0]
     */
    static RewardSignal poor(double confidence = 1.0) {
        if (confidence < 0.0 || confidence > 1.0) {
            throw std::out_of_range("confidence must be in [0.0, 1.0]");
        }
        return {Quality::Poor, -1.0, confidence};
    }
};

} // namespace agenticdsl

#endif // AGENTICDSL_TYPES_REWARD_SIGNAL_H
