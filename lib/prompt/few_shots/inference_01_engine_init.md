# inference_01_engine_init

## input

初始化推理引擎并指定模型路径。

## output

```yaml
## inference.engine.init
model_path: models/qwen.gguf
n_ctx: 4096
```

## error

反例: 漏 n_ctx -> 语义规则缺失。
