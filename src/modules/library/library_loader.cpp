// modules/library/src/library_loader.cpp
#include "library_loader.h"
#include "common/log/log.h"
#include "modules/parser/markdown_parser.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace agenticdsl {

StandardLibraryLoader::StandardLibraryLoader()
    : parser_(std::make_unique<MarkdownParser>()) {}

StandardLibraryLoader::~StandardLibraryLoader() = default;

StandardLibraryLoader& StandardLibraryLoader::instance() {
    static StandardLibraryLoader loader;
    static bool initialized = false;
    if (!initialized) {
        loader.load_builtin_libraries();
        // Optional: loader.load_from_directory("./lib");
        initialized = true;
    }
    return loader;
}

void StandardLibraryLoader::load_builtin_libraries() {
    libraries_.push_back({
         "/lib/utils/noop",
         "() -> void",
         nlohmann::json::parse(R"({"type": "object"})"),
         {},
         true
    });

    libraries_.push_back({
         "/lib/math/add",
         "(a: number, b: number) -> {sum: number}",
         nlohmann::json::parse(R"({"type": "object", "properties": {"sum": {"type": "number"}}})"),
         {},
         true
    });

    // Add other built-in library entries as needed per v3.1 spec
    libraries_.push_back({
         "/lib/reasoning/with_rollback",
         "(try_path: string, fallback_path: string) -> {success: boolean}",
         nlohmann::json::parse(R"({"type": "object", "properties": {"success": {"type": "boolean"}}})"), // Example schema
         {},
         true // is subgraph
    });

    // ... add more ...
}

const std::vector<LibraryEntry>& StandardLibraryLoader::get_available_libraries() const {
    return libraries_;
}

void StandardLibraryLoader::load_from_directory(const std::string& lib_dir) {
    namespace fs = std::filesystem;
    if (!fs::exists(lib_dir) || !fs::is_directory(lib_dir)) return;

    for (const auto& entry : fs::recursive_directory_iterator(lib_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            const std::string file_path = entry.path().string();
            std::ifstream file(entry.path());
            std::stringstream buffer;
            buffer << file.rdbuf();
            try {
                auto graphs = parser_->parse_from_string(buffer.str());
                for (const auto& g : graphs) {
                    if (g.is_standard_library) {
                        LibraryEntry lib_entry;
                        lib_entry.path = g.path;
                        lib_entry.signature = g.signature;
                        lib_entry.output_schema = g.output_schema; // From parser (v3.1)
                        lib_entry.permissions = g.permissions;
                        lib_entry.is_subgraph = true;
                        libraries_.push_back(std::move(lib_entry));
                    }
                }
            } catch (const std::exception& e) {
                // Log error, but don't interrupt loading
                LOG_WARN("[library_loader] Failed to load library from "
                         << file_path << ": " << e.what());
                continue;
            }
        }
    }
}

} // namespace agenticdsl
