// src/modules/parser/node_factory.cpp
// NodeFactoryRegistry 实现 + 11 个 NodeType 工厂函数 + 静态注册 (Sprint 6 P2-6)
// 设计依据: openspec/changes/tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md
#include "agenticdsl/parser/node_factory.h"

#include "core/types/node.h"
#include "core/types/resource.h"
#include "common/llm/llm_types.h"
#include "common/utils/parser_utils.h"

#include <stdexcept>

namespace agenticdsl {
namespace {

struct NodeContext {
  std::vector<NodePath> next_paths;
  nlohmann::json metadata = nlohmann::json::object();
  std::optional<std::string> signature;
  std::vector<std::string> permissions;
};

NodeContext parse_context(const nlohmann::json& json) {
  NodeContext ctx;
  if (json.contains("next")) {
    const auto& next = json["next"];
    if (next.is_string()) {
      ctx.next_paths.push_back(next.get<std::string>());
    } else if (next.is_array()) {
      for (const auto& np : next) {
        ctx.next_paths.push_back(np.get<std::string>());
      }
    }
  }
  ctx.metadata = json.value("metadata", nlohmann::json::object());
  // ADR-0072 D4: backend: 字段接入 (per ADR-0075 EnvBackend) — 节点声明执行环境
  if (json.contains("backend")) {
    ctx.metadata["backend"] = json["backend"];
  }
  // env: 作为 env_vars: 旧别名（向后兼容既有 DSL 写法）
  if (json.contains("env")) {
    ctx.metadata["env_vars"] = json["env"];
  }
  // env_vars: 规范名，优先级覆盖 env:（两者并存时 env_vars 胜出）
  if (json.contains("env_vars")) {
    ctx.metadata["env_vars"] = json["env_vars"];
  }
  if (json.contains("wait_for")) {
    ctx.metadata["wait_for"] = json["wait_for"];
  }
  if (json.contains("signature")) {
    ctx.signature = json["signature"].get<std::string>();
  }
  if (json.contains("permissions") && json["permissions"].is_array()) {
    for (const auto& p : json["permissions"]) {
      if (p.is_string()) ctx.permissions.push_back(p.get<std::string>());
    }
  }
  return ctx;
}

void apply_context(Node& node, const NodeContext& ctx) {
  node.metadata = ctx.metadata;
  node.signature = ctx.signature;
  node.permissions = ctx.permissions;
}

ResourceType parse_resource_type(const std::string& type_str) {
  if (type_str == "file") return ResourceType::FILE;
  if (type_str == "postgres") return ResourceType::POSTGRES;
  if (type_str == "mysql") return ResourceType::MYSQL;
  if (type_str == "sqlite") return ResourceType::SQLITE;
  if (type_str == "api_endpoint") return ResourceType::API_ENDPOINT;
  if (type_str == "vector_store") return ResourceType::VECTOR_STORE;
  if (type_str == "custom") return ResourceType::CUSTOM;
  throw std::runtime_error("Unknown resource_type '" + type_str + "'");
}

std::vector<std::string> parse_output_keys_local(const nlohmann::json& j, const NodePath& path) {
  if (!j.contains("output_keys")) {
    throw std::runtime_error("Missing 'output_keys' in node: " + path);
  }
  const auto& ok = j["output_keys"];
  if (ok.is_string()) return {ok.get<std::string>()};
  if (ok.is_array()) {
    std::vector<std::string> keys;
    for (const auto& k : ok) keys.push_back(k.get<std::string>());
    return keys;
  }
  throw std::runtime_error("'output_keys' must be string or array in node: " + path);
}

std::unique_ptr<Node> make_start(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  auto node = std::make_unique<StartNode>(path, std::move(ctx.next_paths));
  apply_context(*node, ctx);
  return node;
}

std::unique_ptr<Node> make_end(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  auto node = std::make_unique<EndNode>(path);
  node->next = std::move(ctx.next_paths);
  apply_context(*node, ctx);
  return node;
}

std::unique_ptr<Node> make_assign(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::unordered_map<std::string, std::string> assign;
  if (j.contains("assign") && j["assign"].is_object()) {
    for (auto& [key, value] : j["assign"].items()) {
      if (value.is_string()) assign[key] = value.get<std::string>();
      else if (value.is_number_integer()) assign[key] = std::to_string(value.get<long long>());
      else if (value.is_number_float()) assign[key] = std::to_string(value.get<double>());
      else if (value.is_boolean()) assign[key] = value.get<bool>() ? "true" : "false";
      else assign[key] = value.dump();
    }
  }
  auto node = std::make_unique<AssignNode>(path, std::move(assign), std::move(ctx.next_paths));
  apply_context(*node, ctx);
  return node;
}

namespace {

LLMParams parse_llm_params(const nlohmann::json& j) {
  LLMParams params;
  if (j.contains("llm_params") && j["llm_params"].is_object()) {
    const auto& p = j["llm_params"];
    if (p.contains("temperature")) params.temperature = p["temperature"].get<float>();
    if (p.contains("max_tokens")) params.max_tokens = p["max_tokens"].get<int>();
    if (p.contains("top_p")) params.top_p = p["top_p"].get<float>();
    if (p.contains("n_ctx")) params.n_ctx = p["n_ctx"].get<int>();
    if (p.contains("n_threads")) params.n_threads = p["n_threads"].get<int>();
    if (p.contains("model")) params.model = p["model"].get<std::string>();
  }
  return params;
}

std::unique_ptr<Node> make_dsl_or_llm_call(const NodePath& path, const nlohmann::json& j,
                                            const std::string& default_llm_tool_name) {
  auto ctx = parse_context(j);
  std::string prompt = j.at("prompt_template").get<std::string>();
  std::string llm_tool_name = j.contains("llm_tool_name")
      ? j.at("llm_tool_name").get<std::string>()
      : default_llm_tool_name;
  auto llm_params = parse_llm_params(j);
  auto output_keys = parse_output_keys_local(j, path);
  auto node = std::make_unique<DSLNode>(path, std::move(prompt), std::move(llm_tool_name),
                                        std::move(llm_params), std::move(output_keys),
                                        std::move(ctx.next_paths));
  apply_context(*node, ctx);
  return node;
}

} // namespace

std::unique_ptr<Node> make_dsl_call(const NodePath& path, const nlohmann::json& j) {
  return make_dsl_or_llm_call(path, j, /*default_llm_tool_name=*/"");
}

std::unique_ptr<Node> make_llm_call(const NodePath& path, const nlohmann::json& j) {
  return make_dsl_or_llm_call(path, j, /*default_llm_tool_name=*/"llama-default");
}

std::unique_ptr<Node> make_tool_call(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::string tool = j.at("tool").get<std::string>();
  auto output_keys = parse_output_keys_local(j, path);
  std::unordered_map<std::string, std::string> args;
  if (j.contains("arguments") && j["arguments"].is_object()) {
    for (auto& [key, value] : j["arguments"].items()) {
      if (value.is_string()) args[key] = value.get<std::string>();
      else if (value.is_number_integer()) args[key] = std::to_string(value.get<long long>());
      else if (value.is_number_float()) args[key] = std::to_string(value.get<double>());
      else if (value.is_boolean()) args[key] = value.get<bool>() ? "true" : "false";
      else if (value.is_object() || value.is_array()) args[key] = value.dump();
      else throw std::runtime_error("Argument '" + key + "' has unsupported type");
    }
  }
  auto node = std::make_unique<ToolCallNode>(path, std::move(tool), std::move(args),
                                             std::move(output_keys), std::move(ctx.next_paths));
  apply_context(*node, ctx);
  return node;
}

std::unique_ptr<Node> make_resource(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::string rtype_str = j.at("resource_type").get<std::string>();
  ResourceType rtype = parse_resource_type(rtype_str);
  std::string uri = j.at("uri").get<std::string>();
  std::string scope = j.value("scope", std::string("global"));
  auto node = std::make_unique<ResourceNode>(path, rtype, std::move(uri), std::move(scope),
                                             ctx.metadata);
  node->signature = ctx.signature;
  node->permissions = ctx.permissions;
  return node;
}

std::unique_ptr<Node> make_fork(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::vector<NodePath> branches;
  if (j.contains("fork") && j["fork"].contains("branches")) {
    const auto& bj = j["fork"]["branches"];
    if (bj.is_array()) {
      for (const auto& b : bj) branches.push_back(b.get<std::string>());
    } else if (bj.is_string()) {
      branches.push_back(bj.get<std::string>());
    }
  }
  auto node = std::make_unique<ForkNode>(path, std::move(branches), std::move(ctx.next_paths));
  apply_context(*node, ctx);
  return node;
}

std::unique_ptr<Node> make_join(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::vector<NodePath> deps;
  std::string strategy = "error_on_conflict";
  if (j.contains("join")) {
    const auto& jo = j["join"];
    if (jo.contains("wait_for")) {
      const auto& dj = jo["wait_for"];
      if (dj.is_array()) {
        for (const auto& d : dj) deps.push_back(d.get<std::string>());
      } else if (dj.is_string()) {
        deps.push_back(dj.get<std::string>());
      }
    }
    if (jo.contains("merge_strategy")) {
      strategy = jo["merge_strategy"].get<std::string>();
    }
  }
  auto node = std::make_unique<JoinNode>(path, std::move(deps), std::move(strategy),
                                         std::move(ctx.next_paths));
  apply_context(*node, ctx);
  return node;
}

std::unique_ptr<Node> make_generate_subgraph(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::string prompt = j.at("prompt_template").get<std::string>();
  auto output_keys = parse_output_keys_local(j, path);
  std::string sig_validation = j.value("signature_validation", std::string("strict"));
  std::optional<NodePath> on_violation;
  if (j.contains("on_signature_violation") && j["on_signature_violation"].is_string()) {
    on_violation = j["on_signature_violation"].get<std::string>();
  }
  auto node = std::make_unique<GenerateSubgraphNode>(path, std::move(prompt),
                                                     std::move(output_keys),
                                                     std::move(ctx.next_paths));
  node->signature_validation = sig_validation;
  node->on_signature_violation = on_violation;
  apply_context(*node, ctx);
  return node;
}

std::unique_ptr<Node> make_assert(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::string condition = j.at("condition").get<std::string>();
  std::optional<NodePath> on_failure;
  if (j.contains("on_failure") && j["on_failure"].is_string()) {
    on_failure = j["on_failure"].get<std::string>();
  }
  auto node = std::make_unique<AssertNode>(path, std::move(condition), on_failure,
                                           std::move(ctx.next_paths));
  apply_context(*node, ctx);
  return node;
}

// C12 Phase 5 Stage 1 Step 2 §2: YieldNode factory
// 解析 type:yield 节点 + yield_value/mode/stop_path 字段
// OpenSpec change 2026-07-03-phase5-stage1-step2-yield-stream §2
YieldMode parse_yield_mode(const std::string& s, const NodePath& path) {
  std::string lower = s;
  for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (lower == "next") return YieldMode::NEXT;
  if (lower == "continue") return YieldMode::CONTINUE;
  if (lower == "stop") return YieldMode::STOP;
  throw std::runtime_error("YieldNode '" + path + "': invalid mode '" + s +
                           "', expected next|continue|stop");
}

std::unique_ptr<Node> make_yield(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::string yield_value = j.value("yield_value", std::string{});
  YieldMode mode = YieldMode::NEXT;
  if (j.contains("mode") && j["mode"].is_string()) {
    mode = parse_yield_mode(j["mode"].get<std::string>(), path);
  }
  NodePath stop_path = j.value("stop_path", std::string{});
  auto node = std::make_unique<YieldNode>(path, std::move(ctx.next_paths),
                                          std::move(ctx.metadata), ctx.signature,
                                          std::move(ctx.permissions),
                                          std::move(yield_value), mode,
                                          std::move(stop_path));
  return node;
}

}  // namespace

void NodeFactoryRegistry::register_factory(const std::string& type_name, Factory factory) {
  std::unique_lock lock(mutex_);
  factories_[type_name] = std::move(factory);
}

std::unique_ptr<Node> NodeFactoryRegistry::create(const std::string& type_name,
                                                  const NodePath& path,
                                                  const nlohmann::json& node_json) const {
  std::shared_lock lock(mutex_);
  auto it = factories_.find(type_name);
  if (it == factories_.end()) {
    return nullptr;  // Unknown type (旧 if-else 行为, spec §3.3.3)
  }
  return it->second(path, node_json);
}

bool NodeFactoryRegistry::has_factory(const std::string& type_name) const {
  std::shared_lock lock(mutex_);
  return factories_.count(type_name) > 0;
}

size_t NodeFactoryRegistry::size() const {
  std::shared_lock lock(mutex_);
  return factories_.size();
}

NodeFactoryRegistry& NodeFactoryRegistry::global() {
  static NodeFactoryRegistry* instance = []() {
    auto* r = new NodeFactoryRegistry();
    r->register_factory("start", make_start);
    r->register_factory("end", make_end);
    r->register_factory("assign", make_assign);
    r->register_factory("dsl_call", make_dsl_call);
    r->register_factory("llm_call", make_llm_call);
    r->register_factory("tool_call", make_tool_call);
    r->register_factory("resource", make_resource);
    r->register_factory("fork", make_fork);
    r->register_factory("join", make_join);
    r->register_factory("generate_subgraph", make_generate_subgraph);
    r->register_factory("assert", make_assert);
    r->register_factory("yield", make_yield);
    return r;
  }();
  return *instance;
}

}  // namespace agenticdsl
