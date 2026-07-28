// examples/pkm_temporal_demo/demo_args.cpp
// 功能描述：Demo CLI 参数解析实现 (Task 4 Step 3)。
//          手动解析 (--mock / --real / --scenario <name>), 无外部依赖。
//          规则:
//            --mock: 使用 MockTemporalClient
//            --real: 使用 gRPC TemporalClient (stub)
//            --scenario <name>: 场景名 (blocking/async-poll/signal/idempotent)
//            未知参数忽略
//            --mock + --real 同时出现: 后者优先
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 4 Step 3
// 作者：pkm-temporal-demo-scaffold Task 4
// 最后修改日期：2026-07-28

#include "demo_args.h"

#include <string>
#include <vector>

namespace agenticdsl {
namespace pdk {
namespace demo {

DemoArgs parse_args(const std::vector<std::string>& args) {
  DemoArgs result;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--mock") {
      result.mode = DemoMode::Mock;
    } else if (arg == "--real") {
      result.mode = DemoMode::Real;
    } else if (arg == "--scenario") {
      // 下一个参数是场景名
      if (i + 1 < args.size()) {
        result.scenario = args[++i];
      }
    }
    // 未知参数忽略
  }
  return result;
}

DemoArgs parse_args(int argc, char** argv) {
  std::vector<std::string> args;
  // 跳过 argv[0] (程序名)
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return parse_args(args);
}

}  // namespace demo
}  // namespace pdk
}  // namespace agenticdsl
