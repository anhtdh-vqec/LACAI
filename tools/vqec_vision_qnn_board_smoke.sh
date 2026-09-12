#!/usr/bin/env bash
# Board-side QNN model smoke helper. Runs qnn-net-run against one model library and one
# input list using a pinned QAIRT runtime, so a model can be validated before LACAI is
# wired to it. Read-only with respect to the repository; it writes only the output dir.
#
# Usage:
#   vqec_vision_qnn_board_smoke.sh \
#     --qairt /root/qairt/2.43.0.260128 \
#     --model /path/libscrfd_500m_bnkps_w8a16.so \
#     --input-list /path/input_list.txt \
#     --output-dir /tmp/lacai_qnn_smoke/scrfd

set -euo pipefail

qairt_root=""
model=""
input_list=""
output_dir=""
target="aarch64-oe-linux-gcc11.2"

usage() {
    printf '%s\n' "usage: $0 --qairt ROOT --model MODEL.so --input-list FILE --output-dir DIR [--target TARGET]"
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qairt) qairt_root="$2"; shift 2 ;;
        --model) model="$2"; shift 2 ;;
        --input-list) input_list="$2"; shift 2 ;;
        --output-dir) output_dir="$2"; shift 2 ;;
        --target) target="$2"; shift 2 ;;
        *) usage ;;
    esac
done

[[ -n "$qairt_root" && -n "$model" && -n "$input_list" && -n "$output_dir" ]] || usage
[[ -f "$qairt_root/lib/$target/libQnnHtp.so" ]] || { echo "missing libQnnHtp.so under $qairt_root/lib/$target" >&2; exit 1; }
[[ -f "$model" ]] || { echo "missing model: $model" >&2; exit 1; }
[[ -f "$input_list" ]] || { echo "missing input list: $input_list" >&2; exit 1; }

export LD_LIBRARY_PATH="$qairt_root/lib/$target:${LD_LIBRARY_PATH:-}"
export ADSP_LIBRARY_PATH="$qairt_root/lib/hexagon-v68/unsigned;/usr/lib/rfsa/adsp;/lib/rfsa/adsp;/dsp"
export PATH="$qairt_root/bin/$target:$PATH"

rm -rf "$output_dir"
mkdir -p "$output_dir"

echo "== qnn-platform-validator =="
qnn-platform-validator --backend htp || true

echo "== qnn-net-run =="
qnn-net-run \
    --backend "$qairt_root/lib/$target/libQnnHtp.so" \
    --model "$model" \
    --input_list "$input_list" \
    --output_dir "$output_dir"

echo "== outputs =="
find "$output_dir" -type f | sort
