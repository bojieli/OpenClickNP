// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/source.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace openclicknp {

SourceBuffer::SourceBuffer(std::string path, std::string contents)
    : path_(std::move(path)), contents_(std::move(contents)) {
    line_starts_.push_back(0);
    for (size_t i = 0; i < contents_.size(); ++i) {
        if (contents_[i] == '\n') {
            line_starts_.push_back(static_cast<uint32_t>(i + 1));
        }
    }
}

std::string_view SourceBuffer::lineText(uint32_t line) const {
    if (line == 0 || line > line_starts_.size()) return {};
    uint32_t begin = line_starts_[line - 1];
    uint32_t end   = (line < line_starts_.size())
                       ? line_starts_[line] - 1
                       : static_cast<uint32_t>(contents_.size());
    while (end > begin && (contents_[end - 1] == '\r')) --end;
    return std::string_view(contents_.data() + begin, end - begin);
}

std::pair<uint32_t, uint32_t>
SourceBuffer::lineColOf(uint32_t offset) const {
    // Binary search.
    size_t lo = 0, hi = line_starts_.size();
    while (lo + 1 < hi) {
        size_t mid = (lo + hi) / 2;
        if (line_starts_[mid] <= offset) lo = mid; else hi = mid;
    }
    uint32_t line = static_cast<uint32_t>(lo + 1);
    uint32_t col  = static_cast<uint32_t>(offset - line_starts_[lo] + 1);
    return {line, col};
}

uint32_t SourceManager::addBuffer(std::string path, std::string contents) {
    auto buf = std::make_unique<SourceBuffer>(std::move(path),
                                              std::move(contents));
    buffers_.push_back(std::move(buf));
    return static_cast<uint32_t>(buffers_.size() - 1);
}

const SourceBuffer& SourceManager::buffer(uint32_t file_id) const {
    if (file_id >= buffers_.size()) {
        throw std::out_of_range("SourceManager::buffer: bad file_id");
    }
    return *buffers_[file_id];
}

std::string SourceManager::resolveImport(std::string_view referer_path,
                                         std::string_view target) const {
    namespace fs = std::filesystem;
    if (target.empty()) return std::string(target);

    // Absolute path: take as-is if exists.
    fs::path tp(std::string{target});
    if (tp.is_absolute() && fs::exists(tp)) return tp.string();

    // Relative to the importing file's directory.
    if (!referer_path.empty()) {
        fs::path rel = fs::path(std::string{referer_path}).parent_path() / tp;
        if (fs::exists(rel)) return rel.string();
    }

    // Search import dirs.
    for (const auto& dir : import_dirs_) {
        fs::path candidate = fs::path(dir) / tp;
        if (fs::exists(candidate)) return candidate.string();
    }

    // Fall back: bare name. Caller will report failure.
    return std::string(target);
}

}  // namespace openclicknp
