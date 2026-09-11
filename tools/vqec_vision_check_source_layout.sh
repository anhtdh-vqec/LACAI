#!/usr/bin/env bash
# Read-only structural checks. Not a C++ parser, compiler, ABI or ownership validator.
# Portable counterpart to vqec_vision_check_source_layout.ps1 for Linux/CI hosts.

set -u

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_roots=(src include tests tools)
source_extensions='\.(c|cc|cpp|cxx|h|hh|hpp|hxx|ps1|py|sh)$'
include_root_list=(include src src/app src/outputs
    src/adapters/camera src/adapters/qualcomm src/adapters/fw_output
    src/adapters/reference src/runtime/model_registry src/runtime/feature_manager
    src/runtime/scheduler src/runtime/admission src/runtime/lifecycle
    src/perception/detection src/perception/tracking src/perception/attributes)

issues=0
count=0

issue() {
    printf '%s\n' "$1"
    issues=$((issues + 1))
}

is_source_name() {
    [[ "$1" =~ ^vqec_vision_[a-z][a-z0-9]*(_[a-z0-9]+)*\.[a-z0-9]+$ ]]
}

resolve_include() {
    local _including_dir="$1"
    local _include="$2"
    local _candidate
    if [[ -f "$_including_dir/$_include" ]]; then
        return 0
    fi
    for _candidate in "${include_root_list[@]}"; do
        if [[ -f "$root/$_candidate/$_include" ]]; then
            return 0
        fi
    done
    return 1
}

while IFS= read -r -d '' file; do
    count=$((count + 1))
    base="$(basename "$file")"
    if ! is_source_name "$base"; then
        issue "Invalid source filename: $file"
    fi
    case "$base" in
        *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx) ;;
        *) continue ;;
    esac
    dir="$(dirname "$file")"
    while IFS= read -r line; do
        include="${line#*\"}"
        include="${include%%\"*}"
        if ! resolve_include "$dir" "$include"; then
            issue "Unresolved quoted include: $base: $include"
        fi
    done < <(grep -oE '^[[:space:]]*#[[:space:]]*include[[:space:]]*"[^"]+"' "$file" || true)
done < <(find "${source_roots[@]/#/$root/}" -type f -regextype posix-extended -regex ".*$source_extensions" -print0 2>/dev/null)

while IFS= read -r path; do
    if [[ ! -f "$root/$path" ]]; then
        issue "Missing CMake source: $path"
    fi
done < <(grep -oE '(src|tests|tools)/[a-zA-Z0-9_/]+\.(cpp|cc|cxx|c)\b' "$root/CMakeLists.txt" || true)

if [[ "$issues" -gt 0 ]]; then
    exit 1
fi
printf 'PASS: %d source/tool filenames, quoted includes and CMake source paths.\n' "$count"
