// SPDX-License-Identifier: Apache-2.0
// Source-location tracking and source-buffer management.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace openclicknp {

struct SourceLoc {
    uint32_t file_id = 0;
    uint32_t line    = 0;  // 1-based
    uint32_t column  = 0;  // 1-based
    uint32_t offset  = 0;  // byte offset into the file
};

struct SourceRange {
    SourceLoc begin{};
    SourceLoc end{};
};

class SourceBuffer {
public:
    SourceBuffer(std::string path, std::string contents);

    [[nodiscard]] const std::string&    path()     const noexcept { return path_; }
    [[nodiscard]] const std::string&    contents() const noexcept { return contents_; }
    [[nodiscard]] std::string_view      lineText(uint32_t line) const;
    [[nodiscard]] std::pair<uint32_t,uint32_t>
                                        lineColOf(uint32_t offset) const;

private:
    std::string path_;
    std::string contents_;
    std::vector<uint32_t> line_starts_;
};

class SourceManager {
public:
    [[nodiscard]] uint32_t addBuffer(std::string path, std::string contents);
    [[nodiscard]] const SourceBuffer& buffer(uint32_t file_id) const;

    // Search path for `import "..."` resolution.
    void addImportDir(std::string dir) { import_dirs_.push_back(std::move(dir)); }
    [[nodiscard]] const std::vector<std::string>& importDirs() const noexcept {
        return import_dirs_;
    }
    [[nodiscard]] std::string resolveImport(std::string_view referer_path,
                                            std::string_view target) const;

private:
    std::vector<std::unique_ptr<SourceBuffer>> buffers_;
    std::vector<std::string> import_dirs_;
};

}  // namespace openclicknp
