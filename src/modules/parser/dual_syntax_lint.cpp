// src/modules/parser/dual_syntax_lint.cpp
// C7: ADR-0072 D5 双语法共存期 lint 工具 — 独立可执行 `dual_syntax_lint`
// 检测 legacy 语法 → line-level warning + 修复建议 + # lint:disable dual-syntax 豁免 + 新文件 heuristic
// 设计依据: openspec/changes/from-roadmap-phase-6c-execution-dsl/specs/dsl-extensions/spec.md
// 作者: Solo Dev
// 日期: 2026-09-02

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// 修复: 编译期常量 D2/D3 ship timestamp (YYYY-MM-DD)
constexpr const char* kDefaultShipTimestamp = "2026-09-02";

// 警告条目
struct Warning {
  std::string file_path;
  size_t line_number;
  std::string legacy_syntax;
  std::string suggestion;
};

// 解析 ISO 日期 (YYYY-MM-DD) 为 time_t
std::optional<std::time_t> parse_iso_date(const std::string& iso) {
  std::tm tm{};
  std::istringstream ss(iso);
  ss >> std::get_time(&tm, "%Y-%m-%d");
  if (ss.fail()) return std::nullopt;
  tm.tm_isdst = 0;
  return std::mktime(&tm);
}

// 获取文件 mtime
std::optional<std::time_t> get_file_mtime(const fs::path& path) {
  try {
    auto ftime = fs::last_write_time(path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// 执行 git log 获取文件最近提交时间 (ISO 8601), 无历史/失败返回 nullopt
std::optional<std::time_t> get_git_commit_time(const fs::path& path) {
  std::string cmd = "git log -1 --format=%cI -- " + path.string() + " 2>/dev/null";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return std::nullopt;
  char buffer[128];
  std::string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  pclose(pipe);
  result.erase(result.find_last_not_of(" \n\r\t") + 1); // trim
  if (result.empty()) return std::nullopt;
  // 解析 ISO 8601 如 "2026-08-25T10:30:00+08:00"
  std::tm tm{};
  std::istringstream ss(result.substr(0, 19)); // 仅取日期时间部分
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  if (ss.fail()) return std::nullopt;
  tm.tm_isdst = 0;
  return std::mktime(&tm);
}

// 判断是否为新文件 (mtime >= ship_ts AND git commit >= ship_ts 或无 git 历史)
bool is_new_file(const fs::path& path, std::time_t ship_ts) {
  auto mtime = get_file_mtime(path);
  if (!mtime || *mtime < ship_ts) return false; // mtime 旧

  auto git_time = get_git_commit_time(path);
  if (!git_time) return true; // 无 git 历史 -> 视为新文件
  return *git_time >= ship_ts;
}

// 检测 legacy 语法并生成警告
// pattern 1: -> identifier (arrow reference, 无 $ 前缀)  -> 建议 $identifier
// pattern 2: type: fork / type: join  -> 建议 exec: [...]
std::vector<Warning> detect_legacy_in_line(const std::string& file_path,
                                           const std::string& line,
                                           size_t line_number) {
  std::vector<Warning> warnings;
  static const std::regex arrow_ref(R"(->\s*([A-Za-z_][A-Za-z0-9_]*))"); // -> name
  static const std::regex unicode_arrow_ref(R"(→\s*([A-Za-z_][A-Za-z0-9_]*))"); // → name
  static const std::regex fork_join_type(R"(type:\s*(fork|join)\b)");

  // Pattern 1: -> identifier (legacy edge reference)
  std::smatch match;
  std::string line_copy = line;
  while (std::regex_search(line_copy, match, arrow_ref)) {
    std::string name = match[1];
    warnings.push_back({file_path, line_number, "-> " + name, "$" + name});
    line_copy = match.suffix().str();
  }
  // Pattern 1b: → identifier (unicode arrow)
  line_copy = line;
  while (std::regex_search(line_copy, match, unicode_arrow_ref)) {
    std::string name = match[1];
    warnings.push_back({file_path, line_number, "→ " + name, "$" + name});
    line_copy = match.suffix().str();
  }

  // Pattern 2: type: fork / type: join
  line_copy = line;
  while (std::regex_search(line_copy, match, fork_join_type)) {
    std::string type = match[1];
    warnings.push_back({file_path, line_number, "type: " + type, "exec: [...]"});
    line_copy = match.suffix().str();
  }

  return warnings;
}

// 检查某行是否包含 lint:disable 注释
bool has_lint_disable(const std::string& line) {
  static const std::regex disable_re(R"(#\s*lint:disable\s+dual-syntax\b)");
  return std::regex_search(line, disable_re);
}

// Lint 单个文件
std::vector<Warning> lint_file(const fs::path& path,
                               bool include_historical,
                               std::time_t ship_ts) {
  std::vector<Warning> all_warnings;

  if (!include_historical && !is_new_file(path, ship_ts)) {
    return {}; // 历史文件跳过
  }

  std::ifstream file(path);
  if (!file) {
    std::cerr << "WARNING: cannot open " << path << "\n";
    return {};
  }

  std::string line;
  size_t line_number = 0;
  bool next_line_suppressed = false; // 前一行有 lint:disable -> 抑制本行
  while (std::getline(file, line)) {
    ++line_number;

    bool suppressed = next_line_suppressed;
    next_line_suppressed = false;

    // 检查本行是否有 lint:disable (影响下一行)
    if (has_lint_disable(line)) {
      next_line_suppressed = true;
    }

    if (suppressed) continue;

    auto warnings = detect_legacy_in_line(path.string(), line, line_number);
    all_warnings.insert(all_warnings.end(), warnings.begin(), warnings.end());
  }

  return all_warnings;
}

void print_usage(const char* prog) {
  std::cout << "Usage: " << prog << " [options] <file...>\n"
            << "Options:\n"
            << "  --include-historical    Lint all files (default: skip historical files)\n"
            << "  --ship-timestamp <date> Override D2/D3 ship timestamp (YYYY-MM-DD)\n"
            << "  --help, -h              Show this help\n"
            << "\n"
            << "Exit code: always 0 (warnings do not block)\n"
            << "Warning format: <file>:<line>: warning: legacy syntax '<match>'; consider '<suggestion>'\n";
}

} // namespace

int main(int argc, char** argv) {
  bool include_historical = false;
  std::time_t ship_ts = 0;

  // 解析 ship timestamp
  auto ship_ts_opt = parse_iso_date(kDefaultShipTimestamp);
  if (!ship_ts_opt) {
    std::cerr << "ERROR: internal default ship timestamp invalid\n";
    return 1;
  }
  ship_ts = *ship_ts_opt;

  std::vector<fs::path> files;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--include-historical") {
      include_historical = true;
    } else if (a == "--ship-timestamp" && i + 1 < argc) {
      auto ts = parse_iso_date(argv[++i]);
      if (!ts) {
        std::cerr << "ERROR: invalid --ship-timestamp format (expected YYYY-MM-DD)\n";
        return 1;
      }
      ship_ts = *ts;
    } else if (a == "--help" || a == "-h") {
      print_usage(argv[0]);
      return 0;
    } else {
      files.push_back(a);
    }
  }

  if (files.empty()) {
    std::cerr << "ERROR: no files specified\n";
    print_usage(argv[0]);
    return 1;
  }

  size_t total_warnings = 0;
  for (const auto& path : files) {
    auto warnings = lint_file(path, include_historical, ship_ts);
    for (const auto& w : warnings) {
      std::cout << w.file_path << ":" << w.line_number
                << ": warning: legacy syntax '" << w.legacy_syntax
                << "'; consider '" << w.suggestion << "'\n";
      ++total_warnings;
    }
  }

  return 0; // Always exit 0 per spec
}