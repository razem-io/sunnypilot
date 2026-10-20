#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
python3 native/generate_build.py
python3 native/prepare_dashboard.py
cmake -S native -B native-build -DCMAKE_BUILD_TYPE=Release
cmake --build native-build -j6
