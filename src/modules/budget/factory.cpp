// src/modules/budget/factory.cpp
// Sprint 6 P2-7: BudgetController 工厂实现
#include "budget/factory.h"
#include "modules/budget/budget_controller.h"

namespace agenticdsl::budget {

std::unique_ptr<BudgetController> create_controller() {
  return std::make_unique<BudgetController>();
}

}  // namespace agenticdsl::budget
