#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

build_type="Release"
preset="conan-release"
build_target=""
cli_resource_path=""
use_git_proxy=false

while (( $# > 0 )); do
    case "$1" in
        -h|--help)
            cat <<EOF
Usage: $(basename "$0") [Debug|Release] [--proxy] [--target TARGET] [--mir2x-res PATH]

Arguments:
  Debug|Release       Build configuration. Default: Release.

Options:
  --proxy            Use the current Git proxy configuration.
  --target TARGET     Build only the specified CMake target.
  --mir2x-res PATH    Set MIR2X_RES_REPO_PATH for CMake configure.
  -h, --help          Show this help message.
EOF
            exit 0
            ;;
        --proxy)
            use_git_proxy=true
            shift
            ;;
        Debug|debug)
            build_type="Debug"
            preset="conan-debug"
            shift
            ;;
        Release|release)
            build_type="Release"
            preset="conan-release"
            shift
            ;;
        --target)
            if (( $# < 2 )) || [[ -n "${build_target}" ]]; then
                echo "Usage: $0 [Debug|Release] [--proxy] [--target TARGET] [--mir2x-res PATH]" >&2
                exit 2
            fi
            build_target="$2"
            shift 2
            ;;
        --mir2x-res)
            if (( $# < 2 )) || [[ -n "${cli_resource_path}" ]]; then
                echo "Usage: $0 [Debug|Release] [--proxy] [--target TARGET] [--mir2x-res PATH]" >&2
                exit 2
            fi
            cli_resource_path="$2"
            shift 2
            ;;
        *)
            echo "Usage: $0 [Debug|Release] [--proxy] [--target TARGET] [--mir2x-res PATH]" >&2
            exit 2
            ;;
    esac
done

if [[ "${use_git_proxy}" == true ]]; then
    git_http_proxy="$(git config --get http.proxy 2>/dev/null || true)"
    git_https_proxy="$(git config --get https.proxy 2>/dev/null || true)"
    export GIT_CONFIG_COUNT=2
    export GIT_CONFIG_KEY_0="http.proxy"
    export GIT_CONFIG_VALUE_0="${git_http_proxy}"
    export GIT_CONFIG_KEY_1="https.proxy"
    export GIT_CONFIG_VALUE_1="${git_https_proxy}"
fi

resource_args=()
resource_path=""
if [[ -n "${cli_resource_path}" ]]; then
    resource_path="${cli_resource_path}"
elif [[ -n "${MIR2X_RES_REPO_PATH:-}" ]]; then
    resource_path="${MIR2X_RES_REPO_PATH}"
elif [[ -d "../mir2x_res" ]]; then
    resource_path="$(cd ../mir2x_res && pwd)"
fi

if [[ -n "${resource_path}" ]]; then
    resource_args+=("-DMIR2X_RES_REPO_PATH=${resource_path}")
fi

if [[ "${build_target}" == "zsdbdeploy" && ! -d "${resource_path}" ]]; then
    echo "Resource directory not found. Set MIR2X_RES_REPO_PATH or place mir2x_res next to this repository." >&2
    exit 2
fi

echo "[1/3] Installing Conan dependencies for ${build_type}..."
package_manager_args=(-c tools.system.package_manager:mode=install)
if (( EUID != 0 )); then
    package_manager_args+=(-c tools.system.package_manager:sudo=True)
fi

conan install . \
    -s:h "build_type=${build_type}" \
    -s:h compiler.cppstd=26 \
    -c "tools.cmake:configure_args=['-DCMAKE_POLICY_VERSION_MINIMUM=3.5']" \
    "${package_manager_args[@]}" \
    --build=missing

echo "[2/3] Configuring CMake..."
cmake --fresh --preset "${preset}" "${resource_args[@]}"

echo "[3/3] Building ${build_type}..."
if [[ -n "${build_target}" ]]; then
    cmake --build --preset "${preset}" --target "${build_target}" --parallel 10
else
    cmake --build --preset "${preset}" --parallel 10
fi

echo "Build completed successfully."
