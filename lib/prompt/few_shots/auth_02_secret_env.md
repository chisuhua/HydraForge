# auth_02_secret_env

## input

把数据库密码从环境变量 DB_PASSWORD 注入工作流。

## output

```yaml
## auth.secret_env
name: DB_PASSWORD
required: true
mask: true
```

## error

反例: 明文写密码 -> 违反 mask 语义规则。
