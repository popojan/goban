#!/usr/bin/env bash
# Runs scenario scripts against the real application, headlessly.
#
# Each scenario drives goban through its command registry and asserts on state
# it reads back, so a failure is a machine-checkable fact rather than something
# a human has to eyeball. Exit status is 0 only if every scenario passed.
#
# Usage:
#   tests/run_scenarios.sh                       # all scenarios
#   tests/run_scenarios.sh tests/scenarios/x.scn # one scenario
#
# Requires a GL context. On a desktop the window is created hidden. In CI use:
#   xvfb-run -s "-screen 0 1024x768x24" tests/run_scenarios.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"   # asset paths in the config are relative to the repo root

BUILD="$ROOT/cmake-build-release"
GOBAN="$BUILD/goban"
MOCK="$BUILD/mock_gtp_engine"
CONFIG="$ROOT/tests/scenarios/mock.json"

for f in "$GOBAN" "$MOCK"; do
    if [ ! -x "$f" ]; then
        echo "missing $f — build with: cd cmake-build-release && make goban mock_gtp_engine" >&2
        exit 1
    fi
done

if [ $# -gt 0 ]; then
    scenarios=("$@")
else
    mapfile -t scenarios < <(find "$ROOT/tests/scenarios" -name '*.scn' | sort)
fi

if [ ${#scenarios[@]} -eq 0 ]; then
    echo "no scenarios found" >&2
    exit 1
fi

# Scratch settings and games directories, so a run cannot touch real data.
WORK="$(mktemp -d)"
# The scenario config redirects the games folder here. Wiping it per suite run
# matters for more than tidiness: the daily session file is appended to on every
# load, and a large one slows the auto-save enough to blow the scenario waits.
SCENARIO_GAMES="$ROOT/tests/scenarios/.games"
rm -rf "$SCENARIO_GAMES"
trap 'rm -rf "$WORK" "$SCENARIO_GAMES"' EXIT

pass=0
fail=0
failed_names=()

for scn in "${scenarios[@]}"; do
    name="$(basename "$scn")"
    printf '%-44s ' "$name"
    log="$WORK/${name}.log"

    # Seed the throwaway settings with sound off. Stone sounds are pointless in
    # an automated run, and a CI box has no audio device — Pa_Initialize()'s
    # return value is not checked, so it is better not to open a stream at all.
    printf '{"sound_enabled": false}\n' > "$WORK/${name}.user.json"

    # --script implies a throwaway user.json, but pass one explicitly so
    # concurrent runs cannot collide.
    timeout 120 "$GOBAN" \
        --config "$CONFIG" \
        --script "$scn" \
        --user-settings "$WORK/${name}.user.json" \
        --verbosity info \
        > "$log" 2>&1
    status=$?

    if [ $status -eq 0 ]; then
        echo "PASS"
        pass=$((pass + 1))
    else
        if [ $status -eq 124 ]; then
            echo "TIMEOUT"
        else
            echo "FAIL (exit $status)"
        fi
        fail=$((fail + 1))
        failed_names+=("$name")
        # Surface just the scenario diagnostics, not the whole app log.
        grep -E "scenario:" "$log" | tail -40 | sed 's/^/    /'
    fi
done

echo
echo "scenarios: $pass passed, $fail failed"
if [ $fail -gt 0 ]; then
    printf 'failed: %s\n' "${failed_names[*]}"
    exit 1
fi
