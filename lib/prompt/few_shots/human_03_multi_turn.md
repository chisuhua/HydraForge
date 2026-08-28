# human_03_multi_turn

## input

多轮收集用户输入后再执行。

## output

```yaml
## human.ask
prompt: 输入目标路径
repeat_until: valid_path
-> ## utils.echo
```

## error

反例: repeat_until 非法 -> 语义校验失败。
