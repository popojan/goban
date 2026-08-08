// doctest entry point. This is the only translation unit that defines main();
// every other tests/*.cpp file just includes <doctest/doctest.h> and adds cases.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <memory>

#include "Configuration.h"

// goban_core declares this global (GameThread.h, GameRecord.cpp) but the
// definition lives in the application's main.cpp, which tests do not link.
// GameRecord guards every use with `if (config && ...)`, so a null default is
// valid; a test that needs real configuration data can assign to it.
std::shared_ptr<Configuration> config;
