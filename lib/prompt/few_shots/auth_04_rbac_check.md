# auth_04_rbac_check

## input

仅允许 admin 角色执行删除操作。

## output

```yaml
## auth.rbac
role: admin
allow: [delete]
policy: deny_by_default
```

## error

反例: allow 用了字符串而非数组 -> schema 违规。
