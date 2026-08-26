#ifndef AGENTICDSL_TYPES_EXECUTION_TRACE_H
#define AGENTICDSL_TYPES_EXECUTION_TRACE_H

#include "core/types/tool_result.h"
#include <string>
#include <vector>

namespace agenticdsl {

/**
 * @brief High-level task completion summary for evaluation.
 * 
 * Distinct from TraceRecord (low-level event log).
 * Workers construct ExecutionTrace from terminal ToolResult.
 */
struct ExecutionTrace {
    ToolResult final_result;                    // Terminal ok/error/data
    std::string trace_id;                       // Links to TraceRecord
    std::vector<std::string> trajectory_refs;   // Opaque intermediate step refs (V1: empty)
};

} // namespace agenticdsl

#endif // AGENTICDSL_TYPES_EXECUTION_TRACE_H
