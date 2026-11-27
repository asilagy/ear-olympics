#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 asilagy
# Ear Olympics' tests, from the repo root: tests/run_tests.sh
#   1. the music, natively: tests/host/*.c (the melody generator, many seeds), built with the system C compiler
#   2. the whole app in fm1-base's host simulator: app/tests/*.txt scripts, with screenshots in app/build/host/
# Needs the build tools for the second part (BUILDING.md).
set -u
cd "$(dirname "$0")/.."
FAIL=0
mkdir -p app/build/host
for c in tests/host/*.c; do
    name=$(basename "$c" .c)
    if cc -std=c99 -Wall -Wextra -Werror -O2 -o "app/build/host/$name" "$c" 2> "app/build/host/$name.cc.log" &&
       "app/build/host/$name" > "app/build/host/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see app/build/host/$name.cc.log and $name.log)"
        cat "app/build/host/$name.cc.log" "app/build/host/$name.log" | head -10 | sed 's/^/       /'
        FAIL=1
    fi
done
./build.sh --host > app/build.log 2>&1 || { echo "  FAIL build (see app/build.log)"; tail -15 app/build.log; exit 1; }
for t in app/tests/*.txt; do
    name=$(basename "$t" .txt)
    if app/build/host/earolympics_host "$t" app/build/host > "app/build/host/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see app/build/host/$name.log)"
        grep -E "FAIL|EXPECTATIONS" "app/build/host/$name.log" | head -10 | sed 's/^/       /'
        FAIL=1
    fi
done
# the flash budget: a summary, never a failure (tools/budget.py has the table and the plans)
if command -v python3 > /dev/null; then
    python3 tools/budget.py > app/build/budget.txt 2>&1
    echo "  info budget: $(grep 'DATA TOTAL' app/build/budget.txt | sed 's/  */ /g')"
    grep -E "OVER|near its ceiling|over 85%" app/build/budget.txt | sed 's/^/       WARN /'
fi
exit $FAIL
