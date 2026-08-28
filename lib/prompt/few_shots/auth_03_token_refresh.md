# auth_03_token_refresh

## input

访问令牌过期时自动刷新后重试请求。

## output

```yaml
## auth.token_refresh
endpoint: /api/data
on_401: refresh_and_retry
max_retry: 2
```

## error

反例: 漏写 on_401 分支 -> parse 缺节点边。
