# src/modules/skill_interpreter/CMakeLists.txt
# 功能描述：SkillInterpreter 模块 — ADR-0055 定义的 SKILL.md 命令式 DSL
#          隔离执行引擎。支持 posix_spawn + seccomp BPF + NDJSON IPC。
# 设计依据：ADR-0055（Skill 隔离执行模型）
#          + openspec/changes/skill-interpreter-real-loading/
# 作者：AgenticDSL SkillInterpreter change
# 最后修改日期：2026-07-22

# Linux-only: seccomp/posix_spawn/pipe2 是 Linux 特定 API
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_library(agenticdsl_modules_skill_interpreter STATIC
      skill_interpreter.cpp
      skill_child_main.cpp
  )

  target_link_libraries(agenticdsl_modules_skill_interpreter PUBLIC
      agenticdsl_includes
      agenticdsl_common
      agenticdsl_core
  )

  target_compile_options(agenticdsl_modules_skill_interpreter PRIVATE
      -Wno-psabi
  )
endif()