// examples/slice_01_tool_call/main.cpp
// 文件头注释
// 功能描述：Track 0.2 slice_01 端到端示例骨架。
//          MVP：仅 --mock 模式（使用 MockLLMProvider + 注册 echo 工具）。
//          完整 ReAct 循环留待 Task 10 填充。
// 设计依据：plan §7
// 作者：AgenticDSL Phase 0 / Track B
// 最后修改日期：2026-06-08

#include "core/engine.h"
#include "modules/cognitive/simple_orchestrator.h"
#include "core/types/tool_result.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");
  if (!mock_mode) {
    std::cerr << "Usage: " << argv[0] << " --mock\n";
    return 1;
  }

  std::cout << "slice_01_tool_call --mock mode (scaffold)\n";
  // TODO(mvp): implement full ReAct loop in Task 10
  return 0;
}