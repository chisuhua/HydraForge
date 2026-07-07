// pdk/llama_engine/src/llama_state.h
// 功能描述：C14 llama_engine 内部共享状态 (EngineState)
//           由 llama_engine.cpp 定义，llama_model.cpp/inference_arch.cpp 通过 extern 引用
//           编译到同一 .so 中 (libhydraforge_llama_engine.so)，无链接问题
// 作者：C14 Oracle review session
// 最后修改日期：2026-07-07

#pragma once

#include <llama.h>
#include <mutex>
#include <string>
#include <vector>

namespace agenticdsl::pdk::llama {

struct EngineState {
  std::mutex mtx;
  llama_model* model = nullptr;
  llama_context* ctx = nullptr;
  bool initialized = false;
  std::string active_sampler = "temperature";

  struct LoadedModel {
    std::string model_id;
    std::string model_path;
    std::string quantization;
    bool active = false;
  };
  std::vector<LoadedModel> loaded_models;

  ~EngineState() {
    if (ctx) { llama_free(ctx); ctx = nullptr; }
    if (model) { llama_model_free(model); model = nullptr; }
  }
};

EngineState& engine_state();

} // namespace agenticdsl::pdk::llama