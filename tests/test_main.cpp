// doctest entry point. This is the only translation unit that defines main();
// every other tests/*.cpp file just includes <doctest/doctest.h> and adds cases.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <csignal>
#include <memory>

#include "Configuration.h"

// The same disposition main.cpp installs, and for the same reason: GtpClient
// writes into an engine's pipe and is written to handle a failed write, which
// it only gets to do if the default action has not already killed the process.
// Without this the test binary inherits the hazard without the mitigation — a
// killed engine's teardown took the whole suite down with signal 13.
//
// POSIX only, and guarded the same way main.cpp guards its copy: SIGPIPE does
// not exist on Windows, where a write to a broken pipe fails as an ordinary
// error rather than by signal, so there is nothing to disarm. MSVC never saw
// this file until the release build ran for the first time in six months.
#ifndef _WIN32
static const int g_ignoreSigpipe = []() { std::signal(SIGPIPE, SIG_IGN); return 0; }();
#endif

// goban_core declares this global (GameThread.h, GameRecord.cpp) but the
// definition lives in the application's main.cpp, which tests do not link.
// GameRecord guards every use with `if (config && ...)`, so a null default is
// valid; a test that needs real configuration data can assign to it.
std::shared_ptr<Configuration> config;
