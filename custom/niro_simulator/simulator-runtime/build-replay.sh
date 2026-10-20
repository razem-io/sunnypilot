#!/bin/bash
set -eu
cd "$(dirname "$0")"
if [ ! -d libyuv ]; then git clone --depth=1 https://chromium.googlesource.com/libyuv/libyuv libyuv; fi
cmake -S libyuv -B yuv-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build yuv-build --target yuv -j 6
# Keep the development publisher local to this computer.
python3 - <<'PATCH'
from pathlib import Path
p=Path('source/msgq_repo/msgq/impl_zmq.cc')
p.write_text(p.read_text().replace('"tcp://*:"', '"tcp://127.0.0.1:"'))
PATCH
mkdir -p source/cereal/gen/cpp
(cd source && capnp compile -I /opt/homebrew/include --src-prefix=cereal -oc++:cereal/gen/cpp cereal/log.capnp cereal/car.capnp cereal/legacy.capnp cereal/custom.capnp)
python3 source/cereal/services.py > source/cereal/services.h
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 6
