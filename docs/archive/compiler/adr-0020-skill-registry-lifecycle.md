# ADR-0020: Skill Registry 生命周期 + 特权工具模型

**状态**: 已决 (2026-05-25)
**关联决策点**: D4 (Registry 存储), D5 (特权工具)

---

## 背景

自举闭环的核心环节：编译后的技能必须注册到运行时 registry，且下一次路由时优先级高于手写版本。

当前代码库：`StandardLibraryLoader`（单例）+ 硬编码 `LibraryEntry` + `lib/*.md` 文件。无动态注册、无优先级、无热替换。

`check_permissions()` 只检查工具是否存在，无权限分级。

---

## D4: Registry 存储模型（已决）

**决策**: Option A — 文件扫描 + 运行时注册 + JSON 持久化。

**加载路径**:
- **启动时**：扫描 `lib/` 目录（`.md` DSL 标准库）+ `lib/skills/compiled/` 目录（编译产物 `.agent.md`）
- **运行时**：通过 `skill.register` 特权工具热注册（写入内存 + 可选的 JSON 持久化）
- **序列化**：`skill_registry.json` 文件（关机时选择是否持久化）

**与 StandardLibraryLoader 的关系**:
- `StandardLibraryLoader` 保持启动时扫描的角色
- `SkillRegistry` 新增运行时注册能力
- 两者的查询合并：路由到同一个匹配接口
- `skill.register` 写入同时更新 `StandardLibraryLoader` 的缓存

**优先级规则**:
- 手写版（source）：`priority=0`
- AOT 编译版（compiled）：`priority=50`
- 自举编译版（self-bootstrapped）：`priority=100`
- 路由时按 `priority` 降序匹配

---

## D5: 特权工具模型（已决）

**决策**: Option B — 独立 `privileged:` 命名空间。

**语法**:
```yaml
permissions:
  - "tool:fs.read"              # 普通工具权限
  - "privileged:skill.register" # 特权操作
  - "privileged:codelet.run"    # 特权操作
```

**特权操作列表**:
| 操作 | privilege 字符串 | 说明 |
|------|-----------------|------|
| 注册技能 | `privileged:skill.register` | 修改运行时 registry |
| 写 /lib/ 路径 | `privileged:fs.write-lib` | 写入只读标准库命名空间 |
| 执行 Python | `privileged:codelet.run` | 通过 codelet_call 执行子进程 |

**执行规则**:
1. 节点声明 `permissions` 列表
2. `execute_tool_call` 执行前检查：
   - 普通 `tool:xxx` → 检查工具是否在 `ToolRegistry` 中（现有逻辑）
   - `privileged:xxx` → 额外检查节点所属图是否标记为 `is_standard_library` 或路径以 `/lib/` 开头
3. 无特权声明的节点尝试调用特权工具 → `PermissionError`
4. 自举安全：编译器编译产物路径为 `/lib/skills/compiled/skill-compiler@v1` → 解析时 `is_standard_library=true` → 自动获得特权

**实现变更**:
- `check_permissions()` 增加 privileged 检查分支
- `ParsedGraph` 的 `is_standard_library` 标志在解析时根据路径前缀 `/lib/` 设置（已有逻辑）
- 新增 `Node::is_trusted()` 查询：向上查找所属图的 trusted 标志
