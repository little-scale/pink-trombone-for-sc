#!/bin/bash
set -euo pipefail
task_source="$(cd "$(dirname "$0")" && pwd)/PinkTrombone"
task_extensions="$HOME/Library/Application Support/SuperCollider/Extensions"
task_destination="$task_extensions/PinkTrombone"
if [ ! -f "$task_source/PinkTrombone.scx" ]; then
    echo "Keep install.command beside the PinkTrombone folder from the binary package."
    exit 1
fi
for task_binary in "$task_source"/*.scx; do
    codesign --verify --strict --all-architectures "$task_binary"
done
mkdir -p "$task_extensions"
if [ -e "$task_destination" ]; then
    # Put backups outside Extensions so SuperCollider cannot load two copies.
    task_backup="$HOME/Library/Application Support/SuperCollider/PinkTrombone-backups/$(date +%Y%m%d-%H%M%S)-$$"
    mkdir -p "$task_backup"
    mv "$task_destination" "$task_backup/PinkTrombone"
    echo "Previous version saved to: $task_backup/PinkTrombone"
fi
ditto "$task_source" "$task_destination"
echo "Installed: $task_destination"
echo "Restart SuperCollider's interpreter and audio server, then open PinkTrombone help."
