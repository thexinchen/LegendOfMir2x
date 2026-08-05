#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
godot_bin=${GODOT_BIN:-}

if [ -z "$godot_bin" ]; then
    for candidate in /home/czx/godot /home/czx/godot_bin "$(command -v godot4 2>/dev/null || true)"; do
        if [ -n "$candidate" ] && [ -x "$candidate" ]; then
            godot_bin=$candidate
            break
        fi
    done
fi

pck_path="$project_dir/build/mir2x-client.pck"
world_res=${MIR2X_WORLD_RES:-"$project_dir/build/world_res"}
audio_res=${MIR2X_AUDIO_RES:-"$project_dir/build/audio"}

if [ -z "$godot_bin" ] || [ ! -x "$godot_bin" ]; then
    echo "Godot executable not found; set GODOT_BIN to its path" >&2
    exit 1
fi
if [ ! -f "$pck_path" ]; then
    echo "Client package not found: $pck_path" >&2
    echo "Run ./client_godot/build_remote.sh first" >&2
    exit 1
fi
if [ ! -d "$world_res" ]; then
    echo "World resources not found: $world_res" >&2
    exit 1
fi
if [ ! -d "$audio_res" ]; then
    echo "Audio resources not found: $audio_res" >&2
    exit 1
fi

export MIR2X_WORLD_RES=$world_res
export MIR2X_AUDIO_RES=$audio_res

exec "$godot_bin" --main-pack "$pck_path" "$@"
