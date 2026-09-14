#!/bin/sh
# Deploy the LACAI board camera services to a QCS6490 target.
#
# It installs the built camera service binary, the YOLOv8n-person model package and, when
# given, the model library, then writes and starts two systemd units:
#
#   lacai-camera.service  pure QMMF -> H.264 RTSP view of the raw camera
#   lacai-ai.service      QMMF -> LACAI person model -> overlay -> H.264 RTSP
#
# Only the AI unit requires the model library. The script never stores credentials; use an
# SSH key or the environment credential helper for the target.
#
# Usage:
#   vqec_vision_board_deploy.sh <ssh-host> [model-library-on-target]
# Environment overrides:
#   VQEC_VISION_CAMERA_SERVICE_BIN  built vqec_vision_ai_camera_service (default build-esdk-full)
#   VQEC_VISION_MODEL_KIT           model package directory (default yolov8n_person manifest)
#   VQEC_VISION_MODEL_SOURCE        host path to upload as the target model library
#   VQEC_VISION_TARGET_ROOT         install root on the target (default /opt/anhtdh)
set -eu

HOST="${1:?usage: vqec_vision_board_deploy.sh <ssh-host> [model-library-on-target]}"
TARGET_ROOT="${VQEC_VISION_TARGET_ROOT:-/opt/anhtdh}"
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
BIN="${VQEC_VISION_CAMERA_SERVICE_BIN:-$REPO/build-esdk-full/src/app/vqec_vision_ai_camera_service}"
KIT="${VQEC_VISION_MODEL_KIT:-$REPO/manifests/models/yolov8n_person}"
MODEL_LIBRARY="${2:-$TARGET_ROOT/models/libyolov8n_person_w8a16.so}"
SSH="ssh -o BatchMode=yes $HOST"
SCP="scp -q"

if [ ! -x "$BIN" ]; then
    echo "camera service binary not found: $BIN" >&2
    exit 1
fi
if [ ! -d "$KIT" ]; then
    echo "model kit not found: $KIT" >&2
    exit 1
fi

$SSH "mkdir -p '$TARGET_ROOT/bin' '$TARGET_ROOT/modelkit' '$TARGET_ROOT/models' '$TARGET_ROOT/out' '$TARGET_ROOT/inputs'"
$SCP "$BIN" "$HOST:$TARGET_ROOT/bin/vqec_vision_ai_camera_service"
$SCP "$KIT"/model_metadata.json "$KIT"/io_manifest.json "$KIT"/preprocess.json \
    "$KIT"/decoder.json "$KIT"/labels.txt "$HOST:$TARGET_ROOT/modelkit/"
if [ -n "${VQEC_VISION_MODEL_SOURCE:-}" ]; then
    $SCP "$VQEC_VISION_MODEL_SOURCE" "$HOST:$MODEL_LIBRARY"
fi

cat > /tmp/lacai-camera.service <<'EOF'
[Unit]
Description=LACAI QMMF camera RTSP service
After=network.target
[Service]
Type=simple
ExecStart=/opt/anhtdh/camera_service/run_camera_rtsp.sh 0 1280 720 30 8900 /live
Restart=on-failure
RestartSec=2
[Install]
WantedBy=multi-user.target
EOF

cat > /tmp/lacai-ai.service <<EOF
[Unit]
Description=LACAI AI camera service (QMMF + QNN + RTSP overlay)
After=network.target
[Service]
Type=simple
Environment=ADSP_LIBRARY_PATH=/usr/lib/rfsa/adsp;/lib/rfsa/adsp;/dsp
ExecStart=$TARGET_ROOT/bin/vqec_vision_ai_camera_service --package $TARGET_ROOT/modelkit --model $MODEL_LIBRARY --backend /usr/lib/libQnnHtp.so --system /usr/lib/libQnnSystem.so --width 1280 --height 720 --fps 30 --rtsp-port 8900 --rtsp-mount /live
Restart=on-failure
RestartSec=2
[Install]
WantedBy=multi-user.target
EOF

$SSH 'cat > /opt/anhtdh/camera_service/run_camera_rtsp.sh' <<'EOF'
#!/bin/sh
set -e
CAMERA_ID="${1:-0}"
WIDTH="${2:-1280}"
HEIGHT="${3:-720}"
FPS="${4:-30}"
PORT="${5:-8900}"
MOUNT="${6:-/live}"
exec gst-launch-1.0 -e \
  qtiqmmfsrc name=camsrc camera="${CAMERA_ID}" \
  ! video/x-raw,format=NV12,width="${WIDTH}",height="${HEIGHT}",framerate="${FPS}/1" \
  ! v4l2h264enc extra-controls="controls,video_bitrate=6000000,h264_i_frame_period=30" \
  ! h264parse config-interval=1 \
  ! qtirtspbin address=0.0.0.0 port="${PORT}" mpoint="${MOUNT}"
EOF
$SCP /tmp/lacai-camera.service "$HOST:/etc/systemd/system/lacai-camera.service"
$SCP /tmp/lacai-ai.service "$HOST:/etc/systemd/system/lacai-ai.service"
$SSH "chmod +x '$TARGET_ROOT/camera_service/run_camera_rtsp.sh'; systemctl daemon-reload; systemctl enable lacai-camera.service lacai-ai.service >/dev/null 2>&1 || true"
echo "deployed. Start one service at a time (only one owns the camera):"
echo "  $SSH systemctl start lacai-ai.service      # AI + overlay RTSP"
echo "  $SSH systemctl start lacai-camera.service  # raw camera RTSP"
echo "RTSP: rtsp://<host>:8900/live"
