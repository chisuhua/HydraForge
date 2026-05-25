### AgenticDSL '/taxonomy/axis4_frontend/frontend_ui_workflow'

# Frontend UI Workflow — AgenticDSL 实现

> **轴分类**: 轴4-UI/前端
> **核心 DSL 特性**: user_input, dsl_call, fork, tool_call
> **对应 Superpowers 技能**: frontend_ui_workflow

## /__meta__
execution_budget:
  max_llm_calls: 20
  max_tool_calls: 30
  max_total_nodes: 40

---

## /ui/start
type: start
next: ["/ui/gather_requirements"]

## /ui/gather_requirements
type: user_input
prompt: |
  描述你想构建的 UI：

  请提供：
  1. 功能描述（这个界面做什么）
  2. 主要组件（有哪些元素）
  3. 交互行为（点击、悬停、动画等）
  4. 响应式需求（移动端/桌面端）
  5. 技术偏好（React/Vue/原生 HTML+CSS）

  如果有设计稿或参考链接，请提供。
input_variable: ui_requirements
input_type: multiline
next: ["/ui/analyze_ui"]

## /ui/analyze_ui
type: dsl_call
llm_tool: gpt-4
output_keys: ["ui_analysis"]
prompt: |
  UI 需求:

  {{ui_requirements}}

  分析 UI 结构：
  1. 组件树（有哪些组件，层级关系）
  2. 状态管理（哪些组件有状态）
  3. 交互模式（用户如何与应用交互）
  4. 技术选型建议

  输出结构化的 UI 分析。
next: ["/ui/generate_components"]

## /ui/generate_components
type: fork
branches:
  - "/ui/generate_html"
  - "/ui/generate_styles"
  - "/ui/generate_logic"
context_isolation: deep_copy
next: ["/ui/assemble_ui"]

## /ui/generate_html
type: dsl_call
llm_tool: gpt-4
output_keys: ["html_code"]
prompt: |
  UI 分析:
  {{ui_analysis}}

  基于分析结果，生成 HTML 结构：
  - 使用语义化 HTML5 标签
  - 合理的 class/id 命名
  - 可访问性考虑（ARIA labels）

  输出完整的 HTML 代码。
next: ["/ui/save_html"]

## /ui/save_html
type: tool_call
tool_name: write_file
arguments:
  path: "src/ui/components/{{component_name|default:'index'}}.html"
  content: "{{html_code}}"
output_keys: ["html_path"]
next: ["/ui/end_html"]

## /ui/end_html
type: end

## /ui/generate_styles
type: dsl_call
llm_tool: gpt-4
output_keys: ["css_code"]
prompt: |
  UI 分析:
  {{ui_analysis}}

  基于分析结果，生成 CSS 样式：
  - 使用 CSS Variables 管理主题
  - Flexbox/Grid 布局
  - 响应式媒体查询
  - 动画和过渡效果
  - 移动优先设计

  输出完整的 CSS 代码。
next: ["/ui/save_styles"]

## /ui/save_styles
type: tool_call
tool_name: write_file
arguments:
  path: "src/ui/styles/{{component_name|default:'main'}}.css"
  content: "{{css_code}}"
output_keys: ["css_path"]
next: ["/ui/end_styles"]

## /ui/end_styles
type: end

## /ui/generate_logic
type: dsl_call
llm_tool: gpt-4
output_keys: ["js_code"]
prompt: |
  UI 分析:
  {{ui_analysis}}

  基于分析结果，生成 JavaScript 交互逻辑：
  - 事件处理（click, hover, input）
  - 状态管理
  - API 调用（如需要）
  - 动画控制

  输出完整的 JavaScript 代码。
next: ["/ui/save_logic"]

## /ui/save_logic
type: tool_call
tool_name: write_file
arguments:
  path: "src/ui/logic/{{component_name|default:'main'}}.js"
  content: "{{js_code}}"
output_keys: ["js_path"]
next: ["/ui/end_logic"]

## /ui/end_logic
type: end

## /ui/assemble_ui
type: dsl_call
llm_tool: gpt-4
output_keys: ["assembled_ui"]
prompt: |
  生成的 UI 组件：

  HTML: {{html_code}}
  CSS: {{css_code}}
  JavaScript: {{js_code}}

  组装成一个完整的、可运行的 HTML 文件，包含内联样式和脚本。
next: ["/ui/create_preview"]

## /ui/create_preview
type: tool_call
tool_name: write_file
arguments:
  path: "src/ui/preview/{{timestamp}}-{{component_name|default:'ui'}}.html"
  content: "{{assembled_ui}}"
output_keys: ["preview_path"]
next: ["/ui/verify_ui"]

## /ui/verify_ui
type: user_input
prompt: |
  UI 预览已生成: {{preview_path}}

  请在浏览器中打开预览，确认 UI 是否符合预期。
  如需修改，请描述需要调整的地方。
input_variable: ui_approved
input_type: confirm
next: ["/ui/route_approval"]

## /ui/route_approval
type: assert
condition: "{{ui_approved}} == true"
on_failure: "/ui/refine_ui"
next: ["/ui/finalize"]

## /ui/refine_ui
type: user_input
prompt: |
  请描述需要修改的内容：
input_variable: refinement
input_type: multiline
next: ["/ui/apply_refinement"]

## /ui/apply_refinement
type: dsl_call
llm_tool: gpt-4
output_keys: ["refined_ui"]
prompt: |
  原 UI:
  {{assembled_ui}}

  修改需求:
  {{refinement}}

  应用修改，生成更新后的 UI。
next: ["/ui/update_preview"]

## /ui/update_preview
type: tool_call
tool_name: write_file
arguments:
  path: "{{preview_path}}"
  content: "{{refined_ui}}"
output_keys: []
next: ["/ui/verify_ui"]

## /ui/finalize
type: tool_call
tool_name: bash
arguments:
  command: "echo 'UI components generated successfully'"
  timeout: "5"
output_keys: ["summary"]
next: ["/ui/end"]

## /ui/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：frontend_ui_workflow 技能

  ### 1. ui_component 节点
  # UI 组件节点
  type: ui_component
  name: "{{component_name}}"
  structure:
    - type: header
      children: [title, nav]
    - type: main
      children: [sidebar, content, footer]
  styles:
    theme: modern|classic|minimal
    responsive: mobile_first
  output:
    html: component_html
    css: component_css
    js: component_js

  ### 2. style_sheet 节点
  # 样式表生成器
  type: style_sheet
  design_system:
    colors:
      primary: "#007bff"
      secondary: "#6c757d"
    spacing: 8px
    border_radius: 4px
  components: [button, card, form, modal]
  output: stylesheet_content

  ### 3. visual_test 节点
  # 视觉测试验证
  type: visual_test
  target: "{{preview_path}}"
  baseline: "{{baseline_screenshot}}"
  threshold: 0.1
  on_pass:
    next: "/ui/finalize"
  on_fail:
    next: "/ui/refine_ui"
    output: diff_report

  ### 4. skill_invoke（调用前端技能）
  type: skill_invoke
  skill: "frontend_ui_workflow"
  input:
    requirements: "{{ui_requirements}}"
    tech_stack: "html+css+js"
  output:
    components: generated_components
    preview: preview_path
