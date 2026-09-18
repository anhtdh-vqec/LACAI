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
g_preview_fps=${LACAI_PREVIEW_FPS:-30}
g_output_surface_count=${LACAI_OUTPUT_SURFACE_COUNT:-8}
g_cpu_set=${LACAI_CPU_SET:-4-7}
g_dma_heap=${LACAI_DMA_HEAP:-/dev/dma_heap/qcom,system}
g_start_timeout_seconds=${LACAI_START_TIMEOUT_SECONDS:-30}
g_stop_timeout_seconds=${LACAI_STOP_TIMEOUT_SECONDS:-30}
g_runtime_step_interval_us=${LACAI_RUNTIME_STEP_INTERVAL_US:-8000}
g_service=${LACAI_SERVICE:-$g_root/bin/vqec_ai_vision_applications}
g_dsp_v1_dir=${LACAI_DSP_V1_DIR:-$g_root/dsp/v1}
g_deployment=${LACAI_DEPLOYMENT:-$g_root/config/deployment_full.json}
g_model_catalog=${LACAI_MODEL_CATALOG:-$g_root/config/model_catalog.json}
g_model_registry=${LACAI_MODEL_REGISTRY:-$g_root/config/model_registry.json}
g_usecase_snapshot=${LACAI_USECASE_SNAPSHOT:-$g_root/config/usecase_control_snapshot_full.json}
g_hardware_profile=${LACAI_HARDWARE_PROFILE:-$g_root/config/hardware_admission_profile.json}
g_action=${1:-start}

for numeric_value in "$g_rtsp_port" "$g_preview_fps" \
    "$g_output_surface_count" "$g_start_timeout_seconds" \
    "$g_stop_timeout_seconds" "$g_runtime_step_interval_us"; do
    case "$numeric_value" in
        ''|0|*[!0-9]*)
            echo "runtime numeric settings must be positive integers" >&2
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
    vqec_vision_ai_tools_rnful_stop_component "$g_run_dir/camera.pid" \
        vqec_vision_fw_camera_sim.py
    rm -f "$g_camera_socket" "$g_ring_path"
}

vqec_vision_ai_tools_rnful_require_file() {
    if [ ! -f "$1" ]; then
        echo "required file is missing: $1" >&2
        exit 1
    fi
}

vqec_vision_ai_tools_rnful_report_status() {
    result=0
    for component in camera service rtsp; do
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
    echo "vlc=rtsp://192.168.138.98:$g_rtsp_port$g_rtsp_mount"
    return "$result"
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
            "$g_root/out/rtsp.log"
        exit 0
        ;;
    restart)
        vqec_vision_ai_tools_rnful_stop_all
        ;;
    start)
        ;;
    *)
        echo "usage: $0 [start|stop|restart|status|logs]" >&2
        exit 2
        ;;
esac

for required_command in python3 setsid taskset dbus-run-session; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "required command is missing: $required_command" >&2
        exit 1
    fi
done
for required_file in \
    "$g_service" \
    "$g_dsp_v1_dir/libvqec_vision_dsp_v1_skel.so" \
    "$g_root/tools/fixtures/vqec_vision_fw_camera_sim.py" \
    "$g_root/tools/fixtures/vqec_vision_ring_rtsp.py" \
    "$g_deployment" \
    "$g_model_catalog" \
    "$g_model_registry" \
    "$g_usecase_snapshot" \
    "$g_hardware_profile"; do
    vqec_vision_ai_tools_rnful_require_file "$required_file"
done
if [ ! -c "$g_dma_heap" ]; then
    echo "registered DMA-BUF heap is unavailable: $g_dma_heap" >&2
    exit 1
fi
if [ -f "$g_run_dir/camera.pid" ] || [ -f "$g_run_dir/service.pid" ] ||
   [ -f "$g_run_dir/rtsp.pid" ]; then
    echo "managed state already exists; run '$0 stop' first" >&2
    exit 1
fi

mkdir -p "$g_run_dir" "$g_camera_socket_dir" "$g_root/out" \
    /run/lacai_fr_index
chmod 0700 "$g_run_dir" /run/lacai_fr_index
rm -f "$g_camera_socket" "$g_ring_path"

setsid python3 "$g_root/tools/fixtures/vqec_vision_fw_camera_sim.py" \
    --socket-dir "$g_camera_socket_dir" --camera 0 --channel 0 --consumer ai \
    --width 1920 --height 1080 --fps 30 --max-in-flight 3 \
    --dma-heap "$g_dma_heap" >"$g_root/out/camera.log" 2>&1 </dev/null &
g_camera_pid=$!
echo "$g_camera_pid" >"$g_run_dir/camera.pid"

g_wait_count=0
g_wait_limit=$((g_start_timeout_seconds * 10))
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

setsid env \
    LD_LIBRARY_PATH="$g_root/lib" \
    ADSP_LIBRARY_PATH="$g_dsp_v1_dir;/usr/lib/dsp/cdsp/cv/v68/KODIAK;/usr/lib/rfsa/adsp;/dsp" \
    DSP_LIBRARY_PATH="$g_dsp_v1_dir;/usr/lib/dsp/cdsp/cv/v68/KODIAK;/usr/lib/rfsa/adsp;/dsp" \
    taskset -c "$g_cpu_set" dbus-run-session -- \
    "$g_service" \
    --mode production --platform qualcomm \
    --model-execution parallel \
    --runtime-step-interval-us "$g_runtime_step_interval_us" \
    --deployment "$g_deployment" \
    --model-catalog "$g_model_catalog" \
    --usecase-snapshot "$g_usecase_snapshot" \
    --model-package-registry "$g_model_registry" \
    --qnn-backend-library /usr/lib/libQnnHtp.so \
    --qnn-system-library /usr/lib/libQnnSystem.so \
    --model-root "$g_root/models" \
    --dsp-v1-skel-dir "$g_dsp_v1_dir" \
    --dsp-enable-unsigned-pd \
    --hardware-profile "$g_hardware_profile" \
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

g_wait_count=0
while [ ! -f "$g_ring_path" ] && kill -0 "$g_service_pid" 2>/dev/null &&
      [ "$g_wait_count" -lt "$g_wait_limit" ]; do
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

echo "LACAI full workload is running"
echo "Open VLC: rtsp://192.168.138.98:$g_rtsp_port$g_rtsp_mount"
echo "Status: $0 status"
echo "Stop:   $0 stop"
