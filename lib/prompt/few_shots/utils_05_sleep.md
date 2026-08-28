# utils_05_sleep

## input

暂停 5 秒再继续。

## output

```yaml
## utils.sleep
duration_ms: 5000
-> ## utils.echo
```

## error

反例: duration_ms 写成 '5s' 字符串 -> 类型错误。
