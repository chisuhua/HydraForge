#ifndef AGENTICDSL_LLM_LLM_CONFIG_H
#define AGENTICDSL_LLM_LLM_CONFIG_H

// 文件头注释
// 功能描述：统一 LLM 配置结构（合并 LLMConfig + LLMParams 重复定义）
//          解决云端 LLM 与本地 LLM 适配器共享参数配置的问题
// 设计依据：ADR-0005（LLM 后端配置与工厂）、track-01-cloud-llm.md M1.1
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-07

#include <string>
#include <vector>
#include <optional>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace agenticdsl {

/**
 * @brief 统一 LLM 配置结构
 *
 * 合并原 LLMConfig（llm_adapter.h）与 LLMParams（llm_tool.h）的字段，
 * 覆盖本地 llama.cpp 与云端 OpenAI / Anthropic / DeepSeek 等多种 LLM 后端。
 *
 * 默认值与原 LLMConfig 保持一致，确保向后兼容。
 */
struct LLMConfig {
  // === 提供方标识 ===
  /// 提供方名称，如 "openai" / "anthropic" / "deepseek" / "local"
  std::string provider = "openai";

  // === 连接配置 ===
  /// API 基础 URL（云端默认 OpenAI，本地默认 llama.cpp server）
  std::string api_url = "https://api.openai.com/v1";
  /// API 端点路径（默认 /v1/chat/completions；本地服务可自定义）
  std::string api_endpoint = "/v1/chat/completions";
  /// 直填 API Key（最低优先级，仅作旧版兼容；不推荐在配置文件中明文保存）
  std::string api_key;
  /// 环境变量名，优先于 api_key / api_key_file
  std::optional<std::string> api_key_env;
  /// API Key 文件路径，env 未设置时备选
  std::optional<std::string> api_key_file;

  // === 模型配置 ===
  /// 模型名称（云端 "gpt-4o-mini" / 本地 "llama-3-8b"）
  std::string model = "gpt-4o-mini";
  /// 上下文窗口大小（本地 llama.cpp 适用）
  int n_ctx = 2048;
  /// 单次生成最大 token 数
  int max_tokens = 2048;

  // === 采样参数 ===
  /// 温度，范围 [0, 2]
  float temperature = 0.7f;
  /// nucleus sampling 阈值，范围 (0, 1]
  float top_p = 0.95f;
  /// 停止 token 列表
  std::vector<std::string> stop_tokens;

  // === 性能配置（本地 llama.cpp）===
  /// 推理线程数
  int n_threads = 4;
  /// min_p 采样阈值（llama.cpp 特有，保留兼容）
  float min_p = 0.05f;

  // === 云端专用配置 ===
  /// OpenAI 组织 ID（可选）
  std::optional<std::string> organization;
  /// HTTP 超时（秒）
  int timeout_seconds = 60;
  /// 网络错误时的最大重试次数
  int max_retries = 3;

  /**
   * @brief 解析并返回实际可用的 API Key
   *
   * 解析优先级：
   *  1. api_key_env 指定的环境变量
   *  2. api_key_file 指定的文件（首行非空内容）
   *  3. api_key 字段本身
   *
   * @return 解析得到的 API Key 字符串；解析失败时返回空字符串
   *
   * @note 日志中禁止打印此返回值（即使部分），遵循 ADR-0005 安全原则
   */
  std::string resolve_api_key() const {
    // 1. 环境变量优先
    if (api_key_env.has_value() && !api_key_env->empty()) {
      if (const char* env_val = std::getenv(api_key_env->c_str())) {
        if (env_val[0] != '\0') {
          return std::string(env_val);
        }
      }
    }

    // 2. 密钥文件备选
    if (api_key_file.has_value() && !api_key_file->empty()) {
      std::ifstream in(*api_key_file);
      if (in.is_open()) {
        std::string line;
        if (std::getline(in, line)) {
          // 去除首尾空白
          size_t start = line.find_first_not_of(" \t\r\n");
          size_t end = line.find_last_not_of(" \t\r\n");
          if (start != std::string::npos && end != std::string::npos) {
            return line.substr(start, end - start + 1);
          }
        }
      }
    }

    // 3. 直接字段兜底
    return api_key;
  }
};

} // namespace agenticdsl

#endif // AGENTICDSL_LLM_LLM_CONFIG_H
