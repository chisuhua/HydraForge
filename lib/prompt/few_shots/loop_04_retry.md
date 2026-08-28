# loop_04_retry

## input

失败重试 3 次退避。

## output

```yaml
## loop.retry
max_retry: 3
backoff: exponential
```

## error

反例: backoff 非法 -> schema 失败。
