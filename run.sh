#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_type="Release"
auto=false

while (( $# > 0 )); do
    case "$1" in
        Debug|debug)
            build_type="Debug"
            shift
            ;;
        Release|release)
            build_type="Release"
            shift
            ;;
        --auto)
            if [[ "${auto}" == true ]]; then
                echo "Usage: $0 [Debug|Release] [--auto]" >&2
                exit 2
            fi
            auto=true
            shift
            ;;
        *)
            echo "Usage: $0 [Debug|Release] [--auto]" >&2
            exit 2
            ;;
    esac
done

run_dir="${root_dir}/build/${build_type}"

if [[ ! -x "${run_dir}/server" ]]; then
    echo "Missing executable: ${run_dir}/server" >&2
    exit 1
fi

if [[ ! -x "${run_dir}/client" ]]; then
    echo "Missing executable: ${run_dir}/client" >&2
    exit 1
fi

cd "${run_dir}"
./server --auto-launch &
sleep 3

if [[ "${auto}" == true ]]; then
    ./client --server-ip=localhost --auto-login=test:123456
else
    ./client
fi
