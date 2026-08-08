#pragma once

// Locates the mock GTP engine binary. Two build paths must both work:
//   - the cmake goban_tests target, which passes GOBAN_MOCK_ENGINE as a define
//   - tests/compile_one.sh, which builds the mock and exports it in the
//     environment
// The environment wins so a test run can be pointed at a different build.

#include <cstdlib>
#include <string>

#ifndef GOBAN_MOCK_ENGINE
#define GOBAN_MOCK_ENGINE ""
#endif

namespace goban_test {

inline std::string mockEnginePath() {
    if (const char* fromEnv = std::getenv("GOBAN_MOCK_ENGINE")) {
        if (*fromEnv) return fromEnv;
    }
    return GOBAN_MOCK_ENGINE;
}

}  // namespace goban_test
