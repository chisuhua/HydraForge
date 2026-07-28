// examples/pkm_temporal_demo/demo_args.h
// 功能描述：Demo CLI 参数类型声明 (Task 4)
//          DemoMode: Mock / Real
//          DemoArgs: mode + scenario
//          parse_args: 字符串 vector -> DemoArgs
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 4 Step 1
// 作者：pkm-temporal-demo-scaffold Task 4
// 最后修改日期：2026-07-28

#pragma once

#include <string>
#include <vector>

namespace agenticdsl {
namespace pdk {
namespace demo {

enum class DemoMode {
  Mock,
  Real,
};

struct DemoArgs {
  DemoMode mode{DemoMode::Mock};
  std::string scenario;
};

// parse_args: 解析命令行参数 (实现在 demo_args.cpp)
// args: 参数 vector (不含程序名, 如 {"--mock", "--scenario", "blocking"})
DemoArgs parse_args(const std::vector<std::string>& args);

// int argc / char** argv 重载
DemoArgs parse_args(int argc, char** argv);

}  // namespace demo
}  // namespace pdk
}  // namespace agenticdsl
