// src/modules/budget/factory.h
// Sprint 6 P2-7: Budget 工厂函数
// C1 Day 6.2 (2026-06-27): 返回 IBudgetController 抽象类型
#pragma once

#include <memory>

namespace agenticdsl {

class IBudgetController;

namespace budget {

std::unique_ptr<IBudgetController> create_controller();

}  // namespace budget
}  // namespace agenticdsl
