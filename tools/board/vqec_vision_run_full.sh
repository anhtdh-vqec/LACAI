#!/bin/sh
# Start the complete compatibility-board workload and expose its preview through RTSP.
# Deployment choices remain overridable through LACAI_* environment variables.
set -eu

g_root=${LACAI_ROOT:-/opt/lacai}
g_run_dir=${LACAI_RUN_DIR:-/run/lacai}
g_camera_socket_dir=${LACAI_CAMERA_SOCKET_DIR:-/run/camera_ai}
g_camera_socket="$g_camera_socket_dir/0_third_ai.sock"
g_ring_id=${LACAI_RING_ID:-encoded_ai_detect0_cam0_ch0}
g_ring_path="/dev/shm/camera_ai_$g_ring_id"
g_rtsp_port=${LACAI_RTSP_PORT:-8554}
g_rtsp_mount=${LACAI_RTSP_MOUNT:-/live/ai/detect0}
g_board_address=${LACAI_BOARD_ADDRESS:-192.168.0.102}
g_preview_fps=${LACAI_PREVIEW_FPS:-30}
g_output_surface_count=${LACAI_OUTPUT_SURFACE_COUNT:-8}
g_cpu_set=${LACAI_CPU_SET:-4-7}
g_dma_heap=${LACAI_DMA_HEAP:-/dev/dma_heap/qcom,system}
g_start_timeout_seconds=${LACAI_START_TIMEOUT_SECONDS:-30}
g_camera_start_delay_seconds=${LACAI_CAMERA_START_DELAY_SECONDS:-5}
g_preview_ready_timeout_seconds=${LACAI_PREVIEW_READY_TIMEOUT_SECONDS:-120}
g_stop_timeout_seconds=${LACAI_STOP_TIMEOUT_SECONDS:-30}
g_runtime_step_interval_us=${LACAI_RUNTIME_STEP_INTERVAL_US:-8000}
g_source_recovery_backoff_ms=${LACAI_SOURCE_RECOVERY_BACKOFF_MS:-1000}
g_service=${LACAI_SERVICE:-$g_root/bin/vqec_ai_vision_applications}
g_app_manager=${LACAI_APP_MANAGER:-$g_root/bin/vqec_vision_app_manager}
g_app_control=${LACAI_APP_CONTROL:-$g_root/bin/vqec_vision_app_manager_control}
g_evidence_probe=${LACAI_EVIDENCE_PROBE:-$g_root/bin/vqec_vision_evidence_probe}
g_dsp_v1_dir=${LACAI_DSP_V1_DIR:-$g_root/dsp/v1}
g_deployment=${LACAI_DEPLOYMENT:-$g_root/config/deployment_full.json}
g_model_catalog=${LACAI_MODEL_CATALOG:-$g_root/config/model_catalog.json}
g_model_registry=${LACAI_MODEL_REGISTRY:-$g_root/config/model_registry.json}
g_feature_catalog=${LACAI_FEATURE_CATALOG:-$g_root/config/feature_catalog.json}
g_usecase_snapshot=${LACAI_USECASE_SNAPSHOT:-$g_root/config/usecase_control_snapshot_full.json}
g_hardware_profile=${LACAI_HARDWARE_PROFILE:-$g_root/config/hardware_admission_profile.json}
g_metadata_profile=${LACAI_METADATA_PROFILE:-$g_root/config/metadata_runtime_profile.json}
g_evidence_socket=${LACAI_EVIDENCE_SOCKET:-$g_run_dir/evidence.sock}
g_evidence_state_dir=${LACAI_EVIDENCE_STATE_DIR:-$g_root/data/evidence}
g_evidence_outbox=${LACAI_EVIDENCE_OUTBOX:-$g_evidence_state_dir/outbox.db}
g_evidence_receiver_database=${LACAI_EVIDENCE_RECEIVER_DATABASE:-$g_evidence_state_dir/reference_inbox.db}
g_evidence_reference_receiver=${LACAI_EVIDENCE_REFERENCE_RECEIVER:-1}
g_evidence_peer_uid=${LACAI_EVIDENCE_PEER_UID:-0}
g_evidence_io_timeout_ms=${LACAI_EVIDENCE_IO_TIMEOUT_MS:-500}
g_evidence_busy_timeout_ms=${LACAI_EVIDENCE_BUSY_TIMEOUT_MS:-1000}
g_evidence_outbox_max_bytes=${LACAI_EVIDENCE_OUTBOX_MAX_BYTES:-67108864}
g_evidence_initial_retry_ms=${LACAI_EVIDENCE_INITIAL_RETRY_MS:-100}
g_evidence_maximum_retry_ms=${LACAI_EVIDENCE_MAXIMUM_RETRY_MS:-10000}
g_evidence_idle_poll_ms=${LACAI_EVIDENCE_IDLE_POLL_MS:-50}
g_evidence_stop_drain_ms=${LACAI_EVIDENCE_STOP_DRAIN_MS:-2000}
g_evidence_maximum_attempts=${LACAI_EVIDENCE_MAXIMUM_ATTEMPTS:-12}
g_app_state_dir=${LACAI_APP_STATE_DIR:-$g_root/data/app_manager}
g_app_database=${LACAI_APP_DATABASE:-$g_app_state_dir/apps.db}
g_app_content_store_dir=${LACAI_APP_CONTENT_STORE_DIR:-$g_root/models/app_content}
g_app_content_store_bytes=${LACAI_APP_CONTENT_STORE_BYTES:-1073741824}
g_app_content_blob_bytes=${LACAI_APP_CONTENT_BLOB_BYTES:-536870912}
g_app_content_blob_count=${LACAI_APP_CONTENT_BLOB_COUNT:-2048}
g_app_public_key=${LACAI_APP_PUBLIC_KEY:-$g_root/config/trust/app_manager_public.pem}
g_app_catalog=${LACAI_APP_CATALOG:-$g_root/config/usecase_app_catalog.json}
g_app_manifest=${LACAI_APP_MANIFEST:-$g_root/config/app_package/usecase_app_manifest.fire_smoke.json}
g_app_configuration=${LACAI_APP_CONFIGURATION:-$g_root/config/app_package/fire_smoke_configuration.json}
g_app_package_signature=${LACAI_APP_PACKAGE_SIGNATURE:-$g_root/config/app_package/fire_smoke_package.sig}
g_app_entitlement=${LACAI_APP_ENTITLEMENT:-$g_root/config/app_package/fire_smoke_entitlement.json}
g_app_entitlement_signature=${LACAI_APP_ENTITLEMENT_SIGNATURE:-$g_root/config/app_package/fire_smoke_entitlement.sig}
g_app_model_component=${LACAI_APP_MODEL_COMPONENT:-$g_root/models/yolo11n_fire_smoke/libyolo11n_replaymix_w8a16.so}
g_app_labels_component=${LACAI_APP_LABELS_COMPONENT:-$g_root/models/yolo11n_fire_smoke/package/labels.txt}
g_app_service_name=${LACAI_APP_SERVICE_NAME:-com.vqec.AiVision.AppManager}
g_app_backend_name=${LACAI_APP_BACKEND_NAME:-com.vqec.AiVision.Backend}
g_app_runtime_name=${LACAI_APP_RUNTIME_NAME:-com.vqec.AiVision.Runtime}
g_app_object_path=${LACAI_APP_OBJECT_PATH:-/com/vqec/AiVision/AppManager}
g_app_id=${LACAI_APP_ID:-security.fire_smoke_detection}
g_app_source_id=${LACAI_APP_SOURCE_ID:-camera_front}
g_app_target_id=${LACAI_APP_TARGET_ID:-qcs6490_qlinux_1_8}
g_app_key_id=${LACAI_APP_KEY_ID:-vqec_product_signing_key}
g_app_rpc_timeout_ms=${LACAI_APP_RPC_TIMEOUT_MS:-5000}
g_app_poll_interval_ms=${LACAI_APP_POLL_INTERVAL_MS:-100}
g_toggle_interval_seconds=${LACAI_TOGGLE_INTERVAL_SECONDS:-5}
g_toggle_cycles=${LACAI_TOGGLE_CYCLES:-6}
g_max_stress_fd_growth=${LACAI_MAX_STRESS_FD_GROWTH:-8}
g_max_stress_rss_growth_kib=${LACAI_MAX_STRESS_RSS_GROWTH_KIB:-32768}
g_operation_state_committed=5
g_operation_state_cancelled=7
g_operation_state_failed=8
g_operation_state_recovery_required=9
g_operation_result_ok=0
g_action=${1:-start}

for numeric_value in "$g_rtsp_port" "$g_preview_fps" \
    "$g_output_surface_count" "$g_start_timeout_seconds" \
    "$g_camera_start_delay_seconds" \
    "$g_preview_ready_timeout_seconds" "$g_stop_timeout_seconds" \
    "$g_runtime_step_interval_us" "$g_evidence_io_timeout_ms" \
    "$g_source_recovery_backoff_ms" \
    "$g_evidence_busy_timeout_ms" "$g_evidence_outbox_max_bytes" \
    "$g_evidence_initial_retry_ms" "$g_evidence_maximum_retry_ms" \
    "$g_evidence_idle_poll_ms" "$g_evidence_stop_drain_ms" \
    "$g_evidence_maximum_attempts" "$g_app_content_store_bytes" \
    "$g_app_content_blob_bytes" "$g_app_content_blob_count"; do
    case "$numeric_value" in
        ''|0|*[!0-9]*)
            echo "runtime numeric settings must be positive integers" >&2
            exit 2
            ;;
    esac
done
case "$g_evidence_reference_receiver" in
    0|1) ;;
    *) echo "LACAI_EVIDENCE_REFERENCE_RECEIVER must be 0 or 1" >&2; exit 2 ;;
esac
case "$g_evidence_peer_uid" in
    ''|*[!0-9]*) echo "evidence peer UID must be a non-negative integer" >&2; exit 2 ;;
esac
for numeric_value in "$g_toggle_interval_seconds" "$g_toggle_cycles"; do
    case "$numeric_value" in
        ''|0|*[!0-9]*)
            echo "toggle settings must be positive integers" >&2
            exit 2
            ;;
    esac
done
for numeric_value in "$g_max_stress_fd_growth" "$g_max_stress_rss_growth_kib"; do
    case "$numeric_value" in
        ''|*[!0-9]*)
            echo "stress growth limits must be non-negative integers" >&2
            exit 2
            ;;
    esac
done

vqec_vision_ai_tools_rnful_stop_component() {
    pid_file=$1
    expected=$2
    [ -f "$pid_file" ] || return 0
    pid=$(sed -n '1p' "$pid_file")
    case "$pid" in
        ''|*[!0-9]*) rm -f "$pid_file"; return 0 ;;
    esac
    if [ -r "/proc/$pid/cmdline" ] &&
       tr '\000' ' ' <"/proc/$pid/cmdline" | grep -F "$expected" >/dev/null 2>&1; then
        kill -TERM "-$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
        attempts=0
        stop_attempt_limit=$((g_stop_timeout_seconds * 10))
        while kill -0 "$pid" 2>/dev/null &&
              [ "$attempts" -lt "$stop_attempt_limit" ]; do
            sleep 0.1
            attempts=$((attempts + 1))
        done
        if kill -0 "$pid" 2>/dev/null; then
            kill -KILL "-$pid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null || true
        fi
    fi
    rm -f "$pid_file"
}

vqec_vision_ai_tools_rnful_stop_all() {
    vqec_vision_ai_tools_rnful_stop_component "$g_run_dir/rtsp.pid" \
        vqec_vision_ring_rtsp.py
    vqec_vision_ai_tools_rnful_stop_component "$g_run_dir/service.pid" \
        vqec_ai_vision_applications
    vqec_vision_ai_tools_rnful_stop_component "$g_run_dir/evidence_receiver.pid" \
        vqec_vision_evidence_receiver.py
    vqec_vision_ai_tools_rnful_stop_component "$g_run_dir/app_manager.pid" \
        vqec_vision_app_manager
    vqec_vision_ai_tools_rnful_stop_component "$g_run_dir/camera.pid" \
        vqec_vision_fw_camera_sim.py
    vqec_vision_ai_tools_rnful_stop_component "$g_run_dir/app_bus.pid" \
        dbus-daemon
    rm -f "$g_camera_socket" "$g_ring_path" "$g_evidence_socket" \
        "$g_run_dir/app_bus.address"
}

vqec_vision_ai_tools_rnful_require_file() {
    if [ ! -f "$1" ]; then
        echo "required file is missing: $1" >&2
        exit 1
    fi
}

vqec_vision_ai_tools_rnful_report_status() {
    result=0
    for component in app_bus camera service app_manager rtsp; do
        pid_file="$g_run_dir/$component.pid"
        if [ -f "$pid_file" ]; then
            pid=$(sed -n '1p' "$pid_file")
        else
            pid=
        fi
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            echo "$component=running pid=$pid"
        else
            echo "$component=stopped"
            result=1
        fi
    done
    if [ "$g_evidence_reference_receiver" -eq 1 ]; then
        pid=$(sed -n '1p' "$g_run_dir/evidence_receiver.pid" 2>/dev/null || true)
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            echo "evidence_receiver=running pid=$pid"
        else
            echo "evidence_receiver=stopped"
            result=1
        fi
    else
        echo "evidence_receiver=external"
    fi
    echo "vlc=rtsp://$g_board_address:$g_rtsp_port$g_rtsp_mount"
    return "$result"
}

g_snapshot_path="$g_run_dir/app_snapshot.json"

vqec_vision_ai_tools_rnful_control() {
    "$g_app_control" "$@" --service-name "$g_app_service_name" \
        --client-name "$g_app_backend_name" --object-path "$g_app_object_path" \
        --rpc-timeout-ms "$g_app_rpc_timeout_ms" --session
}

vqec_vision_ai_tools_rnful_request_digest() {
    printf '%s' "$1" | sha256sum | cut -d ' ' -f 1
}

vqec_vision_ai_tools_rnful_submit_operation() {
    submission=$(vqec_vision_ai_tools_rnful_control "$@")
    operation_id=$(printf '%s\n' "$submission" | sed -n 's/^operation_id=//p')
    if [ -z "$operation_id" ]; then
        echo "App Manager submission returned no operation ID" >&2
        return 1
    fi
    printf '%s\n' "$operation_id"
}

vqec_vision_ai_tools_rnful_snapshot_field() {
    python3 - "$g_snapshot_path" "$1" <<'PY'
import json
import sys
with open(sys.argv[1], "r", encoding="utf-8") as stream:
    document = json.load(stream)
print(document[sys.argv[2]])
PY
}

vqec_vision_ai_tools_rnful_association_field() {
    python3 - "$g_snapshot_path" "$g_app_id" "$1" <<'PY'
import json
import sys
with open(sys.argv[1], "r", encoding="utf-8") as stream:
    document = json.load(stream)
association = next((item for item in document["associations"]
                    if item["app_id"] == sys.argv[2]), None)
value = None if association is None else association.get(sys.argv[3])
if isinstance(value, bool):
    print("true" if value else "false")
elif value is not None:
    print(value)
PY
}

vqec_vision_ai_tools_rnful_wait_operation() {
    operation_id=$1
    expected_state=$2
    operation_path="$g_run_dir/app_operation.txt"
    wait_count=0
    wait_limit=$((g_preview_ready_timeout_seconds * 10))
    while [ "$wait_count" -lt "$wait_limit" ]; do
        if vqec_vision_ai_tools_rnful_control operation \
            --operation-id "$operation_id" >"$operation_path" 2>/dev/null; then
            operation_state=$(sed -n 's/^state=//p' "$operation_path")
            result_code=$(sed -n 's/^result_code=//p' "$operation_path")
            if [ "$operation_state" = "$expected_state" ] &&
               [ "$result_code" = "$g_operation_result_ok" ]; then
                cat "$operation_path"
                return 0
            fi
            case "$operation_state" in
                "$g_operation_state_cancelled"|"$g_operation_state_failed"|\
                "$g_operation_state_recovery_required")
                    cat "$operation_path" >&2
                    echo "App Manager operation failed: $operation_id" >&2
                    return 1
                    ;;
            esac
        fi
        sleep 0.1
        wait_count=$((wait_count + 1))
    done
    echo "App Manager operation timed out: $operation_id" >&2
    return 1
}

vqec_vision_ai_tools_rnful_ring_sequence() {
    python3 - "$g_ring_path" <<'PY'
import struct
import sys

try:
    with open(sys.argv[1], "rb", buffering=0) as stream:
        header = stream.read(40)
    if len(header) != 40:
        raise ValueError("short ring header")
    version, header_size, slot_count, payload_size = struct.unpack_from("<IIII", header, 4)
    if version != 5 or header_size < 4096 or slot_count == 0 or payload_size == 0:
        raise ValueError("invalid ring header")
    print(struct.unpack_from("<Q", header, 32)[0])
except (OSError, ValueError, struct.error):
    raise SystemExit(1)
PY
}

vqec_vision_ai_tools_rnful_process_metric() {
    process_id=$1
    metric=$2
    case "$metric" in
        fd)
            find "/proc/$process_id/fd" -mindepth 1 -maxdepth 1 -type l 2>/dev/null |
                wc -l
            ;;
        rss)
            awk '/^VmRSS:/ { print $2; found=1 } END { if (!found) exit 1 }' \
                "/proc/$process_id/status"
            ;;
        *) return 2 ;;
    esac
}

vqec_vision_ai_tools_rnful_stress_toggle() {
    vqec_vision_ai_tools_rnful_require_file "$g_app_control"
    vqec_vision_ai_tools_rnful_require_file "$g_run_dir/app_bus.address"
    DBUS_SESSION_BUS_ADDRESS=$(sed -n '1p' "$g_run_dir/app_bus.address")
    export DBUS_SESSION_BUS_ADDRESS
    for component in app_bus camera service app_manager rtsp; do
        pid=$(sed -n '1p' "$g_run_dir/$component.pid" 2>/dev/null || true)
        if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
            echo "cannot stress a stopped component: $component" >&2
            return 1
        fi
    done
    if [ "$g_evidence_reference_receiver" -eq 1 ]; then
        pid=$(sed -n '1p' "$g_run_dir/evidence_receiver.pid" 2>/dev/null || true)
        if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
            echo "cannot stress a stopped component: evidence_receiver" >&2
            return 1
        fi
    fi
    service_pid=$(sed -n '1p' "$g_run_dir/service.pid")
    baseline_fds=$(vqec_vision_ai_tools_rnful_process_metric "$service_pid" fd)
    baseline_rss_kib=$(vqec_vision_ai_tools_rnful_process_metric "$service_pid" rss)
    cycle=1
    while [ "$cycle" -le "$g_toggle_cycles" ]; do
        vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path"
        revision=$(vqec_vision_ai_tools_rnful_snapshot_field desired_revision)
        before=$(vqec_vision_ai_tools_rnful_ring_sequence 2>/dev/null || true)
        before=${before:-0}
        operation_id="desired.disable.$revision.$cycle"
        request_digest=$(vqec_vision_ai_tools_rnful_request_digest \
            "$g_app_id|$g_app_source_id|$revision|false")
        operation_id=$(vqec_vision_ai_tools_rnful_submit_operation desired \
            --app-id "$g_app_id" \
            --source-id "$g_app_source_id" --expected-revision "$revision" \
            --enabled false --idempotency-key "$operation_id" \
            --request-sha256 "$request_digest")
        vqec_vision_ai_tools_rnful_wait_operation \
            "$operation_id" "$g_operation_state_committed" >/dev/null
        sleep "$g_toggle_interval_seconds"
        vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path"
        if [ "$(vqec_vision_ai_tools_rnful_association_field desired)" != "false" ]; then
            echo "disable did not commit at stress cycle $cycle" >&2
            return 1
        fi
        revision=$(vqec_vision_ai_tools_rnful_snapshot_field desired_revision)
        operation_id="desired.enable.$revision.$cycle"
        request_digest=$(vqec_vision_ai_tools_rnful_request_digest \
            "$g_app_id|$g_app_source_id|$revision|true")
        operation_id=$(vqec_vision_ai_tools_rnful_submit_operation desired \
            --app-id "$g_app_id" \
            --source-id "$g_app_source_id" --expected-revision "$revision" \
            --enabled true --idempotency-key "$operation_id" \
            --request-sha256 "$request_digest")
        vqec_vision_ai_tools_rnful_wait_operation \
            "$operation_id" "$g_operation_state_committed" >/dev/null
        sleep "$g_toggle_interval_seconds"
        vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path"
        if [ "$(vqec_vision_ai_tools_rnful_association_field desired)" != "true" ]; then
            echo "enable did not commit at stress cycle $cycle" >&2
            return 1
        fi
        after=$(vqec_vision_ai_tools_rnful_ring_sequence 2>/dev/null || true)
        after=${after:-0}
        if [ "$after" -le "$before" ]; then
            echo "encoded preview ring did not resume at stress cycle $cycle" >&2
            return 1
        fi
        for component in camera service app_manager rtsp; do
            pid=$(sed -n '1p' "$g_run_dir/$component.pid")
            if ! kill -0 "$pid" 2>/dev/null; then
                echo "$component stopped at stress cycle $cycle" >&2
                return 1
            fi
        done
        if [ "$g_evidence_reference_receiver" -eq 1 ]; then
            pid=$(sed -n '1p' "$g_run_dir/evidence_receiver.pid")
            if ! kill -0 "$pid" 2>/dev/null; then
                echo "evidence_receiver stopped at stress cycle $cycle" >&2
                return 1
            fi
        fi
        current_fds=$(vqec_vision_ai_tools_rnful_process_metric "$service_pid" fd)
        current_rss_kib=$(vqec_vision_ai_tools_rnful_process_metric "$service_pid" rss)
        echo "toggle_cycle=$cycle desired=true ring_before=$before ring_after=$after "\
             "service_fds=$current_fds service_rss_kib=$current_rss_kib"
        cycle=$((cycle + 1))
    done
    final_fds=$(vqec_vision_ai_tools_rnful_process_metric "$service_pid" fd)
    final_rss_kib=$(vqec_vision_ai_tools_rnful_process_metric "$service_pid" rss)
    if [ "$final_fds" -gt $((baseline_fds + g_max_stress_fd_growth)) ]; then
        echo "service FD growth exceeded stress limit: baseline=$baseline_fds final=$final_fds" >&2
        return 1
    fi
    if [ "$final_rss_kib" -gt $((baseline_rss_kib + g_max_stress_rss_growth_kib)) ]; then
        echo "service RSS growth exceeded stress limit: baseline=$baseline_rss_kib "\
             "final=$final_rss_kib" >&2
        return 1
    fi
    echo "stress_resources baseline_fds=$baseline_fds final_fds=$final_fds "\
         "baseline_rss_kib=$baseline_rss_kib final_rss_kib=$final_rss_kib"
}

vqec_vision_ai_tools_rnful_configure() {
    vqec_vision_ai_tools_rnful_require_file "$g_app_control"
    vqec_vision_ai_tools_rnful_require_file "$g_app_configuration"
    vqec_vision_ai_tools_rnful_require_file "$g_run_dir/app_bus.address"
    DBUS_SESSION_BUS_ADDRESS=$(sed -n '1p' "$g_run_dir/app_bus.address")
    export DBUS_SESSION_BUS_ADDRESS
    service_pid=$(sed -n '1p' "$g_run_dir/service.pid" 2>/dev/null || true)
    app_manager_pid=$(sed -n '1p' "$g_run_dir/app_manager.pid" 2>/dev/null || true)
    if [ -z "$service_pid" ] || ! kill -0 "$service_pid" 2>/dev/null ||
       [ -z "$app_manager_pid" ] || ! kill -0 "$app_manager_pid" 2>/dev/null; then
        echo "service and App Manager must be running before configure" >&2
        return 1
    fi
    vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path"
    expected_revision=$(vqec_vision_ai_tools_rnful_association_field configuration_revision)
    expected_digest=$(sha256sum "$g_app_configuration" | cut -d ' ' -f 1)
    operation_id="configure.$expected_digest"
    before=$(vqec_vision_ai_tools_rnful_ring_sequence 2>/dev/null || true)
    before=${before:-0}
    operation_id=$(vqec_vision_ai_tools_rnful_submit_operation configure \
        --app-id "$g_app_id" \
        --configuration "$g_app_configuration" \
        --configuration-sha256 "$expected_digest" \
        --expected-revision "$expected_revision" \
        --idempotency-key "$operation_id" \
        --request-sha256 "$expected_digest")
    vqec_vision_ai_tools_rnful_wait_operation \
        "$operation_id" "$g_operation_state_committed" >/dev/null
    expected_revision=$((expected_revision + 1))
    wait_count=0
    wait_limit=$((g_preview_ready_timeout_seconds * 10))
    while [ "$wait_count" -lt "$wait_limit" ]; do
        vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path"
        current_revision=$(vqec_vision_ai_tools_rnful_association_field configuration_revision)
        current_digest=$(vqec_vision_ai_tools_rnful_association_field configuration_sha256)
        after=$(vqec_vision_ai_tools_rnful_ring_sequence 2>/dev/null || true)
        after=${after:-0}
        if [ "$current_revision" = "$expected_revision" ] &&
           [ "$current_digest" = "$expected_digest" ] &&
           [ "$after" -gt "$before" ]; then
            echo "configuration_revision=$current_revision digest=$current_digest "\
                 "ring_before=$before ring_after=$after"
            return 0
        fi
        if ! kill -0 "$service_pid" 2>/dev/null; then
            echo "service stopped while applying configuration" >&2
            return 1
        fi
        sleep 0.1
        wait_count=$((wait_count + 1))
    done
    echo "configuration committed but runtime preview did not reconcile in time" >&2
    return 1
}

vqec_vision_ai_tools_rnful_probe_evidence() {
    vqec_vision_ai_tools_rnful_require_file "$g_evidence_probe"
    if [ ! -S "$g_evidence_socket" ]; then
        echo "evidence receiver socket is unavailable: $g_evidence_socket" >&2
        return 1
    fi
    request_id="evidence.probe.$(date +%s)"
    "$g_evidence_probe" --socket "$g_evidence_socket" \
        --outbox "$g_run_dir/evidence_probe.db" \
        --source-id "$g_app_source_id" --request-id "$request_id" \
        --peer-uid "$g_evidence_peer_uid" --timeout-ms 5000
}

case "$g_action" in
    stop)
        vqec_vision_ai_tools_rnful_stop_all
        echo "LACAI full workload stopped"
        exit 0
        ;;
    status)
        vqec_vision_ai_tools_rnful_report_status
        exit $?
        ;;
    logs)
        tail -n 80 "$g_root/out/camera.log" "$g_root/out/service.log" \
            "$g_root/out/app_manager.log" "$g_root/out/evidence_receiver.log" \
            "$g_root/out/rtsp.log"
        exit 0
        ;;
    stress)
        vqec_vision_ai_tools_rnful_stress_toggle
        echo "LACAI App Manager toggle stress passed"
        exit 0
        ;;
    configure)
        vqec_vision_ai_tools_rnful_configure
        echo "LACAI App Manager configuration reconcile passed"
        exit 0
        ;;
    evidence-probe)
        vqec_vision_ai_tools_rnful_probe_evidence
        echo "LACAI evidence reference flow passed"
        exit 0
        ;;
    restart)
        vqec_vision_ai_tools_rnful_stop_all
        ;;
    start)
        ;;
    *)
        echo "usage: $0 [start|stop|restart|status|logs|stress|configure|evidence-probe]" >&2
        exit 2
        ;;
esac

for required_command in python3 setsid taskset sha256sum dbus-daemon; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "required command is missing: $required_command" >&2
        exit 1
    fi
done
for required_file in \
    "$g_service" \
    "$g_app_manager" \
    "$g_app_control" \
    "$g_dsp_v1_dir/libvqec_vision_dsp_v1_skel.so" \
    "$g_root/tools/fixtures/vqec_vision_fw_camera_sim.py" \
    "$g_root/tools/fixtures/vqec_vision_ring_rtsp.py" \
    "$g_deployment" \
    "$g_model_catalog" \
    "$g_model_registry" \
    "$g_feature_catalog" \
    "$g_usecase_snapshot" \
    "$g_hardware_profile"; do
    vqec_vision_ai_tools_rnful_require_file "$required_file"
done
if [ "$g_evidence_reference_receiver" -eq 1 ]; then
    vqec_vision_ai_tools_rnful_require_file \
        "$g_root/tools/fixtures/vqec_vision_evidence_receiver.py"
fi
vqec_vision_ai_tools_rnful_require_file "$g_metadata_profile"
for app_file in "$g_app_public_key" "$g_app_catalog" "$g_app_manifest" "$g_app_configuration" \
    "$g_app_package_signature" "$g_app_entitlement" \
    "$g_app_entitlement_signature" "$g_app_model_component" \
    "$g_app_labels_component"; do
    vqec_vision_ai_tools_rnful_require_file "$app_file"
done
if [ ! -c "$g_dma_heap" ]; then
    echo "registered DMA-BUF heap is unavailable: $g_dma_heap" >&2
    exit 1
fi
if [ -f "$g_run_dir/app_bus.pid" ] || [ -f "$g_run_dir/camera.pid" ] ||
   [ -f "$g_run_dir/service.pid" ] ||
   [ -f "$g_run_dir/evidence_receiver.pid" ] ||
   [ -f "$g_run_dir/app_manager.pid" ] ||
   [ -f "$g_run_dir/rtsp.pid" ]; then
    echo "managed state already exists; run '$0 stop' first" >&2
    exit 1
fi

mkdir -p "$g_run_dir" "$g_camera_socket_dir" "$g_root/out" \
    /run/lacai_fr_index "$g_app_state_dir" "$g_app_content_store_dir" \
    "$g_evidence_state_dir"
chmod 0700 "$g_run_dir" /run/lacai_fr_index "$g_app_state_dir" \
    "$g_app_content_store_dir" "$g_evidence_state_dir"
rm -f "$g_camera_socket" "$g_ring_path" "$g_evidence_socket"

g_bus_details=$(dbus-daemon --session --fork --print-address=1 --print-pid=1)
DBUS_SESSION_BUS_ADDRESS=$(printf '%s\n' "$g_bus_details" | sed -n '1p')
g_app_bus_pid=$(printf '%s\n' "$g_bus_details" | sed -n '2p')
case "$g_app_bus_pid" in
    ''|*[!0-9]*)
        echo "private App Manager D-Bus did not return a valid PID" >&2
        exit 1
        ;;
esac
export DBUS_SESSION_BUS_ADDRESS
echo "$g_app_bus_pid" >"$g_run_dir/app_bus.pid"
printf '%s\n' "$DBUS_SESSION_BUS_ADDRESS" >"$g_run_dir/app_bus.address"
chmod 0600 "$g_run_dir/app_bus.address"

setsid env \
    LD_LIBRARY_PATH="$g_root/lib" \
    ADSP_LIBRARY_PATH="$g_dsp_v1_dir;/usr/lib/dsp/cdsp/cv/v68/KODIAK;/usr/lib/rfsa/adsp;/dsp" \
    DSP_LIBRARY_PATH="$g_dsp_v1_dir;/usr/lib/dsp/cdsp/cv/v68/KODIAK;/usr/lib/rfsa/adsp;/dsp" \
    taskset -c "$g_cpu_set" \
    "$g_service" \
    --mode production --platform qualcomm \
    --model-execution parallel \
    --runtime-step-interval-us "$g_runtime_step_interval_us" \
    --source-recovery-backoff-ms "$g_source_recovery_backoff_ms" \
    --deployment "$g_deployment" \
    --model-catalog "$g_model_catalog" \
    --feature-catalog "$g_feature_catalog" \
    --usecase-snapshot "$g_usecase_snapshot" \
    --model-package-registry "$g_model_registry" \
    --qnn-backend-library /usr/lib/libQnnHtp.so \
    --qnn-system-library /usr/lib/libQnnSystem.so \
    --model-root "$g_root/models" \
    --dsp-v1-skel-dir "$g_dsp_v1_dir" \
    --dsp-enable-unsigned-pd \
    --hardware-profile "$g_hardware_profile" \
    --metadata-profile "$g_metadata_profile" \
    --evidence-socket "$g_evidence_socket" \
    --evidence-outbox "$g_evidence_outbox" \
    --evidence-peer-uid "$g_evidence_peer_uid" \
    --evidence-io-timeout-ms "$g_evidence_io_timeout_ms" \
    --evidence-outbox-busy-timeout-ms "$g_evidence_busy_timeout_ms" \
    --evidence-outbox-max-bytes "$g_evidence_outbox_max_bytes" \
    --evidence-initial-retry-ms "$g_evidence_initial_retry_ms" \
    --evidence-maximum-retry-ms "$g_evidence_maximum_retry_ms" \
    --evidence-idle-poll-ms "$g_evidence_idle_poll_ms" \
    --evidence-stop-drain-ms "$g_evidence_stop_drain_ms" \
    --evidence-maximum-attempts "$g_evidence_maximum_attempts" \
    --app-manager-dbus-session \
    --app-manager-service-name "$g_app_service_name" \
    --app-manager-client-name "$g_app_runtime_name" \
    --app-manager-object-path "$g_app_object_path" \
    --app-manager-rpc-timeout-ms "$g_app_rpc_timeout_ms" \
    --app-manager-poll-interval-ms "$g_app_poll_interval_ms" \
    --tracker-contract portable.iou.tracker.v1 \
    --event-schema-id reference.zone --event-schema-version 1 \
    --consumer-id-prefix lacai_ai \
    --camera-socket-dir "$g_camera_socket_dir" --camera-producer-uid 0 \
    --nv12-format 23 --output-ring-id "$g_ring_id" \
    --output-fps "$g_preview_fps" --output-bitrate 4000000 \
    --output-keyframe-interval 30 --output-box-color-rgba 0x00ff00ff \
    --output-surface-count "$g_output_surface_count" --output-colorimetry bt709 \
    --output-interlace-mode progressive \
    --fr-gallery-path /run/lacai_fr_index/face_protected_1 \
    --fr-protected-directory "$g_root/protected_gallery" \
    --fr-gallery-file gallery.bin --fr-key-file gallery.key \
    --fr-lock-file gallery.lock --fr-gallery-id face_protected_1 \
    --fr-preprocess-revision 1 --fr-store-max-bytes 16777216 \
    --fr-min-similarity 0.35 --fr-subject-margin 0.05 \
    --fr-max-templates 5 --fr-top-k 5 \
    --fr-feature-id face_recognition --fr-identity-attribute subject_ref \
    >"$g_root/out/service.log" 2>&1 </dev/null &
g_service_pid=$!
echo "$g_service_pid" >"$g_run_dir/service.pid"
g_wait_limit=$((g_start_timeout_seconds * 10))

# Deliberately start the runtime before App Manager. A missing manager must keep
# the runtime disabled; it must never cause Qualcomm graph/DSP/HTP loading.
sleep 1
if ! kill -0 "$g_service_pid" 2>/dev/null; then
    echo "service stopped while App Manager was offline" >&2
    tail -n 80 "$g_root/out/service.log" >&2 || true
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi

# The service intentionally starts first. A missing receiver leaves durable work in the
# outbox and must not affect camera/model startup. The reference receiver is AI-owned test
# infrastructure; set LACAI_EVIDENCE_REFERENCE_RECEIVER=0 for a released external receiver.
if [ "$g_evidence_reference_receiver" -eq 1 ]; then
    setsid python3 "$g_root/tools/fixtures/vqec_vision_evidence_receiver.py" \
        --socket "$g_evidence_socket" --database "$g_evidence_receiver_database" \
        --expected-uid "$g_evidence_peer_uid" \
        >"$g_root/out/evidence_receiver.log" 2>&1 </dev/null &
    g_evidence_receiver_pid=$!
    echo "$g_evidence_receiver_pid" >"$g_run_dir/evidence_receiver.pid"
    g_wait_count=0
    while [ ! -S "$g_evidence_socket" ] &&
          kill -0 "$g_evidence_receiver_pid" 2>/dev/null &&
          [ "$g_wait_count" -lt "$g_wait_limit" ]; do
        sleep 0.1
        g_wait_count=$((g_wait_count + 1))
    done
    if [ ! -S "$g_evidence_socket" ]; then
        echo "evidence reference receiver did not become ready" >&2
        vqec_vision_ai_tools_rnful_stop_all
        exit 1
    fi
fi

setsid "$g_app_manager" \
    --target "$g_app_target_id" \
    --device-id "$(sed -n '1p' /etc/machine-id)" \
    --max-resident-bytes 536870912 --max-tensor-bytes 134217728 \
    --max-active-incidents 32 --max-events-per-second 64 \
    --app-catalog "$g_app_catalog" \
    --database "$g_app_database" --max-database-bytes 67108864 \
    --content-store "$g_app_content_store_dir" \
    --max-content-store-bytes "$g_app_content_store_bytes" \
    --max-content-blob-bytes "$g_app_content_blob_bytes" \
    --max-content-blob-count "$g_app_content_blob_count" \
    --busy-timeout-ms 5000 --public-key "$g_app_public_key" \
    --key-id "$g_app_key_id" --service-name "$g_app_service_name" \
    --object-path "$g_app_object_path" \
    --trusted-backend-name "$g_app_backend_name" \
    --trusted-runtime-name "$g_app_runtime_name" \
    --rpc-timeout-ms "$g_app_rpc_timeout_ms" --callbacks-per-poll 16 \
    --poll-interval-ms 10 --session \
    >"$g_root/out/app_manager.log" 2>&1 </dev/null &
g_app_manager_pid=$!
echo "$g_app_manager_pid" >"$g_run_dir/app_manager.pid"

g_wait_count=0
while ! vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path" 2>/dev/null &&
      kill -0 "$g_app_manager_pid" 2>/dev/null &&
      [ "$g_wait_count" -lt "$g_wait_limit" ]; do
    sleep 0.1
    g_wait_count=$((g_wait_count + 1))
done
if [ ! -s "$g_snapshot_path" ]; then
    echo "App Manager did not become ready" >&2
    tail -n 80 "$g_root/out/app_manager.log" >&2 || true
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi

if [ "$(vqec_vision_ai_tools_rnful_association_field entitled)" != "true" ]; then
    g_entitlement_digest=$(sha256sum "$g_app_entitlement" | cut -d ' ' -f 1)
    g_entitlement_operation="entitlement.$g_entitlement_digest"
    g_entitlement_operation=$(vqec_vision_ai_tools_rnful_submit_operation entitlement \
        --grant "$g_app_entitlement" --signature "$g_app_entitlement_signature" \
        --grant-sha256 "$g_entitlement_digest" --app-id "$g_app_id" \
        --idempotency-key "$g_entitlement_operation" \
        --request-sha256 "$g_entitlement_digest")
    vqec_vision_ai_tools_rnful_wait_operation \
        "$g_entitlement_operation" "$g_operation_state_committed"
    vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path"
fi
if [ "$(vqec_vision_ai_tools_rnful_association_field installed)" != "true" ]; then
    g_inventory_revision=$(vqec_vision_ai_tools_rnful_snapshot_field inventory_revision)
    g_manifest_digest=$(sha256sum "$g_app_manifest" | cut -d ' ' -f 1)
    g_install_operation="install.$g_manifest_digest"
    g_install_operation=$(vqec_vision_ai_tools_rnful_submit_operation install \
        --manifest "$g_app_manifest" --configuration "$g_app_configuration" \
        --signature "$g_app_package_signature" \
        --component "$g_app_model_component" \
        --component "$g_app_labels_component" \
        --manifest-sha256 "$g_manifest_digest" \
        --configuration-sha256 "$(sha256sum "$g_app_configuration" | cut -d ' ' -f 1)" \
        --app-id "$g_app_id" --idempotency-key "$g_install_operation" \
        --request-sha256 "$g_manifest_digest" \
        --expected-revision "$g_inventory_revision")
    vqec_vision_ai_tools_rnful_wait_operation \
        "$g_install_operation" "$g_operation_state_committed"
    vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path"
fi
if [ "$(vqec_vision_ai_tools_rnful_association_field desired)" != "true" ]; then
    g_desired_revision=$(vqec_vision_ai_tools_rnful_snapshot_field desired_revision)
    g_desired_operation="desired.enable.$g_desired_revision.start"
    g_desired_digest=$(vqec_vision_ai_tools_rnful_request_digest \
        "$g_app_id|$g_app_source_id|$g_desired_revision|true")
    g_desired_operation=$(vqec_vision_ai_tools_rnful_submit_operation desired \
        --app-id "$g_app_id" \
        --source-id "$g_app_source_id" --expected-revision "$g_desired_revision" \
        --enabled true --idempotency-key "$g_desired_operation" \
        --request-sha256 "$g_desired_digest")
    vqec_vision_ai_tools_rnful_wait_operation \
        "$g_desired_operation" "$g_operation_state_committed"
    vqec_vision_ai_tools_rnful_control snapshot >"$g_snapshot_path"
fi

# Start the FW camera producer last. This is the canonical dependency-order test:
# the AI service must survive with neither App Manager nor media available, and the
# Qualcomm graph/DSP/HTP path must remain unopened until a real frame is received.
sleep "$g_camera_start_delay_seconds"
if ! kill -0 "$g_service_pid" 2>/dev/null; then
    echo "service stopped while camera media was unavailable" >&2
    tail -n 80 "$g_root/out/service.log" >&2 || true
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi
if grep -F '/libQnnHtp.so' "/proc/$g_service_pid/maps" >/dev/null 2>&1; then
    echo "Qualcomm HTP backend loaded before camera media was available" >&2
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi
setsid python3 "$g_root/tools/fixtures/vqec_vision_fw_camera_sim.py" \
    --socket-dir "$g_camera_socket_dir" --camera 0 --channel 0 --consumer ai \
    --width 1920 --height 1080 --fps 30 --max-in-flight 3 \
    --dma-heap "$g_dma_heap" >"$g_root/out/camera.log" 2>&1 </dev/null &
g_camera_pid=$!
echo "$g_camera_pid" >"$g_run_dir/camera.pid"

g_wait_count=0
while [ ! -S "$g_camera_socket" ] && kill -0 "$g_camera_pid" 2>/dev/null &&
      [ "$g_wait_count" -lt "$g_wait_limit" ]; do
    sleep 0.1
    g_wait_count=$((g_wait_count + 1))
done
if [ ! -S "$g_camera_socket" ]; then
    echo "camera fixture did not become ready" >&2
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi

g_wait_count=0
g_preview_wait_limit=$((g_preview_ready_timeout_seconds * 10))
while [ ! -f "$g_ring_path" ] && kill -0 "$g_service_pid" 2>/dev/null &&
      [ "$g_wait_count" -lt "$g_preview_wait_limit" ]; do
    sleep 0.1
    g_wait_count=$((g_wait_count + 1))
done
if [ ! -f "$g_ring_path" ]; then
    echo "service did not publish the encoded ring" >&2
    tail -n 80 "$g_root/out/service.log" >&2 || true
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi

setsid python3 "$g_root/tools/fixtures/vqec_vision_ring_rtsp.py" read \
    --ring-id "$g_ring_id" --port "$g_rtsp_port" --mount "$g_rtsp_mount" \
    --fps "$g_preview_fps" >"$g_root/out/rtsp.log" 2>&1 </dev/null &
g_rtsp_pid=$!
echo "$g_rtsp_pid" >"$g_run_dir/rtsp.pid"
sleep 2
if ! kill -0 "$g_camera_pid" 2>/dev/null ||
   ! kill -0 "$g_service_pid" 2>/dev/null ||
   ! kill -0 "$g_rtsp_pid" 2>/dev/null; then
    echo "one or more full-workload components stopped during startup" >&2
    tail -n 80 "$g_root/out/camera.log" "$g_root/out/service.log" \
        "$g_root/out/rtsp.log" >&2 || true
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi
if [ "$g_evidence_reference_receiver" -eq 1 ] &&
   ! kill -0 "$g_evidence_receiver_pid" 2>/dev/null; then
    echo "evidence reference receiver stopped during startup" >&2
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi

g_wait_count=0
while [ "$(vqec_vision_ai_tools_rnful_ring_sequence 2>/dev/null || echo 0)" -eq 0 ] &&
      kill -0 "$g_service_pid" 2>/dev/null &&
      kill -0 "$g_rtsp_pid" 2>/dev/null &&
      [ "$g_wait_count" -lt "$g_preview_wait_limit" ]; do
    sleep 0.1
    g_wait_count=$((g_wait_count + 1))
done
if [ "$(vqec_vision_ai_tools_rnful_ring_sequence 2>/dev/null || echo 0)" -eq 0 ]; then
    echo "preview did not publish its first encoded ring frame" >&2
    tail -n 80 "$g_root/out/camera.log" "$g_root/out/service.log" \
        "$g_root/out/rtsp.log" >&2 || true
    vqec_vision_ai_tools_rnful_stop_all
    exit 1
fi

echo "LACAI full workload is running"
echo "Open VLC: rtsp://$g_board_address:$g_rtsp_port$g_rtsp_mount"
echo "Status: $0 status"
echo "Stop:   $0 stop"
