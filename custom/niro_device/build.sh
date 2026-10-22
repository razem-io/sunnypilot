#!/bin/bash
# ARM Linux build; SDK is read-only and is never redistributed in Git.
set -euo pipefail
cd "$(dirname "$0")"
: "${NIRO_SDK:?Set NIRO_SDK to an extracted comma SDK (see README)}"
test -d .build/source || python3 prepare.py
image=niro-branch-builder:local
docker build --platform linux/arm64 -t "$image" -f Dockerfile .
docker run --rm --platform linux/arm64 --name niro-branch-build \
 -v "$PWD:/package" -v "$NIRO_SDK:/device-sdk:ro" "$image" bash -c '
set -euo pipefail
cd /package
mkdir -p .build/source/cereal/gen/cpp release
cmp -s native/main.cc .build/source/selfdrive/ui/main.cc || cp native/main.cc .build/source/selfdrive/ui/main.cc
mkdir -p .build/generated
python3 .build/source/cereal/services.py > .build/generated/services.h
cmp -s .build/generated/services.h .build/source/cereal/services.h || cp .build/generated/services.h .build/source/cereal/services.h
capnp compile -I .build/source -I /device-sdk/usr/local/include --src-prefix=.build/source/cereal -oc++:.build/generated .build/source/cereal/*.capnp
for file in .build/generated/*.capnp.*; do
 target=.build/source/cereal/gen/cpp/$(basename "$file")
 cmp -s "$file" "$target" || cp "$file" "$target"
done
cmake -S native -B .build/arm64 -DNIRO_SOURCE=/package/.build/source -DNIRO_SDK=/device-sdk -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=ON
cmake --build .build/arm64 -j6
cp .build/arm64/sunnypilot-ui release/niro-ui
c++ -O2 -std=c++17 -I .build/source/third_party/linux/include projection/native/qcom_encoder_probe.cc -ldl -o release/qcom-encoder
'
python3 manifest.py --write
