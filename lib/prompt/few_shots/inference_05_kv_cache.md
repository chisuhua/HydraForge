# inference_05_kv_cache

## input

启用 prefix 缓存加速重复前缀。

## output

```yaml
## inference.prefix_cache
enabled: true
max_entries: 100
```

## error

反例: max_entries 为 0 -> 语义错误。
