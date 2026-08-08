// Entry point for the ASCIICraft test suite.
//
// Defines doctest's main and wraps it so the ASCIIgL logger is initialized
// before any test runs and closed afterwards. Several world subsystems log
// warnings during normal operation (missing models, corrupt blobs in the
// corruption tests); routing those to a file keeps test output readable while
// preserving them for diagnosis.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <ASCIIgL/util/Logger.hpp>

#include <cstdlib>
#include <string>

namespace {

/// ASCIICRAFT_TEST_LOG_LEVEL=error|warning|info|debug overrides the default.
/// Defaults to Warning: quiet enough to read failures, loud enough to diagnose.
ASCIIgL::LogLevel ResolveLogLevel() {
    const char* env = std::getenv("ASCIICRAFT_TEST_LOG_LEVEL");
    if (!env) return ASCIIgL::LogLevel::Warning;

    const std::string level(env);
    if (level == "error")   return ASCIIgL::LogLevel::Error;
    if (level == "warning") return ASCIIgL::LogLevel::Warning;
    if (level == "info")    return ASCIIgL::LogLevel::Info;
    if (level == "debug")   return ASCIIgL::LogLevel::Debug;
    return ASCIIgL::LogLevel::Warning;
}

} // namespace

int main(int argc, char** argv) {
    ASCIIgL::Logger::Init("logs/tests.log", ResolveLogLevel());

    doctest::Context context;
    context.applyCommandLine(argc, argv);

    const int result = context.run();

    ASCIIgL::Logger::Close();

    // Honour --exit / --no-run so `--list-test-cases` (used by CTest discovery)
    // does not fall through into extra work.
    if (context.shouldExit()) return result;
    return result;
}
