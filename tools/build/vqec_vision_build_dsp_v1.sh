#!/usr/bin/env bash
set -euo pipefail

sdk_root=""
output_dir=""
dsp_arch=""
host_compat_lib_dir=""

while (($# > 0)); do
    case "$1" in
        --sdk-root)
            sdk_root=${2:-}
            shift 2
            ;;
        --output-dir)
            output_dir=${2:-}
            shift 2
            ;;
        --dsp-arch)
            dsp_arch=${2:-}
            shift 2
            ;;
        --host-compat-lib-dir)
            host_compat_lib_dir=${2:-}
            shift 2
            ;;
        *)
            echo "unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

if [[ -z "$sdk_root" || -z "$output_dir" || -z "$dsp_arch" ]]; then
    echo "usage: $0 --sdk-root PATH --output-dir ABSENT_OR_EMPTY_PATH --dsp-arch v68 [--host-compat-lib-dir PATH]" >&2
    exit 2
fi
if [[ ! "$dsp_arch" =~ ^v[0-9]+$ ]]; then
    echo "invalid DSP architecture: $dsp_arch" >&2
    exit 2
fi

qaic="$sdk_root/ipc/fastrpc/qaic/Ubuntu20/qaic"
hexagon_bin="$sdk_root/tools/HEXAGON_Tools/8.7.06/Tools/bin"
compiler="$hexagon_bin/hexagon-clang"
readelf_tool="$hexagon_bin/hexagon-readelf"
objcopy_tool="$hexagon_bin/hexagon-llvm-objcopy"
qurt_include="$sdk_root/rtos/qurt/compute${dsp_arch}/include/qurt"
for required in "$qaic" "$compiler" "$readelf_tool" "$objcopy_tool" \
    "$sdk_root/incs/stddef/AEEStdDef.idl" "$sdk_root/incs/remote.idl" \
    "$qurt_include/qurt_mutex.h"; do
    if [[ ! -e "$required" ]]; then
        echo "required SDK input is missing: $required" >&2
        exit 1
    fi
done
if [[ -z "$host_compat_lib_dir" && -n "${HOME:-}" &&
      -d "$HOME/.local/lib/hexagon-sdk-compat" ]]; then
    host_compat_lib_dir="$HOME/.local/lib/hexagon-sdk-compat"
fi
if [[ -n "$host_compat_lib_dir" ]]; then
    if [[ ! -d "$host_compat_lib_dir" ]]; then
        echo "host compatibility library directory is missing: $host_compat_lib_dir" >&2
        exit 1
    fi
    export LD_LIBRARY_PATH="$host_compat_lib_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if [[ -e "$output_dir" ]] && [[ -n "$(find "$output_dir" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
    echo "output directory must be absent or empty: $output_dir" >&2
    exit 1
fi

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source_root=$(cd "$script_dir/../.." && pwd)
adapter_dir="$source_root/src/adapters/qualcomm"
v1_dir="$adapter_dir/dsp/v1"
idl="$v1_dir/vqec_vision_dsp_v1.idl"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/vqec-vision-dsp-v1.XXXXXX")
trap 'rm -rf -- "$work_dir"' EXIT
generated_dir="$work_dir/generated"
mkdir -p "$generated_dir"

qaic_version=$($qaic -v | sed -n '1p')
if [[ ! "$qaic_version" =~ "version 01.00.47" ]]; then
    echo "unsupported QAIC: $qaic_version" >&2
    exit 1
fi
$qaic -mdll -o "$generated_dir" -I "$sdk_root/incs/stddef" \
    -I "$sdk_root/incs" "$idl"

artifact="$work_dir/libvqec_vision_dsp_v1_skel.so"
compile_flags=("-m$dsp_arch" -O2 -std=c11 -Wall -Wextra -Werror -fPIC
    -frandom-seed=vqec_vision_dsp_v1
    "-ffile-prefix-map=$work_dir=/vqec_dsp_build"
    "-fdebug-prefix-map=$work_dir=/vqec_dsp_build"
    -I "$source_root/include" -I "$v1_dir" -I "$generated_dir"
    -I "$sdk_root/incs" -I "$sdk_root/incs/stddef" -I "$qurt_include")
sources=("$generated_dir/vqec_vision_dsp_v1_skel.c"
    "$v1_dir/vqec_vision_dsp_v1_skeleton.c"
    "$v1_dir/vqec_vision_dsp_v1_service.c"
    "$v1_dir/vqec_vision_dsp_v1_image.c"
    "$v1_dir/vqec_vision_dsp_v1_overlay.c"
    "$v1_dir/vqec_vision_dsp_v1_dense.c"
    "$v1_dir/vqec_vision_dsp_v1_wire.c")
objects=("$work_dir/01_skel.o" "$work_dir/02_service_binding.o"
    "$work_dir/03_service.o" "$work_dir/04_image.o" "$work_dir/05_overlay.o"
    "$work_dir/06_dense.o" "$work_dir/07_wire.o")
for index in "${!sources[@]}"; do
    "$compiler" "${compile_flags[@]}" -c "${sources[$index]}" \
        -o "${objects[$index]}"
done
"$compiler" "-m$dsp_arch" -shared -Wl,-no-threads "${objects[@]}" -o "$artifact"
canonical_artifact="$work_dir/libvqec_vision_dsp_v1_skel.canonical.so"
"$objcopy_tool" --remove-section .note.llvm.cgmdinfo --remove-section .comment \
    "$artifact" "$canonical_artifact"
mv "$canonical_artifact" "$artifact"

symbol_table="$work_dir/symbols.txt"
"$readelf_tool" -Ws "$artifact" >"$symbol_table"
for symbol in vqec_vision_dsp_v1_skel_handle_invoke vqec_vision_dsp_v1_open \
    vqec_vision_dsp_v1_close vqec_vision_dsp_v1_query_capabilities \
    vqec_vision_dsp_v1_execute; do
    if ! grep -Eq "[[:space:]]${symbol}$" "$symbol_table"; then
        echo "required skeleton symbol is missing: $symbol" >&2
        exit 1
    fi
done

receipt="$work_dir/vqec_vision_dsp_v1_build_receipt.txt"
{
    echo "abi=v1"
    echo "dsp_arch=$dsp_arch"
    echo "qaic=$qaic_version"
    "$compiler" --version | sed -n '1p' | sed 's/^/compiler=/'
    echo "compile_flags=-m$dsp_arch -O2 -std=c11 -Wall -Wextra -Werror -fPIC"
    echo "link_flags=-m$dsp_arch -shared -Wl,-no-threads"
    echo "canonicalization=remove .note.llvm.cgmdinfo and .comment"
    for source in "$script_dir/vqec_vision_build_dsp_v1.sh" \
        "$source_root/include/vqec/vision/ai/contracts/base/vqec_vision_version_registry.h" \
        "$idl" "$v1_dir/vqec_vision_dsp_v1_skeleton.c" \
        "$v1_dir/vqec_vision_dsp_v1_service.c" \
        "$v1_dir/vqec_vision_dsp_v1_service.h" \
        "$v1_dir/vqec_vision_dsp_v1_image.c" \
        "$v1_dir/vqec_vision_dsp_v1_image.h" \
        "$v1_dir/vqec_vision_dsp_v1_overlay.c" \
        "$v1_dir/vqec_vision_dsp_v1_overlay.h" \
        "$v1_dir/vqec_vision_dsp_v1_dense.c" \
        "$v1_dir/vqec_vision_dsp_v1_dense.h" \
        "$v1_dir/vqec_vision_dsp_v1_wire.c" \
        "$v1_dir/vqec_vision_dsp_v1_wire.h"; do
        digest=$(sha256sum "$source" | cut -d' ' -f1)
        echo "sha256.project.$(basename "$source")=$digest"
    done
    for source in "$sdk_root/incs/stddef/AEEStdDef.idl" "$sdk_root/incs/remote.idl"; do
        digest=$(sha256sum "$source" | cut -d' ' -f1)
        echo "sha256.sdk.$(basename "$source")=$digest"
    done
    for source in "$generated_dir/vqec_vision_dsp_v1.h" \
        "$generated_dir/vqec_vision_dsp_v1_skel.c" \
        "$generated_dir/vqec_vision_dsp_v1_stub.c"; do
        digest=$(sha256sum "$source" | cut -d' ' -f1)
        echo "sha256.generated.$(basename "$source")=$digest"
    done
    digest=$(sha256sum "$artifact" | cut -d' ' -f1)
    echo "sha256.artifact.$(basename "$artifact")=$digest"
} >"$receipt"

mkdir -p "$output_dir"
cp "$artifact" "$receipt" "$symbol_table" "$output_dir/"
cp -R "$generated_dir" "$output_dir/"
echo "built $output_dir/$(basename "$artifact")"
