# utils_04_path_join

## input

拼接 /var/log 与 app.log。

## output

```yaml
## utils.path_join
parts: [/var/log, app.log]
output: full_path
```

## error

反例: parts 缺一项 -> 语义规则未满足。
