#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_build="${BUILD_DIR:-$task_root/build}"
task_identity="${SIGN_IDENTITY:--}"
cmake -S "$task_root" -B "$task_build" -DCMAKE_BUILD_TYPE=Release \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$task_build" --parallel
ctest --test-dir "$task_build" --output-on-failure --timeout 60
for task_binary in "$task_build"/plugins/*.scx; do
    if [ "$task_identity" = "-" ]; then
        codesign --force --sign - "$task_binary"
    else
        codesign --force --timestamp --options runtime --sign "$task_identity" "$task_binary"
    fi
    codesign --verify --strict --all-architectures --verbose=2 "$task_binary"
done
cmake --install "$task_build" --prefix "$task_root/dist"
cp "$task_root/LICENSE-DSP" "$task_root/dist/PinkTrombone/"
cp -R "$task_root/docs" "$task_root/dist/PinkTrombone/"
cp "$task_root/scripts/install.command" "$task_root/dist/install.command"
chmod +x "$task_root/dist/install.command"
echo "Built and signed: $task_root/dist/PinkTrombone"
