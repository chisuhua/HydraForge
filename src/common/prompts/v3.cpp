// ADR-0074 D-3 + D-5 — V3 = V2 + two-stage 注入 (SystemFirst → UserSecond)
// Stage 1: system 消息携带 JSON schema 约束
// Stage 2: user 消息携带 few-shot 示例 + 用户请求
// Risk-3 缓解: 总词数超过阈值时 stderr 警告
#include "v3.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <random>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "v1_inline.h"

namespace fs = std::filesystem;

namespace agenticdsl::prompts {

namespace {

constexpr int kMaxFewShots = 5;
constexpr int kTokenWarnThreshold = 8000;

// 粗略 token 计数: 以空格分词 (Batch 2 决议: inline, 不单独抽 token_counter.h)
int rough_word_count(const std::string& s) {
  if (s.empty()) return 0;
  int n = 1;
  for (char c : s) {
    if (c == ' ') n++;
  }
  return n;
}

std::vector<std::string> load_random_fewshots(int max_count) {
  std::vector<std::string> blocks;
  fs::path dir = "lib/prompts/fewshot";
  if (!fs::exists(dir)) return blocks;

  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(dir)) {
    if (e.path().extension() == ".yaml") files.push_back(e.path());
  }

  std::mt19937 rng(42);  // 确定性 seed, 与 V2 保持一致
  std::shuffle(files.begin(), files.end(), rng);

  for (size_t i = 0; i < files.size() && static_cast<int>(blocks.size()) < max_count; ++i) {
    YAML::Node node = YAML::LoadFile(files[i].string());
    std::string block = "input: " + node["input"].as<std::string>()
                      + "\noutput: " + node["output"].as<std::string>();
    blocks.push_back(std::move(block));
  }
  return blocks;
}

}  // namespace

PromptPayload V3TwoStagePromptBuilder::build(const std::string& user_input) const {
  // Stage 1: SystemFirst — schema 约束 (强制先行)
  std::string system = "JSON Schema: " + v1_inline::build_schema_constraint()
                     + "\nProperties: result (string), permissions (array)";

  // Stage 2: UserSecond — few-shot 示例 + 实际用户请求
  auto fewshot_blocks = load_random_fewshots(kMaxFewShots);
  std::string user_content = "Few-shot examples:\n";
  for (const auto& b : fewshot_blocks) {
    user_content += b + "\n\n";
  }
  user_content += "User request: " + user_input;

  // Risk-3 缓解: token 超阈值 stderr 警告
  int total_words = rough_word_count(system) + rough_word_count(user_content);
  if (total_words > kTokenWarnThreshold) {
    std::fprintf(stderr, "[v3-warning] prompt %d words exceeds %d threshold\n",
                 total_words, kTokenWarnThreshold);
  }

  PromptPayload p;
  p.add_system(system);      // Stage 1
  p.add_user(user_content);  // Stage 2
  return p;
}

}  // namespace agenticdsl::prompts
