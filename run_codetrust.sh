#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"

if [ ! -x "$ROOT/build/codetrust-cli" ]; then
    echo "Building CodeTrust CLI..."
    make -C "$ROOT" cli
fi

SOURCE="${1:-$ROOT/src/submission.cpp}"
TESTS="${2:-$ROOT/testcases.txt}"

exec "$ROOT/build/codetrust-cli" \
    --root "$ROOT" \
    --source "$SOURCE" \
    --tests "$TESTS" \
    --label "shell-run"
