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
# Overridable so a scenario can be run against another language's GUI —
# GOBAN_SCENARIO_CONFIG=tests/scenarios/mock-cs.json ./tests/run_scenarios.sh ...
# Do not be tempted to run ./goban directly instead: this script also redirects
# user.json and the games folder, and without it a scripted run writes to the
# real ones.
CONFIG="${GOBAN_SCENARIO_CONFIG:-$ROOT/tests/scenarios/mock.json}"

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
# The scenario config redirects the games folder here. It is wiped before *every*
# scenario, not once per suite: the daily session file is appended to on every
# load and save, so one scenario's games are visible to the next — a fresh
# user.json is not enough, because with no stored session the app falls back to
# today's file in the games folder and loads whatever is in it.
#
# This used to be per-suite and appeared to work, only because every save was
# failing silently: writeFileContent() did not create the games folder, so the
# session file was never written in the first place. Fixing that turned the
# latent coupling into three order-dependent failures.
SCENARIO_GAMES="$ROOT/tests/scenarios/.games"
trap 'rm -rf "$WORK" "$SCENARIO_GAMES"' EXIT

pass=0
fail=0
skip=0
failed_names=()

for scn in "${scenarios[@]}"; do
    name="$(basename "$scn")"
    printf '%-44s ' "$name"
    log="$WORK/${name}.log"

    # Every scenario starts from an empty games folder — see above.
    rm -rf "$SCENARIO_GAMES"

    # A scenario may ask for a different bot list with a `# config: <file>` line
    # in its first few lines, resolved relative to tests/scenarios. Without this
    # the suite has one config for the whole run, so anything needing a
    # differently configured engine could only be run by hand with
    # GOBAN_SCENARIO_CONFIG — which means the documented one-command suite would
    # silently not cover it. An explicit override on the command line still wins.
    scn_config="$CONFIG"
    if [ -z "${GOBAN_SCENARIO_CONFIG:-}" ]; then
        want="$(sed -n 's/^# *config: *//p' "$scn" | head -1)"
        if [ -n "$want" ]; then
            scn_config="$ROOT/tests/scenarios/$want"
            if [ ! -f "$scn_config" ]; then
                echo "FAIL (missing config $want)"
                fail=$((fail + 1))
                failed_names+=("$name")
                continue
            fi
        fi
    fi

    # Seed the throwaway settings with sound off. Stone sounds are pointless in
    # an automated run, and a CI box has no audio device — Pa_Initialize()'s
    # return value is not checked, so it is better not to open a stream at all.
    #
    # A scenario that is *about* the audio path says `# sound: on` in its first
    # few lines. It needs a real output device to prove anything — the mixer
    # never runs without one — so it is skipped unless GOBAN_SCENARIO_AUDIO=1,
    # which keeps the suite hermetic and the test runnable:
    #   GOBAN_SCENARIO_AUDIO=1 tests/run_scenarios.sh
    if sed -n '1,20p' "$scn" | grep -q '^# *sound: *on'; then
        if [ "${GOBAN_SCENARIO_AUDIO:-0}" != "1" ]; then
            echo "SKIP (needs an audio device; set GOBAN_SCENARIO_AUDIO=1)"
            skip=$((skip + 1))
            continue
        fi
        printf '{"sound_enabled": true}\n' > "$WORK/${name}.user.json"
    else
        printf '{"sound_enabled": false}\n' > "$WORK/${name}.user.json"
    fi

    # --script implies a throwaway user.json, but pass one explicitly so
    # concurrent runs cannot collide.
    timeout 120 "$GOBAN" \
        --config "$scn_config" \
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
if [ "$skip" -gt 0 ]; then
    echo "scenarios: $pass passed, $fail failed, $skip skipped"
else
    echo "scenarios: $pass passed, $fail failed"
fi
if [ $fail -gt 0 ]; then
    printf 'failed: %s\n' "${failed_names[*]}"
    exit 1
fi
