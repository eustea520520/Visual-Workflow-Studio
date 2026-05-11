#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
cmake -S . -B "$BUILD_DIR" -G Ninja
