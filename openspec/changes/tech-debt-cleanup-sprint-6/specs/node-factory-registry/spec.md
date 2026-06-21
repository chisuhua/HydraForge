## ADDED Requirements

### Requirement: node-factory-registry-class

项目 MUST 提供 `NodeFactoryRegistry` 类于 `include/agenticdsl/parser/node_factory.h`,类 MUST 满足:
- MUST 使用 `std::unordered_map<NodeType, Factory>` 存储工厂映射
- MUST 定义 `Factory = std::function<std::unique_ptr<Node>(const nlohmann::json&)>`
- MUST 提供 `register_factory(NodeType, Factory)` / `create(NodeType, const json&) -> std::unique_ptr<Node>` / `has_factory(NodeType) const -> bool` / `size() const` 4 个公共方法
- MUST 提供静态 `global()` 单例访问,MUST 线程安全 (使用 `std::shared_mutex` 保护查找)
- MUST 位于命名空间 `agenticdsl` (扁平, 与现有 contract 头一致)

#### Scenario: Registry 类存在

- **WHEN** `grep "class NodeFactoryRegistry" include/agenticdsl/parser/node_factory.h`
- **THEN** MUST 命中类定义
- **AND** MUST 含 4 个公共方法 + `static global()` 单例
- **AND** MUST 含 `mutable std::shared_mutex mutex_` 成员

#### Scenario: Registry 全局单例

- **WHEN** 业务代码调用 `NodeFactoryRegistry::global().create(NodeType::LLM, spec)`
- **THEN** MUST 返回根据 spec 构造的 `std::unique_ptr<Node>`
- **AND** 多次调用 MUST 返回独立实例（unique_ptr 语义）

### Requirement: parser-uses-registry

`MarkdownParser::create_node_from_json` MUST 重构为调用 `NodeFactoryRegistry::global().create()`:
- 函数体 MUST ≤ 30 行（原 216 行）
- 仅处理 NodeType → Factory 映射,具体构造逻辑委托各 NodeType factory 函数
- 13 个现有 NodeType MUST 各自注册 factory（迁移原 if-else 分支）

#### Scenario: create_node_from_json 函数行数

- **WHEN** `awk '/^std::unique_ptr<Node> MarkdownParser::create_node_from_json/,/^}$/' src/modules/parser/markdown_parser.cpp | wc -l`
- **THEN** MUST ≤ 30 行

#### Scenario: 13 个 NodeType 注册

- **WHEN** 静态初始化块 `static bool _ = [] { ... }()` 在 markdown_parser.cpp 末尾执行
- **THEN** MUST 注册所有 13 个现有 NodeType（grep `NodeFactoryRegistry::global().register_factory` ≥ 13）
- **AND** 每个 NodeType MUST 对应一个 factory 函数（`make_llm_node` / `make_tool_node` / ...）

#### Scenario: 新增 NodeType 无需修改本函数

- **WHEN** 业务代码新增 `NodeType::MyCustom` + 实现 `make_my_custom_node` 工厂
- **THEN** 仅需在静态注册块加 1 行 `register_factory(NodeType::MyCustom, make_my_custom_node)`
- **AND** `create_node_from_json` 函数体 MUST NOT 修改（OCP 满足）

#### Scenario: 工厂查找性能

- **WHEN** `create_node_from_json` 被高频调用（parser hot path）
- **THEN** `unordered_map::find` 时间复杂度 MUST 为 O(1) 平均
- **AND** `shared_mutex` 允许多线程并发读取

### Requirement: parser-factory-test-coverage

`tests/test_parser.cpp` MUST 新增 ≥ 5 个 Catch2 test case:

- `factory_registry_registers_all_types`: 验证 13 个 NodeType 全注册
- `factory_registry_creates_correct_subtype`: 验证 create() 返回正确子类
- `factory_registry_unknown_type_throws`: 验证未注册 NodeType 抛 `std::runtime_error`
- `factory_registry_global_singleton`: 验证 `global()` 返回同一实例
- `factory_registry_concurrent_access`: 多线程并发 register + create 无数据竞争

#### Scenario: 5 个新 test case 通过

- **WHEN** `cd build && ctest -R test_parser --output-on-failure`
- **THEN** MUST 至少 5 个新 TEST_CASE 通过
- **AND** 既有 test_parser test MUST 零回归

#### Scenario: TSan 干净

- **WHEN** `cmake --preset tsan && ctest -R test_parser --output-on-failure`
- **THEN** MUST 0 data race report (shared_mutex 正确同步)