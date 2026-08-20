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
world_res="$project_dir/build/world_res"
audio_res="$project_dir/build/audio"

if [ -z "$godot_bin" ] || [ ! -x "$godot_bin" ]; then
    echo "Godot executable not found; set GODOT_BIN to its path" >&2
    exit 1
fi
if [ ! -d "$world_res" ]; then
    echo "World resources not found: $world_res" >&2
    echo "Run ./client_godot/build_remote.sh first" >&2
    exit 1
fi
if [ ! -d "$audio_res" ]; then
    echo "Audio resources not found: $audio_res" >&2
    echo "Run ./client_godot/build_remote.sh first" >&2
    exit 1
fi

export MIR2X_WORLD_RES=$world_res
export MIR2X_AUDIO_RES=$audio_res

# Keep the packed client in sync with the checked-out project.  This avoids
# running an older package after changing GDScript/scene files and makes the
# launcher self-contained for manual login runs.
rebuild_pck=0
if [ ! -f "$pck_path" ]; then
    rebuild_pck=1
elif find "$project_dir/scripts" "$project_dir/scenes" "$project_dir/tests" \
        "$project_dir/assets" "$project_dir/project.godot" "$project_dir/export_presets.cfg" \
        -type f ! -name '*.import' ! -name '*.uid' ! -path '*/generated/*' \
        -newer "$pck_path" -print -quit | grep -q .; then
    rebuild_pck=1
fi

if [ "$rebuild_pck" -eq 1 ]; then
    echo "Exporting current Godot project: $pck_path" >&2
    "$godot_bin" --headless --editor --path "$project_dir" --quit
    "$godot_bin" --headless --path "$project_dir" --export-pack Linux "$pck_path"
fi

exec "$godot_bin" --main-pack "$pck_path" "$@"
