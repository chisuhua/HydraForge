# inference_03_generate

## input

给定提示词生成补全。

## output

```yaml
## inference.generate
prompt: 你好
max_tokens: 256
temperature: 0.7
```

## error

反例: temperature 越界 -> 语义边界错误。
