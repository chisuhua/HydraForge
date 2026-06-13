// include/agenticdsl/contract/iparser.h
// 文件头注释
// 功能描述：DSL 解析器抽象接口（ADR-0019 §1.4 + plan Stage 4）。
//          Markdown DSL → ParsedGraph (DAG nodes + edges) 的转换器抽象。
// 设计依据：ADR-0019 §1.4（跨模块耦合识别）+ ADR-0031（IExecutionPolicy 一致性）。
// 作者：AgenticDSL Stage 4
// 最后修改日期：2026-06-12
#pragma once

#include <filesystem>
#include <string>

namespace agenticdsl {

// 前置声明：ParsedGraph 的完整定义在 src/core/types/node.h。
// 此处不引入 core/types 以保持 contract 层的最小依赖面。
struct ParsedGraph;

/**
 * @brief DSL 解析器抽象接口
 *
 * 引擎只依赖此接口，不依赖具体 MarkdownParser 类。
 *
 * - MVP 实现: agenticdsl::contract::MarkdownParser
 *   （位置：include/agenticdsl/contract/markdown_parser.h，由 Task 17 引入；
 *    原 modules/parser/markdown_parser.h 重命名/迁移在后续 Task 完成）
 *
 * 备注：当前 MarkdownParser::parse_from_string() 返回
 * `std::vector<ParsedGraph>`（一个 markdown 文档可包含多个子图），而本接口
 * 设计为单 ParsedGraph。Task 17 在迁移实现类时需做适配（常见做法：在实现
 * 内部将 vector 转单值或显式选取主图 path="/main"）。
 */
class IParser {
 public:
  virtual ~IParser() = default;

  /**
   * @brief 解析 Markdown DSL 字符串为 ParsedGraph
   * @param markdown DSL 源文本
   * @return 解析后的图结构（含节点列表、依赖边、metadata）
   */
  virtual ParsedGraph parse(const std::string& markdown) = 0;

  /**
   * @brief 从文件系统读取并解析 Markdown DSL 文件
   * @param p DSL 文件路径（UTF-8）
   * @return 解析后的图结构
   */
  virtual ParsedGraph parse_file(const std::filesystem::path& p) = 0;
};

} // namespace agenticdsl