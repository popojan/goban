#!/bin/bash
# Extract unique Chinese characters from RML files and generate font-charset codepoints

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
ZH_DIR="${1:-$SCRIPT_DIR/zh}"

# Extract all text from RML files, get unique Chinese characters, convert to codepoints
grep -hoPr '[\x{4e00}-\x{9fff}\x{ff00}-\x{ffef}]' "$ZH_DIR"/*.rml 2>/dev/null \
  | sort -u \
  | while read -r char; do
      printf "U+%04X," "'$char"
    done \
  | sed 's/,$//'

echo ""
echo "# Characters found:"
grep -hoPr '[\x{4e00}-\x{9fff}\x{ff00}-\x{ffef}]' "$ZH_DIR"/*.rml 2>/dev/null | sort -u | tr '\n' ' '
echo ""
