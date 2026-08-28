# auth_01_approve_permissions

## input

用户请求读取 /etc/passwd 并列出进程。

## output

```yaml
## auth.approve_permissions
permissions:
  - read
  - process_list
scope: system
```

## error

反例: 缺少 permissions 数组 -> schema_validation_failed。
