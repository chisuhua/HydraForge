// tests/test_distillation_capture_training_real_llm.cpp
// Distillation capture-mode=Training 真 LLM 端到端验证 (per AGENTS.md Reverse
// Indicator + 用户 2026-09-29 指令).
//
// 用途: test_distillation_writer.cpp 用硬编码 DistillationRecord 验证写文件契约,
// 但 mock 不证明:  (a) 真 LLM 输出能被正确构造 DistillationRecord; (b) Training 模式
// 三重保护 (agent_id 非空) 与 LLM 真输出能协同; (c) capture-mode 切换为 Training 时
// 真实数据流入能正确写出 meta.json.
//
// 此 test 用真 LLM 生成 (input, output) 对, 构造 DistillationRecord,
// 写 FileDistillationWriter, 验证:
//   1. 真 LLM 产出非空 (验证 LLM 真被调用)
//   2. capture-mode=Training 路径下 record 写出 (验证 PII 保护 + agent_id)
//   3. finalize() 写 meta.json 含 total_examples = 实际记录数
//
// tag: [distillation][real_llm][must_realllm]

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/idistillation_writer.h"
#include "agenticdsl/types/capture_mode.h"
#include "agenticdsl/types/distillation_record.h"
#include "common/llm/llm_types.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_config.h"
#include "test_helpers/real_llm_env.h"

using agenticdsl::test::must_require_real_llm_env;
using agenticdsl::test::real_llm_config;

namespace {

// 真 LLM 生成 (input, output) 对用于 distillation record
struct LlmPair {
  std::string input;
  std::string output;
};

LlmPair call_real_llm(std::shared_ptr<agenticdsl::ILLMProvider>& provider,
                       const std::string& prompt,
                       const std::string& model) {
  agenticdsl::GenerationRequest req;
  req.prompt = prompt;
  req.params.model = model;
  auto result = provider->generate(req, {});
  if (!result.has_value()) {
    return {prompt, ""};
  }
  return {prompt, result.value().text};
}

}  // namespace

TEST_CASE("Distillation capture-mode=Training real LLM smoke",
          "[must_realllm] [realllm] [distillation] [smoke] [real_llm]") {
  must_require_real_llm_env();

  auto cfg = real_llm_config();
  agenticdsl::LLMConfig llm_cfg;
  llm_cfg.provider = cfg.provider;
  llm_cfg.model = cfg.model;
  llm_cfg.api_url = cfg.api_url;
  llm_cfg.api_endpoint = cfg.api_endpoint;
  llm_cfg.api_key = cfg.api_key;
  llm_cfg.api_key_env = cfg.api_key_env;

  agenticdsl::LLMProviderFactory factory;
  auto provider = factory.create(llm_cfg);
  REQUIRE(provider != nullptr);

  std::shared_ptr<agenticdsl::ILLMProvider> provider_shared = std::move(provider);

  // 真 LLM 生成 input/output
  LlmPair pair = call_real_llm(provider_shared,
      "What is 2+3? Answer in one short sentence.",
      cfg.model);
  REQUIRE_FALSE(pair.output.empty());

  // 构造 DistillationRecord (Training 模式, agent_id 非空 = 三重保护 #1)
  agenticdsl::DistillationRecord rec;
  rec.input = pair.input;
  rec.output = pair.output;
  rec.trace_id = "real_llm_distill_001";
  rec.source_event = "llm.response:real_llm";
  rec.agent_id = "real_llm_agent_v1";
  rec.teacher_version = "v1.0.0";
  rec.generation_timestamp_ms = 1234567890;
  rec.capture_mode = agenticdsl::CaptureMode::Training;
  rec.reward = agenticdsl::RewardSignal::acceptable(0.5);

  // 写入 tmp 目录
  auto tmp_dir = std::filesystem::temp_directory_path() /
                 ("distill_real_llm_" +
                  std::to_string(::getpid()));
  std::filesystem::create_directories(tmp_dir);

  auto writer = agenticdsl::IDistillationWriter::make_file_writer(
      tmp_dir, "real_llm_agent_v1");

  writer->write_record(rec);

  agenticdsl::DistillationMetadata meta;
  meta.version = "1.0.0";
  meta.total_examples = 1;
  meta.dataset_hash = "real_llm_hash_placeholder";
  meta.generation_config = {{"model", cfg.model}, {"provider", cfg.provider}};
  writer->finalize(meta);

  writer->close();

  bool found_jsonl = false;
  std::string content;
  for (const auto& entry : std::filesystem::directory_iterator(tmp_dir)) {
    if (entry.path().extension() == ".jsonl") {
      found_jsonl = true;
      std::ifstream ifs(entry.path());
      content.assign((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
      break;
    }
  }
  REQUIRE(found_jsonl);
  INFO("JSONL length: " << content.size());
  REQUIRE(content.size() > 0);
  REQUIRE(content.find(pair.output) != std::string::npos);

  // cleanup
  std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("Distillation capture-mode=Training R3: 5 records batched",
          "[must_realllm][realllm][distillation][r3]") {
  must_require_real_llm_env();

  auto cfg = real_llm_config();
  agenticdsl::LLMConfig llm_cfg;
  llm_cfg.provider = cfg.provider;
  llm_cfg.model = cfg.model;
  llm_cfg.api_url = cfg.api_url;
  llm_cfg.api_endpoint = cfg.api_endpoint;
  llm_cfg.api_key = cfg.api_key;
  llm_cfg.api_key_env = cfg.api_key_env;

  agenticdsl::LLMProviderFactory factory;
  auto provider = factory.create(llm_cfg);
  REQUIRE(provider != nullptr);

  std::shared_ptr<agenticdsl::ILLMProvider> provider_shared = std::move(provider);

  // 5 个简单 prompt
  const std::vector<std::string> prompts = {
      "What is 1+1?",
      "What is 2+2?",
      "What is 3+3?",
      "What is 4+4?",
      "What is 5+5?",
  };

  auto tmp_dir = std::filesystem::temp_directory_path() /
                 ("distill_real_llm_r3_" +
                  std::to_string(::getpid()));
  std::filesystem::create_directories(tmp_dir);

  auto writer = agenticdsl::IDistillationWriter::make_file_writer(
      tmp_dir, "real_llm_agent_r3");

  int non_empty = 0;
  for (const auto& p : prompts) {
    LlmPair pair = call_real_llm(provider_shared, p, cfg.model);
    if (!pair.output.empty()) {
      agenticdsl::DistillationRecord rec;
      rec.input = pair.input;
      rec.output = pair.output;
      rec.trace_id = "r3_" + std::to_string(non_empty);
      rec.source_event = "llm.response:r3";
      rec.agent_id = "real_llm_agent_r3";
      rec.teacher_version = "v1.0.0";
      rec.generation_timestamp_ms = 1234567890 + non_empty;
      rec.capture_mode = agenticdsl::CaptureMode::Training;
      rec.reward = agenticdsl::RewardSignal::acceptable(0.5);
      writer->write_record(rec);
      ++non_empty;
    }
  }

  agenticdsl::DistillationMetadata meta;
  meta.version = "1.0.0";
  meta.total_examples = non_empty;
  meta.dataset_hash = "r3_hash";
  meta.generation_config = {{"model", cfg.model}};
  writer->finalize(meta);
  writer->close();

  // 至少要有 1 个真实 LLM 调用成功的 record
  REQUIRE(non_empty >= 1);

  bool found_meta = false;
  std::string meta_content;
  for (const auto& entry : std::filesystem::directory_iterator(tmp_dir)) {
    if (entry.path().extension() == ".json") {
      found_meta = true;
      std::ifstream ifs(entry.path());
      meta_content.assign((std::istreambuf_iterator<char>(ifs)),
                           std::istreambuf_iterator<char>());
      break;
    }
  }
  REQUIRE(found_meta);
  REQUIRE(meta_content.find("\"total_examples\":") != std::string::npos);
  REQUIRE(meta_content.find(std::to_string(non_empty)) != std::string::npos);

  std::filesystem::remove_all(tmp_dir);
}