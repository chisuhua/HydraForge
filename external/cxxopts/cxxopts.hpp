// cxxopts.hpp - Minimal stub for offline build
// Vendored from jarro2783/cxxopts v3.2.1
// License: MIT - see external/cxxopts/LICENSE

#ifndef CXXOPTS_HPP_INCLUDED
#define CXXOPTS_HPP_INCLUDED

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace cxxopts {

// Minimal OptionValue that supports as<T>()
class OptionValue {
public:
  bool bool_value = false;
  std::string string_value;

  template<typename T>
  T as() const;
};

template<>
inline bool OptionValue::as<bool>() const { return bool_value; }

template<>
inline std::string OptionValue::as<std::string>() const { return string_value; }

// cxxopts::value<T>() - minimal stub that signals a string value
inline std::shared_ptr<void> value() { return nullptr; }

class ParseResult {
public:
  std::unordered_map<std::string, OptionValue> values_;
  std::vector<std::string> positional_;

  OptionValue& operator[](const std::string& key) {
    return values_[key];
  }

  const OptionValue& operator[](const std::string& key) const {
    static OptionValue empty;
    auto it = values_.find(key);
    return (it != values_.end()) ? it->second : empty;
  }

  size_t count(const std::string& key) const {
    return values_.count(key);
  }
};

class Options {
public:
  std::string program_name;
  std::string program_description;
  std::vector<std::pair<std::string, bool>> specs_;  // spelling, is_string
  std::vector<std::pair<std::string, std::string>> spec_descriptions_;  // spelling, description
  std::vector<std::string> spec_value_names_;  // value name for string options
  std::map<std::string, std::string> long_opts_;
  std::map<char, std::string> short_opts_;

  Options(const std::string& name, const std::string& desc)
      : program_name(name), program_description(desc) {}

  class Adder {
  public:
    explicit Adder(Options& opt) : options_(opt) {}
    Options& options_;

    Adder& operator()(const std::string& spelling, const std::string& description) {
      options_.add_spec(spelling, description, false, "");
      return *this;
    }
    Adder& operator()(const std::string& spelling, const std::string& description,
                      const std::shared_ptr<void>&, const std::string& value_name) {
      options_.add_spec(spelling, description, true, value_name);
      return *this;
    }
  };

  Adder add_options() { return Adder(*this); }

  void add_spec(const std::string& spelling, const std::string& description,
                bool is_string, const std::string& value_name) {
    size_t comma = spelling.find(',');
    std::string long_name_for_spec = spelling;
    if (comma != std::string::npos) {
      std::string first = spelling.substr(0, comma);
      std::string second = spelling.substr(comma + 1);
      if (first.size() == 2 && first[0] == '-') {
        short_opts_[first[1]] = second;
        long_opts_["--" + second] = second;
        long_name_for_spec = second;
      } else if (second.size() == 2 && second[0] == '-') {
        short_opts_[second[1]] = first;
        long_opts_["--" + first] = first;
        long_name_for_spec = first;
      } else if (first.size() == 1) {
        short_opts_[first[0]] = second;
        long_opts_["--" + second] = second;
        long_name_for_spec = second;
      } else {
        short_opts_[second[0]] = first;
        long_opts_["--" + first] = first;
        long_name_for_spec = first;
      }
    } else {
      parse_spelling(spelling);
      long_name_for_spec = spelling;
    }
    specs_.push_back({long_name_for_spec, is_string});
    spec_descriptions_.push_back({long_name_for_spec, description});
    spec_value_names_.push_back(value_name);
  }

  void parse_spelling(const std::string& spelling) {
    std::string s = spelling;
    std::string long_opt;
    char short_opt = 0;
    size_t comma = s.find(',');
    if (comma != std::string::npos) {
      std::string first = s.substr(0, comma);
      std::string second = s.substr(comma + 1);
      if (first.size() == 2 && first[0] == '-') {
        short_opt = first[1];
        long_opt = second;
      } else if (second.size() == 2 && second[0] == '-') {
        short_opt = second[1];
        long_opt = first;
      } else {
        long_opt = first;
      }
    } else if (s.size() > 2 && s[0] == '-' && s[1] == '-') {
      long_opt = s;
    } else if (s.size() == 2 && s[0] == '-') {
      short_opt = s[1];
    } else {
      long_opt = std::string("--") + s;
    }
    if (!long_opt.empty()) long_opts_[long_opt] = spelling;
    if (short_opt) short_opts_[short_opt] = spelling;
  }

  ParseResult parse(int argc, char* argv[]) {
    ParseResult result;
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      std::string key;
      std::string value;

      if (arg == "--help" || arg == "-h") {
        result.values_["help"].bool_value = true;
        continue;
      }

      size_t eq_pos = arg.find('=');
      if (eq_pos != std::string::npos) {
        key = arg.substr(0, eq_pos);
        value = arg.substr(eq_pos + 1);
      } else if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
        key = arg;
        if (i + 1 < argc && argv[i + 1][0] != '-') {
          value = argv[++i];
        }
      } else if (arg.size() == 2 && arg[0] == '-') {
        key = std::string("-") + arg[1];
        if (i + 1 < argc && argv[i + 1][0] != '-') {
          value = argv[++i];
        }
      } else {
        result.positional_.push_back(arg);
        continue;
      }

      auto it = long_opts_.find(key);
      if (it != long_opts_.end()) {
        key = it->second;
      } else if (key.size() == 2 && key[0] == '-') {
        auto sit = short_opts_.find(key[1]);
        if (sit != short_opts_.end()) key = sit->second;
      } else {
        throw std::runtime_error("unrecognized option: " + key);
      }

      bool found = false;
      for (size_t idx = 0; idx < specs_.size(); ++idx) {
        if (specs_[idx].first == key) {
          found = true;
          if (!specs_[idx].second) {
            result.values_[key].bool_value = true;
          } else {
            if (value.empty()) {
              throw std::runtime_error("option '" + key + "' argument missing");
            }
            result.values_[key].string_value = value;
          }
          break;
        }
      }
      if (!found) {
        throw std::runtime_error("unrecognized option: " + key);
      }
    }
    return result;
  }

  std::string help() const {
    std::ostringstream oss;
    oss << "Usage: " << program_name << " [options]\n";
    oss << "\n" << program_description << "\n\n";
    oss << "Options:\n";
    for (size_t idx = 0; idx < specs_.size(); ++idx) {
      const std::string& spelling = specs_[idx].first;
      const std::string& description = spec_descriptions_[idx].second;
      const std::string& value_name = spec_value_names_[idx];
      std::string opt_str;
      auto comma = spelling.find(',');
      std::string long_part;
      std::string short_part;
      if (comma != std::string::npos) {
        short_part = spelling.substr(0, comma);
        long_part = spelling.substr(comma + 1);
      } else {
        long_part = spelling;
      }
      if (!short_part.empty()) opt_str += short_part + ", ";
      if (long_part.size() > 2 && long_part.substr(0, 2) == "--") {
        opt_str += long_part;
      } else if (!long_part.empty()) {
        opt_str += "--" + long_part;
      }
      if (!value_name.empty()) opt_str += " <" + value_name + ">";
      oss << "  " << opt_str << "\n";
      if (!description.empty()) oss << "    " << description << "\n";
    }
    oss << "  --help\n    Show generated usage";
    return oss.str();
  }
};

}  // namespace cxxopts

#endif  // CXXOPTS_HPP_INCLUDED
