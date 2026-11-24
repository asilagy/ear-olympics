#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 asilagy
# Build Ear Olympics with fm1-base (base/, a git submodule): see BUILDING.md.
#   ./build.sh [--release X.Y] [--host | --web]
#   JIELI_TOOLCHAIN, AC79_SDK, PYTHON: as fm1-base's tools/build.py (setup_toolchain.sh puts them in base/.toolchain)
set -e
cd "$(dirname "$0")"
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
PY="${PYTHON:-}"
[ -n "$PY" ] || { [ -x base/.venv/bin/python ] && PY=base/.venv/bin/python || PY=python3; }
exec "$PY" base/tools/build.py app "$@"
