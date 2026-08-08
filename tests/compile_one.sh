#!/usr/bin/env bash
# Compile and run a single test file against the already-built libgoban_core.a.
#
# Purpose: a fast inner loop that does NOT touch the shared cmake build
# directory, so several people (or agents) can iterate on different test files
# concurrently without racing on `make`. The authoritative build is still
# `make goban_tests && ctest` in cmake-build-release.
#
# Usage:  tests/compile_one.sh tests/test_board_rules.cpp [-- <doctest args>]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/cmake-build-release"

if [ $# -lt 1 ]; then
    echo "usage: $0 <tests/foo.cpp> [-- <doctest args>]" >&2
    exit 2
fi

SRC="$1"; shift
DOCTEST_ARGS=()
if [ "${1:-}" = "--" ]; then shift; DOCTEST_ARGS=("$@"); fi

if [ ! -f "$BUILD/libgoban_core.a" ]; then
    echo "libgoban_core.a not found. Run: cd $BUILD && make goban_core" >&2
    exit 1
fi

INCLUDES=(
    -I"$BUILD/generated"
    -I"$ROOT/tests"
    -I"$ROOT/src"
    -I"$BUILD/_deps/glm-src"
    -I"$ROOT/deps/libsgfcplusplus/include"
    -I"$BUILD/deps/libsgfcplusplus/src"
    -I"$BUILD/_deps/spdlog-src/include"
    -I"$BUILD/_deps/rmlui-src/Include"
    -I"$BUILD/_deps/json-src/include"
    -I"$BUILD/_deps/doctest-src"
)

LIBS=(
    "$BUILD/libgoban_core.a"
    "$BUILD/deps/libsgfcplusplus/src/liblibsgfcplusplus_static.a"
    "$BUILD/_deps/spdlog-build/libspdlog.a"
    "$BUILD/librmlui.a"
    "$BUILD/librmlui_debugger.a"
    /usr/lib64/libfreetype.so
    -lpthread
)

OUT="$(mktemp -d)/$(basename "$SRC" .cpp)"
trap 'rm -rf "$(dirname "$OUT")"' EXIT

# Tests that spawn a GTP engine need the mock built. Keep it up to date here so
# this script stays self-sufficient (the cmake build has its own target).
MOCK="$BUILD/mock_gtp_engine"
if [ ! -x "$MOCK" ] || [ "$ROOT/tests/mock_gtp_engine.cpp" -nt "$MOCK" ]; then
    g++ -std=gnu++17 -O2 -o "$MOCK" "$ROOT/tests/mock_gtp_engine.cpp"
fi
export GOBAN_MOCK_ENGINE="$MOCK"

g++ -std=gnu++17 -O0 -g -Wall -Wextra \
    "${INCLUDES[@]}" \
    "$ROOT/tests/test_main.cpp" "$ROOT/$SRC" \
    -o "$OUT" "${LIBS[@]}"

"$OUT" "${DOCTEST_ARGS[@]}"
