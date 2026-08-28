# human_01_confirm_step

## input

删除生产数据库前要求人工确认。

## output

```yaml
## human.confirm
message: 确认删除 prod 库?
required: true
on_timeout: abort
```

## error

反例: 漏 on_timeout -> 语义规则缺失。
