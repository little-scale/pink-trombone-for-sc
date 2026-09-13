#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
"$task_root/scripts/build_macos.sh"
task_binary_zip="$task_root/dist/PinkTromboneSC-1.0.0-macOS-universal.zip"
task_source_zip="$task_root/dist/PinkTromboneSC-1.0.0-source.zip"
task_stage="$(mktemp -d "${TMPDIR:-/tmp}/pink-trombone-package.XXXXXX")"
trap 'rm -rf "$task_stage"' EXIT
# Recreate the archives to avoid retaining obsolete files from an older package.
rm -f "$task_binary_zip" "$task_source_zip"
(
    cd "$task_root/dist"
    /usr/bin/zip -qr "$task_binary_zip" PinkTrombone install.command
)
(
    # Keep a stable archive root regardless of the checkout directory's name.
    rsync -a --exclude '/build*/' --exclude '/dist/' --exclude '.git/' \
        --exclude '__pycache__/' --exclude '.DS_Store' \
        "$task_root/" "$task_stage/PinkTromboneSC/"
    cd "$task_stage"
    /usr/bin/zip -qr "$task_source_zip" PinkTromboneSC
)
(
    cd "$task_root/dist"
    shasum -a 256 "${task_binary_zip##*/}" "${task_source_zip##*/}" > SHA256SUMS.txt
)
echo "Packages: $task_binary_zip and $task_source_zip"
