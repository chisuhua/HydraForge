---
name: code-review-run
version: 0.1
description: 运行 code-review 工具审查代码文件
---

# 调用 code_review/run 工具审查文件
assign content = call_tool("fs.read", {"path": "{{args.path}}"})
assign review = call_tool("code_review/run", {"content": "{{content}}", "level": "thorough"})
return review