// src/modules/budget/factory.h
// Sprint 6 P2-7: Budget 工厂函数
#pragma once

#include <memory>

namespace agenticdsl {

class BudgetController;

namespace budget {

std::unique_ptr<BudgetController> create_controller();

}  // namespace budget
}  // namespace agenticdsl
