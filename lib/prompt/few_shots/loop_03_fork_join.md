# loop_03_fork_join

## input

并行 fork 两个分支后 join。

## output

```yaml
## loop.fork_join
branches: [b1, b2]
mode: wait_all
```

## error

反例: mode 写成 wait_first -> 语义错误。
