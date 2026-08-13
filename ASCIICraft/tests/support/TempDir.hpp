#pragma once

#include <filesystem>
#include <string>

namespace testsupport {

/// RAII unique temporary directory, removed on destruction.
///
/// Every test that touches region files must run inside one of these. Without it
/// tests share the process CWD's `regions/` directory, which means they interfere
/// with each other, cannot run in parallel, and write into the real save.
class TempDir {
public:
    /// \param label short tag included in the directory name to aid debugging.
    explicit TempDir(const std::string& label = "test");
    ~TempDir();

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& Path() const { return path_; }
    operator const std::filesystem::path&() const { return path_; }

    /// Path to a child entry inside this directory. Does not create anything.
    std::filesystem::path operator/(const std::string& child) const { return path_ / child; }

    /// Leave the directory on disk after destruction, for inspecting a failure.
    void Keep() { keep_ = true; }

private:
    std::filesystem::path path_;
    bool keep_ = false;
};

} // namespace testsupport
