#!/usr/bin/env bash
# Capture the authorized QCS6490 RTSP preview, measure effective packet cadence and emit
# a contact sheet for explicit human overlay review. This tool does not auto-approve boxes.
set -euo pipefail

uri=""
output_dir=""
duration_seconds="8"
expected_width="1920"
expected_height="1080"
expected_fps="25"
fps_tolerance="1.0"

while (($# > 0)); do
    case "$1" in
        --uri) uri=${2:-}; shift 2 ;;
        --output-dir) output_dir=${2:-}; shift 2 ;;
        --duration-seconds) duration_seconds=${2:-}; shift 2 ;;
        --expected-width) expected_width=${2:-}; shift 2 ;;
        --expected-height) expected_height=${2:-}; shift 2 ;;
        --expected-fps) expected_fps=${2:-}; shift 2 ;;
        --fps-tolerance) fps_tolerance=${2:-}; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

if [[ -z "$uri" || -z "$output_dir" ]]; then
    echo "usage: $0 --uri rtsp://192.168.138.98:PORT/PATH --output-dir PATH [--duration-seconds N] [--expected-width N] [--expected-height N] [--expected-fps N] [--fps-tolerance N]" >&2
    exit 2
fi
if [[ ! "$uri" =~ ^rtsp://192\.168\.138\.98(:[0-9]+)?/ ]]; then
    echo "preview acceptance is restricted to the authorized target 192.168.138.98" >&2
    exit 2
fi
if [[ "$uri" == *"@"* ]]; then
    echo "credential-bearing RTSP URIs are prohibited" >&2
    exit 2
fi
if [[ ! "$duration_seconds" =~ ^[1-9][0-9]*$ ]] ||
   [[ ! "$expected_width" =~ ^[1-9][0-9]*$ ]] ||
   [[ ! "$expected_height" =~ ^[1-9][0-9]*$ ]]; then
    echo "duration and dimensions must be positive integers" >&2
    exit 2
fi
if ! awk -v fps="$expected_fps" -v tolerance="$fps_tolerance" \
    'BEGIN { exit !(fps > 0 && tolerance >= 0) }'; then
    echo "expected FPS must be positive and tolerance must be non-negative" >&2
    exit 2
fi
for required_tool in ffmpeg ffprobe awk sed; do
    if ! command -v "$required_tool" >/dev/null 2>&1; then
        echo "required host tool is missing: $required_tool" >&2
        exit 1
    fi
done
if [[ -e "$output_dir" ]] &&
   [[ -n "$(find "$output_dir" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
    echo "output directory must be absent or empty: $output_dir" >&2
    exit 2
fi
mkdir -p "$output_dir"

capture="$output_dir/capture.mkv"
metrics="$output_dir/metrics.txt"
contact_sheet="$output_dir/overlay_contact_sheet.png"

ffmpeg -nostdin -hide_banner -loglevel warning -rtsp_transport tcp -i "$uri" \
    -map 0:v:0 -t "$duration_seconds" -an -c:v copy -y "$capture"

probe_output=$(ffprobe -v error -select_streams v:0 -count_packets \
    -show_entries stream=codec_name,width,height,avg_frame_rate,nb_read_packets \
    -show_entries format=duration -of default=noprint_wrappers=1 "$capture")
codec=$(printf '%s\n' "$probe_output" | sed -n 's/^codec_name=//p' | head -n1)
width=$(printf '%s\n' "$probe_output" | sed -n 's/^width=//p' | head -n1)
height=$(printf '%s\n' "$probe_output" | sed -n 's/^height=//p' | head -n1)
declared_rate=$(printf '%s\n' "$probe_output" | sed -n 's/^avg_frame_rate=//p' | head -n1)
packet_count=$(printf '%s\n' "$probe_output" | sed -n 's/^nb_read_packets=//p' | head -n1)
captured_duration=$(printf '%s\n' "$probe_output" | sed -n 's/^duration=//p' | head -n1)

if [[ "$codec" != "h264" || "$width" != "$expected_width" ||
      "$height" != "$expected_height" || ! "$packet_count" =~ ^[1-9][0-9]*$ ]]; then
    printf '%s\n' "$probe_output" >&2
    echo "captured stream metadata does not meet the requested profile" >&2
    exit 1
fi
effective_fps=$(awk -v packets="$packet_count" -v duration="$captured_duration" \
    'BEGIN { if (duration <= 0) exit 1; printf "%.3f", packets / duration }')
if ! awk -v actual="$effective_fps" -v expected="$expected_fps" -v tolerance="$fps_tolerance" \
    'BEGIN { delta = actual - expected; if (delta < 0) delta = -delta; exit !(delta <= tolerance) }'; then
    echo "effective FPS $effective_fps is outside $expected_fps +/- $fps_tolerance" >&2
    exit 1
fi

ffmpeg -nostdin -hide_banner -loglevel warning -i "$capture" \
    -vf "fps=1,scale=960:-2,tile=2x2:nb_frames=4:padding=4:margin=4" \
    -frames:v 1 -y "$contact_sheet"

{
    echo "target=192.168.138.98"
    echo "codec=$codec"
    echo "width=$width"
    echo "height=$height"
    echo "declared_avg_frame_rate=$declared_rate"
    echo "captured_packets=$packet_count"
    echo "captured_duration_seconds=$captured_duration"
    echo "effective_fps=$effective_fps"
    echo "expected_fps=$expected_fps"
    echo "fps_tolerance=$fps_tolerance"
    echo "fps_gate=PASS"
    echo "overlay_visual_gate=REVIEW_REQUIRED"
} >"$metrics"

printf 'PASS stream profile and packet cadence; visual review required: %s\n' "$contact_sheet"
