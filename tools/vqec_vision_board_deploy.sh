#!/bin/sh
# Deploy the LACAI board test helpers to a QCS6490 target:
#   - the QMMF raw camera RTSP view (lacai-camera.service)
#   - the mock FW camera service (vqec_vision_fw_camera_sim.py)
#
# The mock publishes raw NV12 frames to the FW wire socket so LACAI's own camera
# adapter can consume them, and owns the Camera1 D-Bus control name.
#
# Usage: vqec_vision_board_deploy.sh <ssh-host>
# Environment overrides:
#   VQEC_VISION_TARGET_ROOT  install root on the target (default /opt/anhtdh)
set -eu

HOST="${1:?usage: vqec_vision_board_deploy.sh <ssh-host>}"
TARGET_ROOT="${VQEC_VISION_TARGET_ROOT:-/opt/anhtdh}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SSH="ssh -o BatchMode=yes $HOST"
SCP="scp -q"

$SSH "mkdir -p '$TARGET_ROOT/bin' '$TARGET_ROOT/camera_service' /run/camera_ai"
$SCP "$HERE/vqec_vision_fw_camera_sim.py" "$HOST:$TARGET_ROOT/bin/vqec_vision_fw_camera_sim.py"

$SSH "cat > $TARGET_ROOT/camera_service/run_camera_rtsp.sh" <<'EOF'
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
$SCP "$HERE/lacai-camera.service" "$HOST:/etc/systemd/system/lacai-camera.service" 2>/dev/null || \
$SSH "cat > /etc/systemd/system/lacai-camera.service" <<'EOF'
[Unit]
Description=LACAI QMMF raw camera RTSP view
After=network.target
[Service]
Type=simple
ExecStart=/opt/anhtdh/camera_service/run_camera_rtsp.sh 0 1280 720 30 8900 /live
Restart=on-failure
RestartSec=2
[Install]
WantedBy=multi-user.target
EOF
$SSH "chmod +x '$TARGET_ROOT/camera_service/run_camera_rtsp.sh'; systemctl daemon-reload; systemctl enable lacai-camera.service >/dev/null 2>&1 || true"
echo "deployed."
echo "  raw camera view : $SSH systemctl start lacai-camera.service   -> rtsp://<host>:8900/live"
echo "  mock FW service : $SSH python3 $TARGET_ROOT/bin/vqec_vision_fw_camera_sim.py"
echo "Only one owner may hold the camera at a time."
