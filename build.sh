#!/usr/bin/env bash
# Build and (optionally) launch openFrameworks sketches under apps/myApps on macOS,
# via each sketch's oF Makefile. macOS counterpart of build.ps1.
#
# Usage:
#   ./build.sh                                   # list buildable projects (folders with a Makefile)
#   ./build.sh <project> [Debug|Release] [build|rebuild|clean] [--run]
#
# Examples:
#   ./build.sh Template_1_reactive_bounce
#   ./build.sh Template_1_reactive_bounce Release --run
#   ./build.sh Template_1_reactive_bounce Debug clean
#
# --run launches bin/<project>[_debug].app after a successful build. These sketches open an
# audio stream and may start recording an .mp4 into bin/data/ (see bRecordEnabled), so running
# one has real side effects.

set -euo pipefail

repo_root="$(cd "$(dirname "$0")" && pwd)"

project=""
config="Debug"
target="build"
run=0
for arg in "$@"; do
    case "$arg" in
        Debug|Release) config="$arg" ;;
        build|rebuild|clean) target="$arg" ;;
        --run|-r) run=1 ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) project="$(basename "${arg%/}")" ;;
    esac
done

if [[ -z "$project" ]]; then
    echo "Usage: ./build.sh <project> [Debug|Release] [build|rebuild|clean] [--run]"
    echo
    echo "Buildable projects (have a Makefile):"
    for d in "$repo_root"/*/; do
        [[ -f "$d/Makefile" ]] && echo "  $(basename "$d")"
    done
    exit 0
fi

project_dir="$repo_root/$project"
if [[ ! -f "$project_dir/Makefile" ]]; then
    echo "error: no Makefile in $project_dir (copy scripts/templates/osx/Makefile from the oF root)" >&2
    exit 1
fi

jobs="$(sysctl -n hw.ncpu)"
cd "$project_dir"
case "$target" in
    clean)   make clean ;;
    rebuild) make clean && make -j"$jobs" "$config" ;;
    build)   make -j"$jobs" "$config" ;;
esac

if [[ "$run" == 1 && "$target" != "clean" ]]; then
    suffix=""
    [[ "$config" == "Debug" ]] && suffix="_debug"
    exe="$project_dir/bin/$project$suffix.app/Contents/MacOS/$project$suffix"
    echo "Launching $exe"
    # Run from bin/ like the VS Code launch config, so relative paths behave the same.
    cd "$project_dir/bin" && exec "$exe"
fi
