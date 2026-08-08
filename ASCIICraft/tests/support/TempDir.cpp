#include "support/TempDir.hpp"

#include <atomic>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#define TESTSUPPORT_GETPID _getpid
#else
#include <unistd.h>
#define TESTSUPPORT_GETPID getpid
#endif

namespace testsupport {
namespace {

/// Distinguishes directories created within one process run.
std::atomic<unsigned> g_counter{0};

} // namespace

TempDir::TempDir(const std::string& label) {
    const unsigned n = g_counter.fetch_add(1, std::memory_order_relaxed);
    const std::string name =
        "asciicraft_" + label + "_" +
        std::to_string(TESTSUPPORT_GETPID()) + "_" + std::to_string(n);

    path_ = std::filesystem::temp_directory_path() / "asciicraft_tests" / name;

    std::error_code ec;
    std::filesystem::remove_all(path_, ec);   // paranoia against a stale collision
    std::filesystem::create_directories(path_, ec);
    if (ec) {
        throw std::filesystem::filesystem_error("TempDir: failed to create directory", path_, ec);
    }
}

TempDir::~TempDir() {
    if (keep_) return;
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);   // best effort; a destructor must not throw
}

} // namespace testsupport
