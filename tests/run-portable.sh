#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
validation_dir="$(mktemp -d)"
trap 'rm -rf "$validation_dir"' EXIT
for name in model reset chat transport data features runtime export preferences; do
 g++ -std=c++17 -O2 "tests/${name}_test.cpp" -o "$validation_dir/$name"
 "$validation_dir/$name"
done
g++ -std=c++17 -O2 tests/mock_codex.cpp -o "$validation_dir/mock_codex"
python3 tests/fixture_test.py "$validation_dir/mock_codex"
