#!/bin/sh
# Runs the cross-built native test binaries on the QCS6490 target and supplies the fixtures
# two device-free tests need. Without fixtures those tests print a usage error and exit
# non-zero; this runner is the reproducible way to obtain board evidence.
#
# Usage:
#   vqec_vision_board_native_tests.sh <test_dir> <manifest_models_dir> \
#       <zvec_tmpfs_root> <zvec_scratch_base>
#
# - <manifest_models_dir> must contain scrfd_500m_bnkps/decoder.json and
#   edgeface_s_gamma_05/decoder.json (stage the repository manifests/models tree).
# - <zvec_tmpfs_root> must be an existing current-UID-owned mode-0700 tmpfs directory.
# - <zvec_scratch_base> must be a writable directory on a NON-tmpfs filesystem; the default
#   private-index policy rejects its collection path, which the test asserts.
set -eu

test_dir="${1:?test directory required}"
manifest_models="${2:?manifest models directory required}"
zvec_tmpfs_root="${3:?zvec tmpfs root required}"
zvec_scratch_base="${4:?zvec scratch base required}"

if [ ! -d "$manifest_models" ]; then
    echo "manifest models directory is missing: $manifest_models" >&2
    exit 2
fi
if [ ! -f "$manifest_models/scrfd_500m_bnkps/decoder.json" ] ||
   [ ! -f "$manifest_models/edgeface_s_gamma_05/decoder.json" ]; then
    echo "manifest models directory lacks the decoder fixtures: $manifest_models" >&2
    exit 2
fi
owner=$(stat -c '%u' "$zvec_tmpfs_root" 2>/dev/null || echo missing)
mode=$(stat -c '%a' "$zvec_tmpfs_root" 2>/dev/null || echo 0)
if [ "$owner" != "$(id -u)" ] || [ "$mode" != "700" ]; then
    echo "zvec tmpfs root must be current-UID mode-0700: $zvec_tmpfs_root" >&2
    exit 2
fi

zvec_collection="$zvec_scratch_base/collection"
private_probe="$zvec_tmpfs_root"
rm -rf "$zvec_collection"

pass=0
fail=0
failed=""
cd "$test_dir"
for test_binary in vqec_vision_ai_*; do
    [ -x "$test_binary" ] || continue
    status=0
    case "$test_binary" in
        vqec_vision_ai_decoder_package_test)
            ./"$test_binary" "$manifest_models" >/tmp/vqec_native_test.out 2>&1 || status=$?
            ;;
        vqec_vision_ai_zvec_embedding_index_test)
            ./"$test_binary" "$zvec_collection" "$private_probe" \
                >/tmp/vqec_native_test.out 2>&1 || status=$?
            ;;
        *)
            ./"$test_binary" >/tmp/vqec_native_test.out 2>&1 || status=$?
            ;;
    esac
    if [ "$status" -eq 0 ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        failed="$failed $test_binary"
        echo "---- $test_binary (exit $status) ----" >&2
        cat /tmp/vqec_native_test.out >&2
    fi
done
rm -rf "$zvec_collection"

echo "PASS=$pass FAIL=$fail"
if [ -n "$failed" ]; then
    echo "FAILED:$failed"
fi
[ "$fail" -eq 0 ]
