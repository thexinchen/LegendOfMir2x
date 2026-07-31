#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
godot_bin=${GODOT_BIN:-/home/ubuntu/.local/bin/godot4}

if [ ! -x "$godot_bin" ]; then
    echo "Godot executable not found: $godot_bin" >&2
    exit 1
fi

mkdir -p "$project_dir/build"
"$godot_bin" --headless --editor --path "$project_dir" --quit
"$godot_bin" --headless --path "$project_dir" --export-pack Linux "$project_dir/build/mir2x-client.pck"
