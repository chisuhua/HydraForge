# human_04_approval_gate

## input

高风险操作必须人工审批通过才继续。

## output

```yaml
## human.approval
level: high_risk
approver: security_lead
if_denied: abort
```

## error

反例: 缺少 if_denied -> 不重试直接 schema 失败。
