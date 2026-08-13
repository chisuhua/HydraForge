// include/agenticdsl/tools/schema_generation.h
// ADR-0073 D4: C++ type reflection to JSON Schema 2020-12
#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include <map>

namespace agenticdsl {

// ADR-0073 Table: C++ Type → JSON Schema 2020-12
// string → string, int/long/size_t → integer, float/double → number
// bool → boolean, vector<T> → array, optional<T> → (type of T)
// map<string, T> → object, enum class → string (enum values)
// struct → object (nested properties)

template<typename T>
struct SchemaGenerator {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "object";
    schema["additionalProperties"] = true;
    return schema;
  }
};

// string → {type: string}
template<>
struct SchemaGenerator<std::string> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "string";
    return schema;
  }
};

// int → {type: integer}
template<>
struct SchemaGenerator<int> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "integer";
    return schema;
  }
};

// long → {type: integer}
template<>
struct SchemaGenerator<long> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "integer";
    return schema;
  }
};

// size_t → {type: integer}
template<>
struct SchemaGenerator<size_t> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "integer";
    return schema;
  }
};

// float → {type: number}
template<>
struct SchemaGenerator<float> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "number";
    return schema;
  }
};

// double → {type: number}
template<>
struct SchemaGenerator<double> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "number";
    return schema;
  }
};

// bool → {type: boolean}
template<>
struct SchemaGenerator<bool> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "boolean";
    return schema;
  }
};

// std::optional<T> → schema of T (strip optional wrapper)
template<typename T>
struct SchemaGenerator<std::optional<T>> {
  static nlohmann::json to_schema() {
    return SchemaGenerator<T>::to_schema();
  }
};

// std::vector<T> → {type: array, items: schema of T}
template<typename T>
struct SchemaGenerator<std::vector<T>> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "array";
    schema["items"] = SchemaGenerator<T>::to_schema();
    return schema;
  }
};

// std::map<std::string, T> → {type: object, additionalProperties: schema of T}
template<typename T>
struct SchemaGenerator<std::map<std::string, T>> {
  static nlohmann::json to_schema() {
    nlohmann::json schema;
    schema["type"] = "object";
    schema["additionalProperties"] = SchemaGenerator<T>::to_schema();
    return schema;
  }
};

// Helper to generate full JSON Schema 2020-12 document with $schema and title
inline nlohmann::json make_input_schema(const std::string& title,
                                        const nlohmann::json& schema) {
  nlohmann::json doc;
  doc["$schema"] = "https://json-schema.org/draft/2020-12/schema";
  doc["title"] = title;
  if (schema.contains("type")) {
    for (const auto& key : {"type", "properties", "required", "items",
                             "additionalProperties", "enum", "const"}) {
      if (schema.contains(key)) {
        doc[key] = schema[key];
      }
    }
  } else {
    doc = schema;
  }
  return doc;
}

}  // namespace agenticdsl
