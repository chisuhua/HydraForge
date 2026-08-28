# engine_02_session_branch

## input

从会话节点 fork 出新分支。

## output

```yaml
## engine.session.branch
parent: s1
name: s1b
```

## error

反例: 漏 parent -> schema_validation_failed。
