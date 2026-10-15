// A test harness small enough not to be a dependency.

#pragma once

#include <cstdio>
#include <filesystem>
#include <string>

namespace hamflac::check {

inline int failures = 0;
inline int checks = 0;

inline void fail(const std::string& what, const std::string& detail) {
    ++failures;
    std::fprintf(stderr, "FAIL %s: %s\n", what.c_str(), detail.c_str());
}

inline void is_true(bool ok, const std::string& what) {
    ++checks;
    if (!ok) fail(what, "expected true");
}

// A path as the UTF-8 string HamFLAC takes.
inline std::string path_text(const std::filesystem::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

inline int report(const char* suite) {
    if (failures == 0) {
        std::printf("ok: %s (%d checks)\n", suite, checks);
        return 0;
    }
    std::fprintf(stderr, "%d of %d checks FAILED in %s\n", failures, checks, suite);
    return 1;
}

}  // namespace hamflac::check

#define CHECK(c) ::hamflac::check::is_true((c), std::string(#c) + " (line " + std::to_string(__LINE__) + ")")
