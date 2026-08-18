// ADR-0074 D-3 — V2 = V1 + few-shot 注入 (≤ 5 examples, RNG seed=42 确定性)
#include "v2.h"

#include <algorithm>
#include <filesystem>
#include <random>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "v1_inline.h"

namespace fs = std::filesystem;

namespace agenticdsl::prompts {

namespace {

constexpr int kMaxFewShots = 5;

struct FewShotExample {
  std::string dimension;
  std::string input;
  std::string output;
};

std::vector<FewShotExample> load_fewshots(int max_count = kMaxFewShots) {
  std::vector<FewShotExample> examples;
  fs::path dir = "lib/prompts/fewshot";
  if (!fs::exists(dir)) return examples;

  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(dir)) {
    if (e.path().extension() == ".yaml") files.push_back(e.path());
  }

  std::mt19937 rng(42);
  std::shuffle(files.begin(), files.end(), rng);

  for (size_t i = 0; i < files.size() && static_cast<int>(examples.size()) < max_count; ++i) {
    YAML::Node node = YAML::LoadFile(files[i].string());
    FewShotExample ex;
    ex.dimension = node["dimension"].as<std::string>();
    ex.input = node["input"].as<std::string>();
    ex.output = node["output"].as<std::string>();
    examples.push_back(std::move(ex));
  }
  return examples;
}

}  // namespace

PromptPayload V2FewShotPromptBuilder::build(const std::string& user_input) const {
  PromptPayload p;
  auto examples = load_fewshots();

  std::string few_shots_block = "Few-shot examples:\n";
  for (const auto& ex : examples) {
    few_shots_block += "input: " + ex.input + "\n";
    few_shots_block += "output: " + ex.output + "\n\n";
  }

  std::string system = "JSON Schema: " + v1_inline::build_schema_constraint() + "\n\n"
                     + few_shots_block
                     + "User request: " + user_input;
  p.add_system(system);
  return p;
}

}  // namespace agenticdsl::prompts
