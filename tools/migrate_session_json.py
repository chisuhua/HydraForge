#!/usr/bin/env python3
# tools/migrate_session_json.py
# 功能描述: 旧版线性 JSON 单文件会话 → 新版 JSONL 树状存储迁移工具
#          对应 OpenSpec change session-manager-jsonl §4 + Task 6
#          与 C++ SessionManager::migrate_legacy_json 行为对齐:
#            1. 读取 legacy_path (JSON: {"messages": [...]})
#            2. 复制 legacy_path → <legacy_path>.backup
#            3. 写入新 JSONL 文件 (session_id 取 legacy_path.stem)
#               - 每个 message 转为 SessionNode {id, parent_id, branch_id="main",
#                 content=msg}
#               - 末尾追加 BranchMeta {type="branch", branch_id="main", name="main",
#                 forked_from_node="", created_at=<unix_ms>}
#          输出: 打印迁移后的 session_id, 退出 0 成功, 非 0 失败
# 设计依据: OpenSpec change session-manager-jsonl §4
# 作者: AgenticDSL Phase 5 / Session Manager JSONL Sprint
# 最后修改日期: 2026-08-05

import argparse
import json
import os
import shutil
import sys
import time
from pathlib import Path

# 镜像 C++ 实现的常量
LEGACY_FILE_EXT = ".json"      # 旧版格式后缀
JSONL_FILE_EXT = ".jsonl"      # 新版格式后缀 (与 src/core/session_manager.h kSessionFileExt 一致)
MAIN_BRANCH_ID = "main"


def _next_node_id(counter: list) -> str:
  """
  镜像 C++ next_node_id() — "node_<hex_counter>" 格式.
  counter 是单元素 list, 用作可变引用避免 nonlocal 关键字.
  """
  counter[0] += 1
  return "node_{:x}".format(counter[0])


def _iso_now_ms() -> int:
  """ISO 8601 (毫秒精度) — 镜像 C++ chrono::system_clock."""
  return int(time.time() * 1000)


def _build_branch_meta(now_ms: int) -> dict:
  """构造 BranchMeta 的 JSON 序列化 — 镜像 C++ BranchMeta::to_json()."""
  return {
      "type": "branch",
      "branch_id": MAIN_BRANCH_ID,
      "name": MAIN_BRANCH_ID,
      "forked_from_node": "",
      "created_at": str(now_ms),
  }


def _build_session_node(node_id: str, parent_id: str, branch_id: str,
                        content: dict) -> dict:
  """构造 SessionNode 的 JSON 序列化 — 镜像 C++ SessionNode::to_json()."""
  return {
      "id": node_id,
      "parent_id": parent_id,
      "branch_id": branch_id,
      "content": content,
  }


def migrate_legacy(legacy_path: Path, output_dir: Path) -> str:
  """
  执行迁移 — 镜像 C++ SessionManager::migrate_legacy_json().

  Returns:
    迁移后使用的 session_id (legacy_path.stem()).

  Raises:
    FileNotFoundError: 旧文件不存在.
    json.JSONDecodeError: 旧文件不是合法 JSON.
    RuntimeError: 其它 I/O 或解析错误.
  """
  legacy_path = Path(legacy_path)
  output_dir = Path(output_dir)

  if not legacy_path.exists():
    raise FileNotFoundError(
        "migrate_legacy: legacy file not found: {}".format(legacy_path))

  # 1) 读取旧版 JSON
  with open(legacy_path, "r", encoding="utf-8") as fp:
    try:
      legacy = json.load(fp)
    except json.JSONDecodeError as exc:
      raise RuntimeError(
          "migrate_legacy: parse error in {}: {}".format(
              legacy_path, exc)) from exc

  # 2) 备份原文件 — std::filesystem::copy + overwrite_existing 语义
  backup_path = legacy_path.with_name(legacy_path.name + ".backup")
  shutil.copyfile(legacy_path, backup_path)  # 隐式覆盖

  # 3) 计算 session_id — 镜像 legacy_path.stem()
  session_id = legacy_path.stem
  if not session_id:
    session_id = "migrated_{}".format(os.getpid())

  # 4) 创建输出目录
  output_dir.mkdir(parents=True, exist_ok=True)

  # 5) 写新 JSONL (覆盖现有同名文件: 与 compact() 行为一致)
  jsonl_path = output_dir / (session_id + JSONL_FILE_EXT)

  counter = [0]  # 模拟 C++ atomic counter
  parent_id = ""

  with open(jsonl_path, "w", encoding="utf-8") as fp:
    # 遍历 messages 数组 (与 C++ 一致: 仅处理 "messages" key)
    messages = legacy.get("messages", [])
    if isinstance(messages, list):
      for msg in messages:
        node = _build_session_node(
            node_id=_next_node_id(counter),
            parent_id=parent_id,
            branch_id=MAIN_BRANCH_ID,
            content=msg,
        )
        # nlohmann::json::dump() 默认无 '\n' (单行 JSON)
        fp.write(json.dumps(node, ensure_ascii=False) + "\n")
        parent_id = node["id"]

    # 末尾追加 BranchMeta "main"
    now_ms = _iso_now_ms()
    bm = _build_branch_meta(now_ms)
    fp.write(json.dumps(bm, ensure_ascii=False) + "\n")

    # fsync — 强制刷盘 (与 C++ flush_append 一致的崩溃安全语义)
    fp.flush()
    os.fsync(fp.fileno())

  return session_id


def _build_argparser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
      prog="migrate_session_json.py",
      description=("将旧版线性 JSON 单文件会话迁移为新版 JSONL 树状存储.\n"
                   "镜像 C++ SessionManager::migrate_legacy_json() 行为.\n"
                   "默认输出目录: ~/.hydraforge/sessions/\n"
                   "(可由环境变量 HYDRAFORGE_SESSION_DIR 覆盖)"),
      formatter_class=argparse.RawDescriptionHelpFormatter,
      epilog=("示例:\n"
              "  python3 tools/migrate_session_json.py --input legacy.json\n"
              "  python3 tools/migrate_session_json.py --input /data/a.json "
              "--output-dir /var/lib/hydra/sessions/"),
  )
  parser.add_argument(
      "--input",
      "-i",
      required=True,
      help="旧版 JSON 文件路径 (含 .json 后缀)",
  )
  parser.add_argument(
      "--output-dir",
      "-o",
      default=None,
      help=("迁移目标目录. 默认: $HYDRAFORGE_SESSION_DIR 或 "
            "~/.hydraforge/sessions/"),
  )
  return parser


def main(argv=None) -> int:
  parser = _build_argparser()
  args = parser.parse_args(argv)

  # 解析输出目录
  if args.output_dir is None:
    env_dir = os.environ.get("HYDRAFORGE_SESSION_DIR")
    if env_dir:
      output_dir = Path(env_dir)
    else:
      output_dir = Path.home() / ".hydraforge" / "sessions"
  else:
    output_dir = Path(args.output_dir)

  legacy_path = Path(args.input)

  try:
    session_id = migrate_legacy(legacy_path, output_dir)
  except FileNotFoundError as exc:
    print("error: {}".format(exc), file=sys.stderr)
    return 1
  except RuntimeError as exc:
    print("error: {}".format(exc), file=sys.stderr)
    return 2
  except Exception as exc:  # 防御性兜底
    print("error: unexpected {}".format(exc), file=sys.stderr)
    return 3

  # 成功: 打印 session_id + 路径
  print(session_id)
  return 0


if __name__ == "__main__":
  sys.exit(main())
