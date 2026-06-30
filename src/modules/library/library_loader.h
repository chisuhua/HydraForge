// modules/library/include/library/library_loader.h
#ifndef AGENTICDSL_MODULES_LIBRARY_LIBRARY_LOADER_H
#define AGENTICDSL_MODULES_LIBRARY_LIBRARY_LOADER_H

#include "library/schema.h" // 引入 LibraryEntry
#include <memory>
#include <vector>
#include <string>

namespace agenticdsl {

class MarkdownParser;

class StandardLibraryLoader {
public:
    static StandardLibraryLoader& instance();
    const std::vector<LibraryEntry>& get_available_libraries() const;
    void load_from_directory(const std::string& lib_dir);
    void load_builtin_libraries(); // 加载内置子图定义(路径、Schema)

private:
    StandardLibraryLoader();
    ~StandardLibraryLoader();
    std::vector<LibraryEntry> libraries_;
    std::unique_ptr<MarkdownParser> parser_;
};

} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_LIBRARY_LIBRARY_LOADER_H
