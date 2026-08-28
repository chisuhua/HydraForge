# human_02_escalation

## input

工具连续失败两次后升级给人类专家。

## output

```yaml
## human.escalate
condition: failure_count >= 2
target: oncall_engineer
```

## error

反例: condition 表达式语法错误 -> parse_failed。
