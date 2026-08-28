# utils_02_template_render

## input

用 inja 渲染 template.md 并注入变量。

## output

```yaml
## utils.template_render
template: template.md
vars: {name: hydra}
output: rendered
```

## error

反例: 漏 vars 对象 -> 语义规则缺失。
