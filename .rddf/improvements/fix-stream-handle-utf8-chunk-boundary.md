# fix-stream-handle-utf8-chunk-boundary

**优先级**: P2 | **来源**: dual-agent 调研 (2026-10-14) — Metis 失败模式 5
**阶段**: post-6c | **分类**: execution-runtime
**类型**: bugfix
**主题**: kStreamChunkSize=64 字节切片在 UTF-8 多字节字符中间截断, 产生无效 UTF-8 chunk

## 架构依据

per 2026-10-14 dual-agent 调研 (`ses_ec5f36ffaffeT3KEsoFqAUPq43`):
- **V1 ship (2026-09-04)**: `i_stream_handle.h` (5 虚函数) + `stream_handle.{h,cpp}` (`BufferedStreamHandle` pull + `CallbackStreamHandle` push+pull)
- **V1 切片逻辑**: `kStreamChunkSize=64` 字节切片 (per 提案原文 + 已 ship 代码)

## Status: 🔴 OPEN (2026-10-15 audit verification, 真待 ship)

**Status 段补全** (per Pattern #10 hygiene):

**5-维判定**:
- 维度 1 self-declared: header 无 SUPERSEDED, 无 status
- 维度 2 archive: ❌ (dual-agent 调研新立, 2026-10-14 commit `3e33929`)
- 维度 3 commit: ✅ commit `3e33929` (Phase B 立 4 个 improvement)
- 维度 4 ADR: ADR-0072 D1 stream 🟡 Partial, UTF-8 切片边界未在 spec 明确
- 维度 5 fallback: V1 ship (2026-09-04) 切片逻辑存在, UTF-8 边界 bug 未修

**判定依据**:
- `kStreamChunkSize=64` 字节切片可能在 CJK 多字节字符中间截断, 产生无效 UTF-8 chunk
- Metis 失败模式 5 识别: 用户输入中文 prompt + 工具输出含中文 → chunk 边界产生乱码
- effort 估: 0.5-1 day (UTF-8 边界检测 + chunk 重对齐 + 测试 3 case)

**判定结论**: 🔴 OPEN (真待 ship, 需独立 OpenSpec change + TDD 5-step 实施)

**触发条件**:
- 用户报告 "中文/日文/韩文 stream chunk 乱码"
- 任何 UTF-8 测试 fixture 触发

**Pattern 沉淀**: dual-agent Phase B 立 4 个 improvement 第 4 个, 真待 ship 不是 ship drift. 与 adr-0072-d1-stream-v2-true-streaming (B1) + adr-0072-d4-env-vars-redaction (B4) + fix-stream-handle-sink-lifecycle (B2) 同源 (per 2026-10-14 调研).
- **bug 场景**: 64 字节切片可能在 UTF-8 多字节字符中间截断
  - ASCII 安全 (1 byte = 1 char)
  - CJK 危险: UTF-8 中文 = 3 bytes/char, 64 字节可能落在第 22 字符中间
  - Emoji 危险: UTF-8 emoji = 4 bytes/char, 64 字节可能落在第 16 emoji 中间

## Why

### Bug 实证 (推测)

```cpp
// 假设 kStreamChunkSize = 64
// LLM 输出: "Hello 世界! 😀 World"
//          H=1  e=1  l=1  o=1  ' '=1  世=3  界=3  !=1  ' '=1  😀=4  ' '=1  W=1  o=1  r=1  l=1  d=1
//          1   2   3   4    5   6-8  9-11 12  13  14-17 18  19  20  21  22  23

// 切片 #1 (bytes 1-64): "Hello 世" (含 世 的 3 bytes)
// 切片 #2 (bytes 65-128): "界! 😀 W" (含 界 3 + ! 1 + 😀 4 都在)
//   ↑ 但 "世" 后面的字节在 chunk 1 末尾可能不完整 (实际 64 字节切割)
```

**真实风险**: 若 chunk 边界落在 世 的第 2 字节 (即 0xE4 0xB8), 切片 #1 是 6 字节 ASCII + 2 字节 UTF-8 fragment = **无效 UTF-8 chunk**. 消费端解码失败 → ??? 替换或 silent corrupt.

### 影响

- **TTY 输出**: CJK 字符显示为 ? 或 0xFFFD (REPLACEMENT CHARACTER)
- **JSON 输出**: chunk 拼回完整字符串时, JSON parser 拒绝 (invalid UTF-8)
- **测试 fixture**: 既有测试用 ASCII 文本, **未发现 bug** (V1 ship 15/19 PASS 已知限制 4 个包含此)

## What Changes (proposal scope)

- **修复 1**: `stream_handle.cpp` 切片函数改用 UTF-8 boundary-aware 算法:
  - 扫描 chunk 末尾字节
  - 若在 multi-byte sequence 中 (high bit set, second byte 0x80-0xBF), 回退到 sequence 起始位置
  - 返回的 chunk 长度 ≤ 64 bytes 但保证 UTF-8 valid
- **修复 2**: 新增单元测试覆盖 CJK + Emoji 边界场景
- **修复 3**: 文档化 `kStreamChunkSize=64` 是 **byte 数**, chunk 实际 UTF-8 字符数可能 < 64

## Out of Scope

- 改 chunk 大小 (建议 V2 改 dynamic chunk size, 本 fix 仅修 boundary)
- 其他 UTF-8 invalid 来源 (LLM 输出本身 invalid 序列, 需更上游 fix)
- V2 完整流式 (per `adr-0072-d1-stream-v2-true-streaming.md`)

## Dependencies

- **前置 ship**: `adr-0072-d1-stream-runtime-semantics` 已 ship (V1, 2026-09-04)

## Acceptance Criteria (建议)

- AC-1: `BufferedStreamHandle::next()` 返回的 chunk 字符串通过 `utf8::is_valid()` 检查
- AC-2: 单元测试覆盖 CJK 中间截断场景 ("Hello 世界" 64 bytes 切片边界)
- AC-3: 单元测试覆盖 Emoji 中间截断场景 ("Hello 😀 World" 64 bytes 切片边界)
- AC-4: 既有 `test_stream_runtime.cpp` 19 cases 零回归
- AC-5: 文档化 chunk size 语义 (bytes vs chars)

## Estimated Effort

- 修复 1: 2-3h (UTF-8 边界扫描算法)
- 修复 2: 2-3h (CJK + Emoji 单元测试)
- 修复 3: 0.5h (注释)
- 总: 0.5-1 天

## Reference

- `src/common/runtime/stream_handle.cpp` (V1 ship 切片函数)
- `tests/test_stream_runtime.cpp` (V1 19 cases 已知限制 4 个)
- UTF-8 spec: RFC 3629 (multi-byte sequence 0xC0-0xFD + continuation 0x80-0xBF)
- Dual-agent 调研: `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md`

## Status

📋 **登记但未立项** (per 2026-10-14 dual-agent 调研, Metis 失败模式 5 隐藏 bug 发现). 待独立 OpenSpec change 立项实施.
