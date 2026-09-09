# Qualcomm plugins reference — QSC6490 / Qualcomm Linux 1.8

Ngày cập nhật: 2026-09-09. Repository được đọc trực tiếp:
`/home/a/Workspace/gst-plugins-qti-oss`.
Source snapshot đã kiểm tra: `0cdf24a99c625fa616564ebf82fd8813c744ed82`.

Phần hướng dẫn tích hợp và ranh giới adapter hiện hành nằm tại
[qualcomm plugin adapter reference](../architecture/qualcomm_plugin_adapter_reference.md).
Tài liệu này giữ inventory/evidence chi tiết; không được dùng các default trong
phụ lục như cấu hình triển khai cố định.

## 1. Baseline và phạm vi sử dụng tài liệu

Người dùng xác nhận các plugin đã chạy và được tối ưu trên **QSC6490,
Qualcomm Linux 1.8**. Đây là baseline thiết bị do người dùng cung cấp, không phải
benchmark do agent chạy. Giữ nguyên tên QSC6490 như cung cấp; không tự sửa SoC ID.
Không cần sysroot để viết source; mọi CMake/build/test của LACAI phải dùng eSDK
tại `/home/a/Workspace/eSDK` theo rule dự án.

Tài liệu kiểm kê **54 lời gọi đăng ký factory** trong checkout hiện tại;
đọc registration/properties/pads toàn bộ và đọc sâu converter/QNN/metadata/batch/
sample wiring phục vụ AI. Đây là reference API/source, không phải audit đầy đủ
mọi thuật toán, camera HAL, codec hoặc proprietary SDK. Không tuyên bố hiểu hay
kiểm thử mọi tổ hợp pipeline chỉ từ việc liệt kê source.

Factory tồn tại trong source khác với compile-time variants trên image. Trên target,
gst-inspect là nguồn xác nhận property/type/enum/pad/plugin version thực tế.
Các property/default macro ở phụ lục được trích từ source, có thể nằm trong #if
khác nhau; không cộng các biến thể thành một cấu hình runtime.

## 2. Cách tra cứu và kiểm tra trên thiết bị (chưa chạy trong đợt này)

```sh
gst-inspect-1.0 qtimlvconverter
gst-inspect-1.0 qtimlqnn
gst-inspect-1.0 qtivtransform
gst-inspect-1.0 qtimlpostprocess
```

Ghi plugin filename/version, enum nick, pad caps/features và mutability. Kiểm tra
GParamSpec qua code trước g_object_set; không dùng integer enum copy từ bản khác.
Property G_PARAM_READWRITE không tự chứng minh reconfigure an toàn lúc PLAYING.
Chốt cấu hình trước state transition; version change phải rerun contract tests.

Build failure khác missing plugin, missing model khác caps negotiation, inference
error khác parser error. Bus ERROR phải được ghi element name + error domain/code,
không chỉ báo pipeline failed.

## 3. Đường AI chuẩn và hai ranh giới adapter

```text
Camera Service -> project frame bridge -> appsrc
    -> qtimlvconverter engine=fcv
    -> capsfilter (model input tensor shape/type)
    -> qtimlqnn backend=<approved HTP library> model=<verified artifact>
    -> appsink (raw tensor result)
    -> portable decoder/tracking/features

Alternative: inference -> qtimlpostprocess/<legacy task plugin>
    -> metadata translator -> neutral observations
```

Không thêm qtiqmmfsrc/encoder/overlay vào graph inference. Camera capture thuộc FW;
AI preview overlay/encode là adapter output riêng do AI APP sở hữu theo
[FW release baseline](../contracts/fw_release_compatibility.md). Bọc plugin thay vì viết lại FastCV/QNN
loader trong đợt đầu; GStreamer không lộ ra core/contracts/features.

QNN plugin có limitations về native dtype/multiple graphs: backend này phải công
bố giới hạn, không pretend toàn bộ capabilities direct SDK. Direct SDK chỉ là
extension sau nếu plugin không đáp ứng một model cụ thể.

## 4. qtimlvconverter — đọc trước khi cấu hình

Input: video/x-raw (có thêm GBM caps khi backend hỗ trợ).
Output: neural-network/tensors, không phải NV12 video. Tensor dimensions/layout
được fixate/negotiation từ downstream model/caps, không đặt width/height bằng
property giả trên converter.

- engine nick `fcv`, không `fastcv`; property tên `engine`, không `backend`.
  Shared video engine mặc định có thể chọn GLES/C2D/OCV theo compile flags.
  Adapter luôn chọn fcv, không fallback sang ocv.
- image-disposition: `top-left` (default), `centre`, `stretch`.
  Centre khác top-left về box/landmark transform; không mặc định pad value 114.
  set_caps khởi tạo background 0; model letterbox khác cần đường preprocess khác.
- subpixel-layout: `regular` hoặc `reverse`; không tự thay NHWC/NCHW.
- mode: image-batch-non-cumulative, image-batch-cumulative,
  roi-batch-non-cumulative, roi-batch-cumulative. ROI modes lấy ROI metadata;
  không ROI có thể trả GAP; cumulative giữ buffers đến batch hoặc GAP.
- mean/sigma là GstValueArray các double. Mặc dù description sigma nói divisor,
  set_caps chuyển sigma trực tiếp thành composition.scales; base normalization
  thực hiện phép nhân. FP32 path còn scale pixel bởi 1/255 trước (value-mean)*sigma.
  Không lấy tên property làm công thức; phải golden so sánh đúng path.
- Không áp normalization hai lần. Không mặc định output INT8 là quantization
  arbitrary của model; implementation có offset/scaling convention riêng.
- Converter output pool trong source min2/max24; đây là giới hạn nội bộ,
  max-buffers ở appsrc không điều khiển pool này.

Nguồn sâu: mlvconverter.c: enum types, set_caps, transform;
gst-plugin-base/gst/video/video-converter-engine.c: gst_data_normalization,
gst_video_frame_normalize_ip, gst_video_converter_default_backend.

## 5. qtimlqnn — cấu hình và negotiation

Property `model` chấp nhận .bin cached context hoặc .so graph model.
`backend` default /usr/lib/libQnnCpu.so: muốn HTP phải set explicit.
`system` là QNN System library; `backend-device-id` và `tensors` theo runtime.
Không dùng đường SNPE .dlc hoặc TFLite .tflite cho qtimlqnn.

Plugin load engine ở state transition (xem change_state); graph model quyết định
caps. Link lúc NULL chưa chứng minh model load, tensor caps hay inference thành công.
Raw graph execution trong wrapper là synchronous graphExecute; GStreamer streaming
threads không đồng nghĩa QNN async API.

Wrapper negotiate float32 output (workaround trong ml-qnn-engine.cc), execute
graph_infos[0]. Backend ban đầu chỉ nhận model single graph + all outputs,
không expose output subset/reorder chưa regression. Model metadata contract vẫn
bắt buộc; factory tồn tại không chứng minh artifact tương thích.

## 6. Postprocess, metadata, cascade và traffic

Legacy qtimlvdetection/qtimlvclassification/qtimlvpose threshold ở thang 10–100%;
ví dụ 40.0 là 40%, không 0.4. Module/constants/labels phụ thuộc model.
qtimlpostprocess dùng settings và bbox-stabilization; không có generic threshold
property. Không chuyển nguyên g_object_set từ legacy sang plugin mới.

ROI meta, tensor meta, source stream id, PTS và geometry transformation phải giữ
qua converter/inference/postprocess. Tensor != detection != track != identity.
qtimetamux attach metadata vào media, qtivcomposer compose pixels, không thay nhau.
qtimlmetaextractor xuất text, qtimlmetaparser cần parser module đúng data format.

Cascade: detect -> associate metadata to source -> ROI converter -> classifier/
pose/embedding. Giới hạn số ROI, freshness và cadence; không chạy mọi attribute
trên mọi người mỗi frame. Mô hình temporal cần window/FPS/gap policy explicit.
qtibatch/qtimldemux hỗ trợ batching, không tự là scheduler cho 13 feature.
Model phải hỗ trợ batch; sample tối đa streams không phải capacity đã đo trên target.

Áp dụng 13 bài: shared person tracking cho intrusion/count/heatmap/crowd;
PPE/luggage/abandoned thêm object/relations; smoking/weapon/action cần model riêng;
blacklist/retrieval cần FR/ReID + gallery/search + entitlement. Traffic mở vehicle/
plate OCR/lane/calibration mà không thay vendor-neutral domain model.

## 7. Buffer lifetime, memory và shutdown

DMA-BUF FD không đồng nghĩa GBM object, cũng không đồng nghĩa Linux fence.
Không gắn memory:GBM chỉ vì có FD. Source NV12 stride/offset/colorimetry/modifier
phải khai đúng; UBWC không được coi linear. DMA sync CPU khác device completion.
Input read-only; overlay chỉ output owned/writable branch.

appsrc push success = accepted, không = processed. Giữ lease trên lifetime memory
owner (bao gồm subbuffers/views), không release ngay sau push hay sau timeout.
GstBuffer finalization chỉ được dùng làm release evidence khi mọi downstream
reader giữ memory đúng lifetime; cần prove với chain đang dùng.
Không dùng weak-ref trên parent buffer nếu child giữ memory nhưng không parent.

Shutdown: stop admission -> drain -> EOS/result completion -> verified quiescence ->
release camera leases. gst_element_set_state(NULL) có thể block; timeout get_state
không tự hủy DMA. Backend stall cần FW recovery contract.
Đợt code đầu chưa mở submit/start: chỉ tạo/configure/link graph ở NULL để xây đúng
boundary trước khi thêm frame lease bridge và streaming lifecycle.

qtisocketsrc/sink có protocol riêng (SOCK_SEQPACKET, SCM_RIGHTS, return-buffer).
Không tự dùng socket path Camera Service hiện tại với plugin này. Source timeout
có nhánh nhân GST_USECOND và nhánh poll dùng trực tiếp; phải kiểm code/image trước
chọn timeout nonzero. Camera control/entitlement không có sẵn từ plugin socket.

## 8. Mẫu cách ghép để tham khảo

Các dòng dưới là **template thiết kế**, chưa chạy; placeholder MODEL phải thay bằng
artifact/golden tương thích. Camera sample chỉ chạy khi FW không sở hữu camera đó.

```text
Standalone video resize:
qtiqmmfsrc camera=0 -> video/x-raw,NV12,3840x2160 -> qtivtransform engine=fcv
    -> video/x-raw,NV12,<output_width>x<output_height> -> sink

App AI:
appsrc -> video/x-raw,NV12 -> qtimlvconverter engine=fcv
    image-disposition=<model_policy> -> qtimlqnn model=<MODEL.bin>
    backend=/usr/lib/libQnnHtp.so system=/usr/lib/libQnnSystem.so -> appsink

Legacy detection:
converter -> inference -> qtimlvdetection module=<model_module>
    labels=<labels> threshold=40.0 -> <caps supported by module> -> sink

Batch:
sources -> qtibatch.sink_%u -> converter -> batch-compatible inference
    -> qtimldemux.src_%u -> per-source postprocess

Preview with metadata:
video tee -> queue -> qtimetamux.sink
AI metadata -> qtimetamux.data_%u -> qtivoverlay -> AI preview encode -> FW ring
```

Production graph tạo elements/properties typed, không gst_parse_launch với chuỗi
từ remote user. Queue/pool có bounds; temporal branch không drop tùy tiện.
Model shape/channel/preprocess exact từ model kit, không copy 640x640 mọi model.

## 9. Catalog toàn bộ factory

Mỗi mục: vai trò/cách ghép, link implementation, pads, properties và examples tìm
thấy trong source. `Không tìm thấy example` không nghĩa plugin không chạy.
Examples được index bằng factory mention (có thể trong comment/conditional branch);
đọc link trước dùng. Property declarations dưới đây là source reference giữ nguyên
type/default expression/flags; không phải gst-inspect output đã chạy.
Không liệt kê properties kế thừa của base GStreamer; tra gst-inspect trên target.

### qtibatch

Gom buffer nhiều stream/window trước ML converter; request sink_%u, đầu ra src. Batch size model phải khớp; moving-window-size không tự biến model batch1 thành batchN.

Implementation: [gst-plugin-batch/batch.c](../../../gst-plugins-qti-oss/gst-plugin-batch/batch.c).

Pads: `sink_%u, SINK, REQUEST`; `src, SRC, ALWAYS`.

Properties tại [batch.c](../../../gst-plugins-qti-oss/gst-plugin-batch/batch.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_uint ("moving-window-size", "Moving window size", "Number of new buffers that will be used for output frames", 1, 16, DEFAULT_PROP_MOVING_WINDOW_SIZE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));
```

</details>

Examples (factory mention):

- [gst-sample-apps/gst-ai-multistream-batch-inference/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-multistream-batch-inference/main.c)

### qtic2adec

AAC encoded -> PCM; đặt parser/caps đúng framing AAC trước decoder. Audio thuộc FW hoặc extension audio AI.

Implementation: [gst-plugin-codec2/c2adec/c2adec.c](../../../gst-plugins-qti-oss/gst-plugin-codec2/c2adec/c2adec.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Không có property riêng được trích tại các file class; xem inherited properties trên target.

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtic2aenc

PCM -> AAC; bitrate bits/s. Không đưa encoder vào inference path.

Implementation: [gst-plugin-codec2/c2aenc/c2aenc.c](../../../gst-plugins-qti-oss/gst-plugin-codec2/c2aenc/c2aenc.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [c2aenc.c](../../../gst-plugins-qti-oss/gst-plugin-codec2/c2aenc/c2aenc.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_uint ("bitrate", "Bitrate", "Bitrate in bits per second", 0, G_MAXUINT, DEFAULT_PROP_BITRATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtic2vdec

Compressed video -> raw; chọn parser/framing theo sink caps. secure cần protected-content path, không là input AI mặc định.

Implementation: [gst-plugin-codec2/c2vdec/c2vdec.c](../../../gst-plugins-qti-oss/gst-plugin-codec2/c2vdec/c2vdec.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [c2vdec.c](../../../gst-plugins-qti-oss/gst-plugin-codec2/c2vdec/c2vdec.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_boolean ("secure", "Secure", "Secure Playback" "If property is enabled it will select the codec2 secure component", FALSE, G_PARAM_READWRITE));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-tflite-posenet-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-posenet-display-example/main.cc)

### qtic2venc

Raw -> H264/H265/HEIC; target-bitrate bits/s, GOP/IDR/QP/ROI theo property. FW sở hữu recording và concurrent load.

Implementation: [gst-plugin-codec2/c2venc/c2venc.c](../../../gst-plugins-qti-oss/gst-plugin-codec2/c2venc/c2venc.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [c2venc.c](../../../gst-plugins-qti-oss/gst-plugin-codec2/c2venc/c2venc.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("rotate", "Rotate", "Rotate video image", GST_TYPE_C2_VIDEO_ROTATION, DEFAULT_PROP_ROTATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_enum ("control-rate", "Rate Control", "Bitrate control method", GST_TYPE_C2_RATE_CONTROL, DEFAULT_PROP_RATE_CONTROL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("target-bitrate", "Target bitrate", "Target bitrate in bits per second (0xffffffff=component default)", 0, G_MAXUINT, DEFAULT_PROP_TARGET_BITRATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int ("idr-interval", "IDR Interval", "Periodicity of IDR/I frames (0x7fffffff=component default). " "When set to -1, only the first frame will be IDR/I frame. " "When set to 0 or 1, all frames will be IDR/I frame.", -1, G_MAXINT, DEFAULT_PROP_IDR_INTERVAL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("intra-refresh-mode", "Intra refresh mode", "Intra refresh mode (0xffffffff=component default)." "Allow IR only for CBR(_CFR/VFR) RC modes", GST_TYPE_C2_INTRA_REFRESH_MODE, DEFAULT_PROP_INTRA_REFRESH_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("intra-refresh-period", "Intra Refresh Period", "The period of intra refresh. Only support random mode.", 0, G_MAXUINT, DEFAULT_PROP_INTRA_REFRESH_PERIOD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("b-frames", "B Frames", "Number of B-frames between neighboring P-frame and " "P-frame/I-frame (0xffffffff=component default). " #if (CODEC2_CONFIG_VERSION_MAJOR == 1) "B-frame will be disabled if temporal layer has non-zero p-layer" " count for AVC or b-layer count less than 2 for HEVC" #endif // CODEC2_CONFIG_VERSION_MAJOR "Allow B-frame only for VBR(_CFR/VFR) RC modes.", 0, G_MAXUINT, DEFAULT_PROP_B_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("quant-i-frames", "I-Frame Quantization", "Quantization parameter for I-frames (0xffffffff=component default)", 0, G_MAXUINT, DEFAULT_PROP_QUANT_I_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("quant-p-frames", "P-Frame Quantization", "Quantization parameter for P-frames (0xffffffff=component default)", 0, G_MAXUINT, DEFAULT_PROP_QUANT_P_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("quant-b-frames", "B-Frame Quantization", "Quantization parameter for B-frames (0xffffffff=component default)", 0, G_MAXUINT, DEFAULT_PROP_QUANT_B_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("min-quant-i-frames", "Min quant I frames", "Minimum quantization parameter allowed for I-frames", 0, G_MAXUINT, DEFAULT_PROP_MIN_QP_I_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("max-quant-i-frames", "Max quant I frames", "Maximum quantization parameter allowed for I-frames", 0, G_MAXUINT, DEFAULT_PROP_MAX_QP_I_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("min-quant-p-frames", "Min quant P frames", "Minimum quantization parameter allowed for P-frames", 0, G_MAXUINT, DEFAULT_PROP_MIN_QP_P_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("max-quant-p-frames", "Max quant P frames", "Maximum quantization parameter allowed for P-frames", 0, G_MAXUINT, DEFAULT_PROP_MAX_QP_P_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("min-quant-b-frames", "Min quant B frames", "Minimum quantization parameter allowed for B-frames", 0, G_MAXUINT, DEFAULT_PROP_MIN_QP_B_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("max-quant-b-frames", "Max quant B frames", "Maximum quantization parameter allowed for B-frames", 0, G_MAXUINT, DEFAULT_PROP_MAX_QP_B_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_boolean ("roi-quant-mode", "ROI Quantization Mode", "Enable/Disable Adjustment of the quantization parameter according " "to ROIs set manually via the 'roi-quant-boxes' property and/or " "arriving as GstVideoRegionOfInterestMeta attached to the buffer", DEFAULT_PROP_ROI_QUANT_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_boxed ("roi-quant-meta-value", "ROI Meta Quantization Value", "Set specific QP value, different then the default value of (-15), " "for a GstVideoRegionOfInterestMeta type (e.g. 'roi-meta-qp," "person=-20,cup=10,dog=-5;'). The QP values must be in the range of " "-31 (best quality) to 30 (worst quality)", GST_TYPE_STRUCTURE, G_PARAM_READWRITE| G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

gst_param_spec_array ("roi-quant-boxes", "ROI Quantization Boxes", "Manually set ROI boxes (e.g. '<<X, Y, W, H, QP>, <X, Y, W, H, QP>>'). " "The QP values must be in the range of -31 (best quality) to " "30 (worst quality)", gst_param_spec_array ("rectangle", "Rectangle", "Rectangle", g_param_spec_int ("value", "Rectangle Value", "One of X, Y, WIDTH, HEIGHT or QP", G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_enum ("slice-mode", "slice mode", "Slice mode (0xffffffff=component default)", GST_TYPE_C2_SLICE_MODE, DEFAULT_PROP_SLICE_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("slice-size", "Slice size", "Slice size, just set when slice mode setting to MB or Bytes", 0, G_MAXUINT, DEFAULT_PROP_SLICE_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_enum ("entropy-mode", "Entropy Mode", "Entropy mode (0xffffffff=component default)", GST_TYPE_C2_ENTROPY_MODE, DEFAULT_PROP_ENTROPY_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_enum ("loop-filter-mode", "Loop Filter mode", "Deblocking filter mode (0xffffffff=component default)", GST_TYPE_C2_LOOP_FILTER_MODE, DEFAULT_PROP_LOOP_FILTER_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("num-ltr-frames", "LTR Frames Count", "Number of Long Term Reference Frames (0xffffffff=component default)", 0, G_MAXUINT, DEFAULT_PROP_NUM_LTR_FRAMES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_int ("priority", "Priority", "The proirity of current video instance among concurrent cases," "(0x7fffffff=component default)", G_MININT32, G_MAXINT32, DEFAULT_PROP_PRIORITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

gst_param_spec_array ("temporal-layer", "Temporal Layer", "Set temporal layer value for layer encoding, include layers (" "p-layers and b-layers) number, b-layers number and bitrate-ratios " "in integer percent (e.g. '<4,0,25,50,75,100>;'). layers number " "couldn't be larger than 6." #if (CODEC2_CONFIG_VERSION_MAJOR == 1) "blayers number is ignored if profile is not HEVC_MAIN" #endif // CODEC2_CONFIG_VERSION_MAJOR "b-layers number couldn't be larger than " "layers number, bitrate-ratios couldn't be larger than 100 and last " "layer's budget is always 100.", g_param_spec_int ("temporal-layer", "Temporal Layer", "One of layers number, b-layers number, ratios", G_MININT, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_enum ("flip", "Flip", "Flip video image", GST_TYPE_C2_VIDEO_FLIP, DEFAULT_PROP_FLIP, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_int ("vbv-delay", "Video Buffer Verifier Delay", "The buffering delay in milliseconds which is used to stabilize " "bitrate, equivalent to target bitrate measured in thousandth unit." "(0x7fffffff=component default, limited below 100 milliseconds, " "i.e 1/10 of the target bitrate)", 0, G_MAXINT, DEFAULT_PROP_VBV_DELAY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-activate-deactivate-streams-runtime/main.c](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-activate-deactivate-streams-runtime/main.c)
- [gst-plugin-examples/gst-add-remove-streams-runtime/main.c](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-add-remove-streams-runtime/main.c)
- [gst-plugin-examples/gst-add-streams-as-bundle-example/main.c](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-add-streams-as-bundle-example/main.c)
- [gst-plugin-examples/gst-buffering-encoding-mode-switch/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-buffering-encoding-mode-switch/main.cc)
- [gst-plugin-examples/gst-camera-burst-intervalcapture-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-camera-burst-intervalcapture-example/main.cc)

### qticamimgreproc

Camera image reprocess với request sink_%u và src; camera-id/request metadata ở pad riêng, không phải property của element.

Implementation: [gst-plugin-camimgreproc/camera-image-reprocess.c](../../../gst-plugins-qti-oss/gst-plugin-camimgreproc/camera-image-reprocess.c).

Pads: `sink_%u: SINK REQUEST`; `src: SRC ALWAYS`.

Properties tại [camera-image-reprocess.c](../../../gst-plugins-qti-oss/gst-plugin-camimgreproc/camera-image-reprocess.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_uint ("queue-size", "Input and output queue size", "Set the size of the input and output queues.", 3, G_MAXUINT, DEFAULT_PROP_QUEUE_SIZE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));
```

</details>

Properties tại [camera-image-reprocess-pad.c](../../../gst-plugins-qti-oss/gst-plugin-camimgreproc/camera-image-reprocess-pad.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_uint ("camera-id", "Camera ID", "Camera ID", 0, G_MAXINT8, DEFAULT_PROP_SINK_CAMERA_ID, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));

g_param_spec_string ("request-meta-path", "Request Metadata Path", "Absolute path of request metadata to read by camera hal.", DEFAULT_PROP_SINK_REQUEST_METADATA_PATH, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_uint ("request-meta-step", "Request Metadata Step", "Step to read request metadata by camera hal.", 0, G_MAXUINT16, DEFAULT_PROP_SINK_REQUEST_METADATA_STEP, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));

g_param_spec_enum ("eis", "EIS", "Electronic Image Stabilization to reduce the effects of camera \ shake", GST_TYPE_CAMERA_IMAGE_REPROC_EIS, DEFAULT_PROP_SINK_EIS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qticamreproc

Offline camera reprocess trên GBM; camera-id/session-metadata và EIS là contract Camera HAL, không generic resize.

Implementation: [gst-plugin-camreproc/camera-reprocess.c](../../../gst-plugins-qti-oss/gst-plugin-camreproc/camera-reprocess.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [camera-reprocess.c](../../../gst-plugins-qti-oss/gst-plugin-camreproc/camera-reprocess.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_uint ("camera-id", "Camera ID", "Camera ID", 0, G_MAXINT8, DEFAULT_PROP_CAMERA_ID, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));

g_param_spec_string ("request-meta-path", "Request Metadata Path", "Absolute path of request metadata to read by camera hal.", DEFAULT_PROP_REQUEST_METADATA_PATH, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_uint ("request-meta-step", "Request Metadata Step", "Step to read request metadata by camera hal.", 0, G_MAXUINT16, DEFAULT_PROP_REQUEST_METADATA_STEP, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));

g_param_spec_enum ("eis", "EIS", "Electronic Image Stabilization to reduce the effects of camera shake", GST_TYPE_CAMERA_REPROCESS_EIS, DEFAULT_PROP_EIS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));

g_param_spec_pointer ("session-metadata", "Session Metadata", "Settings metadata used for creating offline camera session", G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qticvimgpyramid

Sinh pyramid GRAY8 qua request src_%u; num-octaves/num-scales và sharpness cho mỗi octave. Không đồng nghĩa tensor inference.

Implementation: [gst-plugin-cv-imgpyramid/imagepyramid.c](../../../gst-plugins-qti-oss/gst-plugin-cv-imgpyramid/imagepyramid.c).

Pads: `sink, SINK, ALWAYS`; `src_%u, SRC, REQUEST`.

Properties tại [imagepyramid.c](../../../gst-plugins-qti-oss/gst-plugin-cv-imgpyramid/imagepyramid.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_uint ("num-octaves", "Number of octaves", "Number of layers in the pyramid where the resolution is halved", 1, 5, DEFAULT_PROP_N_OCTAVES, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("num-scales", "Number of scales", "Number of intermediate layers in the pyramid between two octaves", 1, 4, DEFAULT_PROP_N_SCALES, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("octave-sharpness", "Adjust sharpness of octaves.", "Array of coefficients, the size of this array is equal to the " "number of octaves (n_octaves). Format is <c1, c2, c3, cn>. The " "value range per octave [0-4], with default 3", g_param_spec_uint ("value", "Coefficient Value", "One of the filter coefficient value", 0, 4, DEFAULT_OCTAVE_SHARPNESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qticvoptclflow

Motion vectors giữa frame hiện tại và trước; stats threshold điều khiển filtering. Temporal input continuity quan trọng.

Implementation: [gst-plugin-cv-optclflow/opticalflow.c](../../../gst-plugins-qti-oss/gst-plugin-cv-optclflow/opticalflow.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [opticalflow.c](../../../gst-plugins-qti-oss/gst-plugin-cv-optclflow/opticalflow.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_boolean ("stats", "Stats", "Enable statistics for additional motion vector info", DEFAULT_PROP_ENABLE_STATS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("stats-variance-thld", "Stats Variance Threshold", "The statistics variance threshold below which motion vectors will " "be ignored", 0, G_MAXUINT16, DEFAULT_PROP_VARIANCE_THRESHOLD, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("stats-sad-thld", "Stats SAD Threshold", "The statistics SAD threshold below which motion vectors will " "be ignored", 0, G_MAXUINT16, DEFAULT_PROP_SAD_THRESHOLD, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtidfs

Stereo -> disparity; cần rectification/calibration config. mode phụ thuộc RVSDK, không chọn integer mode từ tên SoC.

Implementation: [gst-plugin-dfs/dfs.c](../../../gst-plugins-qti-oss/gst-plugin-dfs/dfs.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [dfs.c](../../../gst-plugins-qti-oss/gst-plugin-dfs/dfs.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("mode", "mode", "Select DFS mode. 0-cvp: hardware, 1-cpu: coverage mode. " "2-opencl: speed mode. 3-opencl: balance mode, only for " "equal or after RVSDK202403 version. 4-cpu: accuracy mode," "only for equal or before RVSDK202307 version.", GST_TYPE_DFS_MODE, DEFAULT_PROP_MODE, (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_int ("min-disparity", "min-disparity", "Set min disparity", 0, 240, DEFAULT_PROP_MIN_DISPARITY, (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_int ("num-disparity-level", "num-disparity-level", "Set disparirty level. Distinct disparity levels between" "neighboring pixels. Multiples of 16", 16, 256, DEFAULT_PROP_NUM_DISPARITY_LEVELS, (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_int ("filter-width", "filter-width", "Set filter width. Controls window size for guided filter" "used in DFS implementation. Must be odd number. Max value" "should be < image width", 1, INT_MAX, DEFAULT_PROP_FILTER_WIDTH, (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_int ("filter-height", "filter-height", "Set filter height. Controls window size for guided filter" "used in DFS implementation. Must be odd number. Max value" "should be < image height", 1, INT_MAX, DEFAULT_PROP_FILTER_HEIGHT, (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_boolean ("rectification", "rectification", "Perform rectification on input frames.", DEFAULT_PROP_RECTIFICATION, (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_string("config", "Path to stereo config file", "Path to config file. Eg.: /data/stereo.config", DEFAULT_CONFIG_PATH, (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_enum ("pplevel", "pplevel", "Select postprocessing level", GST_TYPE_DFS_PPLEVEL, DEFAULT_PROP_PPLEVEL, (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtidngpacker

MIPI RAW + optional JPEG thumbnail -> DNG; raw_sink và image_sink khác NV12 video/x-raw.

Implementation: [gst-plugin-dngpacker/dngpacker.c](../../../gst-plugins-qti-oss/gst-plugin-dngpacker/dngpacker.c).

Pads: `raw_sink, SINK, ALWAYS`; `image_sink, SINK, REQUEST`; `dng_src, SRC, ALWAYS`.

Không có property riêng được trích tại các file class; xem inherited properties trên target.

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtidrmdecryptor

CENC protected encoded content -> clear codec bitstream qua PlayReady/Widevine. Không dùng cho camera AI thông thường.

Implementation: [gst-plugin-drmdecryptor/drmdecryptor.cc](../../../gst-plugins-qti-oss/gst-plugin-drmdecryptor/drmdecryptor.cc).

Pads: `GST_STATIC_PAD_TEMPLATE ( sink, SINK, ALWAYS`; `GST_STATIC_PAD_TEMPLATE ( src, SRC, ALWAYS`.

Properties tại [drmdecryptor.cc](../../../gst-plugins-qti-oss/gst-plugin-drmdecryptor/drmdecryptor.cc):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("session-id", "Session ID", "Session id that is generated upon PR DRM plugin open session or WV DRM" " create session", DEFAULT_PROP_SESSION_ID, GParamFlags ( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_pointer ("cdm-instance", "CDM Instance", "Widevine CDM Instance to call CDM decrypt API", GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-drm-player-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-drm-player-example/main.cc)

### qtijpegenc

Raw -> JPEG; quality 0–100, orientation, camera-id. Dùng khi FW evidence cần snapshot, không encode mỗi inference frame.

Implementation: [gst-plugin-jpegenc/jpegenc.c](../../../gst-plugins-qti-oss/gst-plugin-jpegenc/jpegenc.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [jpegenc.c](../../../gst-plugins-qti-oss/gst-plugin-jpegenc/jpegenc.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_int ("quality", "Quality", "Quality of encoding", 0, 100, DEFAULT_PROP_JPEG_QUALITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("orientation", "Orientation", "Orientation of Jpeg encoder", GST_TYPE_JPEG_ENC_ORIENTATION, DEFAULT_PROP_ORIENTATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("camera-id", "Camera ID", "Camera ID", 0, G_MAXINT8, DEFAULT_PROP_CAMERA_ID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-timelapse-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-timelapse-example/main.cc)

### qtimetamux

Raw media tại sink + data_%u metadata -> media có meta. mode/latency/queue-size kiểm soát đồng bộ, không phải merge pixel video.

Implementation: [gst-plugin-metamux/metamux.c](../../../gst-plugins-qti-oss/gst-plugin-metamux/metamux.c).

Pads: `sink, SINK, ALWAYS`; `data_%u, SINK, REQUEST`; `src, SRC, ALWAYS`.

Properties tại [metamux.c](../../../gst-plugins-qti-oss/gst-plugin-metamux/metamux.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("mode", "Mode", "Operational mode", GST_TYPE_METAMUX_MODE, DEFAULT_PROP_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint64 ("latency", "Latency", "Additional latency to allow more time for upstream to produce " "metadata entries for the current position (in nanoseconds).", 0, G_MAXUINT64, DEFAULT_PROP_LATENCY, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("queue-size", "Input and output queue size", "Set the size of the input and output queues.", 3, G_MAXUINT, DEFAULT_PROP_QUEUE_SIZE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));
```

</details>

Examples (factory mention):

- [gst-python-examples/gst-ai-object-detection.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-ai-object-detection.py)
- [gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py)
- [gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py)
- [gst-python-examples/gst-camera-two-stream-encode-file-detection-display.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-two-stream-encode-file-detection-display.py)
- [gst-python-examples/gst-daisychain-detection-pose.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-daisychain-detection-pose.py)

### qtimetatransform

Xử lý/filter metadata qua module + module-params (GstStructure string). Không thay feature entitlement/rule engine.

Implementation: [gst-plugin-metatransform/metatransform.c](../../../gst-plugins-qti-oss/gst-plugin-metatransform/metatransform.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [metatransform.c](../../../gst-plugins-qti-oss/gst-plugin-metatransform/metatransform.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for processing the buffer metas", GST_TYPE_META_TRANSFORM_BACKEND, DEFAULT_PROP_MODULE_BACKEND, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("module-params", "Module Parameters", "Parameters specific to the chosen module for processing/filtering/" "conversion of buffer metas. The format is in GstStructure string.", DEFAULT_PROP_MODULE_PARAMS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtimlaclassification

Audio tensor -> classification; module/labels/threshold phần trăm/results. Ghép với audio converter và inference phù hợp.

Implementation: [gst-plugin-mlaclassification/mlaclassification.c](../../../gst-plugins-qti-oss/gst-plugin-mlaclassification/mlaclassification.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlaclassification.c](../../../gst-plugins-qti-oss/gst-plugin-mlaclassification/mlaclassification.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for processing the tensors", GST_TYPE_ML_MODULES, DEFAULT_PROP_MODULE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("labels", "Labels", "Labels filename", DEFAULT_PROP_LABELS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("results", "Results", "Number of results to display", 0, 10, DEFAULT_PROP_NUM_RESULTS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_double ("threshold", "Threshold", "Confidence threshold in %", 10.0F, 100.0F, DEFAULT_PROP_THRESHOLD, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-sample-apps/gst-ai-audio-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-audio-classification/main.c)

### qtimlaconverter

PCM mono -> audio feature tensors; sample-rate, feature, params (nfft/nhop/nmels...). Không dùng resize video.

Implementation: [gst-plugin-mlaconverter/mlaconverter.c](../../../gst-plugins-qti-oss/gst-plugin-mlaconverter/mlaconverter.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlaconverter.c](../../../gst-plugins-qti-oss/gst-plugin-mlaconverter/mlaconverter.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_int ("sample-rate", "Sample-Rate", "Audio sample rate converter expects", G_MININT, G_MAXINT, DEFAULT_PROP_SAMPLE_RATE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("params", "Preprocessor feature parameters", "Preprocessor configuration" "The format is in GstStructure string. Example params can be passed as" "params=\"params,nfft=512,nhop=5,nmels=80;\"", DEFAULT_PROP_PARAMS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("feature", "Audio Preprocessing feature", "Preprocessing function to run on raw audio samples", GST_TYPE_ML_AUDIO_CONVERSION_FEATURE, DEFAULT_PROP_CONVERSION_FEATURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-sample-apps/gst-ai-audio-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-audio-classification/main.c)

### qtimlaic

AIC model với request input/output pads, devices/activations. Không đồng nhất AIC runtime với QNN HTP; chọn chỉ khi model/runtime tương ứng.

Implementation: [gst-plugin-mlaic/mlaic.c](../../../gst-plugins-qti-oss/gst-plugin-mlaic/mlaic.c).

Pads: `src_%u, SRC, REQUEST`; `sink_%u, SINK, REQUEST`.

Properties tại [mlaic.c](../../../gst-plugins-qti-oss/gst-plugin-mlaic/mlaic.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("model", "Model", "Model filename", DEFAULT_PROP_MODEL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("devices", "Devices", "List of AIC device IDs. ('<ID, ID, ID, ...>')", g_param_spec_int ("id", "Device ID", "AIC device ID.", 0, G_MAXINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("activations", "Activations", "Number of activations (AIC programs and queues).", 1, 10, DEFAULT_PROP_N_ACTIVATIONS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtimldemux

Tách batch ML theo request src_%u; giữ stream-id/timestamp metadata. Static metadata description trong source bị giống batch, đọc pad/implementation.

Implementation: [gst-plugin-mldemux/mldemux.c](../../../gst-plugins-qti-oss/gst-plugin-mldemux/mldemux.c).

Pads: `sink, SINK, ALWAYS`; `src_%u, SRC, REQUEST`.

Không có property riêng được trích tại các file class; xem inherited properties trên target.

Examples (factory mention):

- [gst-sample-apps/gst-ai-multistream-batch-inference/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-multistream-batch-inference/main.c)

### qtimlmetaextractor

Extract ML meta từ video sang text stream để truyền/đọc metadata; không thực hiện inference.

Implementation: [gst-plugin-mlmetaextractor/mlmetaextractor.c](../../../gst-plugins-qti-oss/gst-plugin-mlmetaextractor/mlmetaextractor.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Không có property riêng được trích tại các file class; xem inherited properties trên target.

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtimlmetaparser

Parse metadata qua module từ text/video tùy caps. Cần parser module phù hợp format, không mọi JSON đều tương thích.

Implementation: [gst-plugin-mlmetaparser/mlmetaparser.c](../../../gst-plugins-qti-oss/gst-plugin-mlmetaparser/mlmetaparser.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlmetaparser.c](../../../gst-plugins-qti-oss/gst-plugin-mlmetaparser/mlmetaparser.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for parsing metadata", GST_TYPE_PARSER_MODULES, DEFAULT_PROP_MODULE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtimlpostprocess

Postprocess tổng quát module-driven; module/labels/results/settings/bbox-stabilization. Không có generic threshold property như qtimlvdetection.

Implementation: [gst-plugin-mlpostprocess/mlpostprocess.cc](../../../gst-plugins-qti-oss/gst-plugin-mlpostprocess/mlpostprocess.cc).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlpostprocess.cc](../../../gst-plugins-qti-oss/gst-plugin-mlpostprocess/mlpostprocess.cc):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for processing the tensors", GST_TYPE_ML_MODULES, DEFAULT_PROP_MODULE, static_cast <GParamFlags> ( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_string ("labels", "Labels", "Labels filename", DEFAULT_PROP_LABELS, static_cast <GParamFlags> ( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("results", "Results", "Number of results to display", 0, 50, DEFAULT_PROP_NUM_RESULTS, static_cast <GParamFlags> ( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_string ("settings", "Settings", "Settings used by the chosen module for post-processing. " "Applicable only for some modules.", DEFAULT_PROP_SETTINGS, static_cast <GParamFlags> ( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_boolean ("bbox-stabilization", "BBox Stabilization enable", "Enable stabilization of bboxes", DEFAULT_PROP_BBOX_STABILIZATION, static_cast <GParamFlags> ( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
```

</details>

Examples (factory mention):

- [gst-python-examples/gst-ai-object-detection.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-ai-object-detection.py)
- [gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py)
- [gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py)
- [gst-python-examples/gst-camera-two-stream-encode-file-detection-display.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-two-stream-encode-file-detection-display.py)
- [gst-python-examples/gst-daisychain-detection-pose.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-daisychain-detection-pose.py)

### qtimlqnn

Tensor -> QNN -> tensor. model .bin/.so; backend HTP explicit, system lib. Output dtype là hành vi wrapper, xem phần AI bên dưới.

Implementation: [gst-plugin-mlqnn/mlqnn.c](../../../gst-plugins-qti-oss/gst-plugin-mlqnn/mlqnn.c).

Pads: `src, SRC, ALWAYS`; `sink, SINK, ALWAYS`.

Properties tại [mlqnn.c](../../../gst-plugins-qti-oss/gst-plugin-mlqnn/mlqnn.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("backend", "Backend", "Backend lib path", PROP_QNN_BACKEND_DEFAULT, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("model", "Model", "Model/CachedBin file path. " "Expecting a .so model file or a .bin cache bin file.", PROP_QNN_MODEL_DEFAULT, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("system", "System", "System lib path", PROP_QNN_SYSTEM_DEFAULT, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("backend-device-id", "Backend Device ID", "Backend Device ID", 0, (gst_ml_qnn_get_num_cdsp_backends() - 1), PROP_BACKEND_DEVICE_ID_DEFAULT, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("tensors", "Tensors", "List of output tensors.", g_param_spec_string ("name", "Tensor Name", "Name of the output tensor.", NULL, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-sample-apps/gst-ai-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-classification/main.c)
- [gst-sample-apps/gst-ai-face-detection/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-face-detection/main.c)
- [gst-sample-apps/gst-ai-face-recognition/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-face-recognition/main.c)
- [gst-sample-apps/gst-ai-monodepth/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-monodepth/main.c)
- [gst-sample-apps/gst-ai-multistream-batch-inference/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-multistream-batch-inference/main.c)

### qtimlsnpe

DLC + SNPE delegate/performance-profile; layers hoặc tensors chọn output. Không dùng QNN .bin ở model property.

Implementation: [gst-plugin-mlsnpe/mlsnpe.c](../../../gst-plugins-qti-oss/gst-plugin-mlsnpe/mlsnpe.c).

Pads: `src, SRC, ALWAYS`; `sink, SINK, ALWAYS`.

Properties tại [mlsnpe.c](../../../gst-plugins-qti-oss/gst-plugin-mlsnpe/mlsnpe.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("model", "Model", "Model filename", DEFAULT_PROP_MODEL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("delegate", "Delegate", "Delegate the graph execution to another executor", GST_TYPE_ML_SNPE_DELEGATE, DEFAULT_PROP_DELEGATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("performance-profile", "Performance Profile", "Request a performance profile.", GST_TYPE_ML_SNPE_PERF_PROFILE, DEFAULT_PROP_PERF_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("profiling-level", "Profiling Level", "Set the profiling level.", GST_TYPE_ML_SNPE_PROFILING_LEVEL, DEFAULT_PROP_PROFILING_LEVEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("priority", "Execution Priority", "Sets a preference for execution priority. " "This allows the caller to give coarse hint to SNPE runtime " "about the priority of the network.", GST_TYPE_ML_SNPE_EXEC_PRIORITY, DEFAULT_PROP_EXEC_PRIORITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("layers", "Layers", "List of output layers. Should be set if model has more than one output", g_param_spec_string ("name", "Layer Name", "Name of the output layer.", NULL, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("tensors", "Tensors", "List of output tensors. Alternative to output layer list. " "The outputs will be generated in the order defined in this list.", g_param_spec_string ("name", "Tensor Name", "Name of the output tensor.", NULL, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-snpe-yolo-ssd-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-snpe-yolo-ssd-display-example/main.cc)
- [gst-plugin-examples/gst-snpe-yolo-ssd-encode-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-snpe-yolo-ssd-encode-example/main.cc)
- [gst-python-examples/gst-ai-object-detection.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-ai-object-detection.py)
- [gst-sample-apps/gst-ai-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-classification/main.c)
- [gst-sample-apps/gst-ai-metadata-parser-example/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-metadata-parser-example/main.c)

### qtimltflite

TFLite + delegate CPU/GPU/external; external-delegate-path/options khi delegate=external. Properties canonical dùng dấu gạch ngang.

Implementation: [gst-plugin-mltflite/mltflite.c](../../../gst-plugins-qti-oss/gst-plugin-mltflite/mltflite.c).

Pads: `src, SRC, ALWAYS`; `sink, SINK, ALWAYS`.

Properties tại [mltflite.c](../../../gst-plugins-qti-oss/gst-plugin-mltflite/mltflite.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("model", "Model", "Model filename", DEFAULT_PROP_MODEL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("delegate", "Delegate", "Delegate part or all of graph execution to another executor", GST_TYPE_ML_TFLITE_DELEGATE, DEFAULT_PROP_DELEGATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("threads", "Threads", "Number of threads", 1, 4, DEFAULT_PROP_THREADS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("priority", "Priority", "Set inference priority explicitly for gpu delegate precision only", GST_TYPE_ML_TFLITE_PRIORITY, DEFAULT_PROP_PRIORITY, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("external-delegate-path", "External Delegate Path", "External delegate's absolute path. " "This takes effect when the 'delegate' property is 'external'.", DEFAULT_PROP_EXT_DELEGATE_PATH, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boxed ("external-delegate-options", "External Delegate Options", "External delegate's options, " "that includes backend type and backend library path. " "This takes effect when the 'delegate' property is 'external'.", GST_TYPE_STRUCTURE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-tflite-posenet-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-posenet-display-example/main.cc)
- [gst-plugin-examples/gst-tflite-yolo-ssd-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-yolo-ssd-display-example/main.cc)
- [gst-python-examples/gst-ai-object-detection.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-ai-object-detection.py)
- [gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py)
- [gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py)

### qtimlvclassification

Tensor -> image classification; extra-operation/module/constants theo model. threshold là 10–100%, không 0–1.

Implementation: [gst-plugin-mlvclassification/mlvclassification.c](../../../gst-plugins-qti-oss/gst-plugin-mlvclassification/mlvclassification.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlvclassification.c](../../../gst-plugins-qti-oss/gst-plugin-mlvclassification/mlvclassification.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for processing the tensors", GST_TYPE_ML_MODULES, DEFAULT_PROP_MODULE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("labels", "Labels", "Labels filename", DEFAULT_PROP_LABELS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("results", "Results", "Number of results to display", 0, 10, DEFAULT_PROP_NUM_RESULTS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_double ("threshold", "Threshold", "Confidence threshold in %", 10.0F, 100.0F, DEFAULT_PROP_THRESHOLD, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("constants", "Constants", "Constants, offsets and coefficients used by the chosen module for " "post-processing of incoming tensors in GstStructure string format. " "Applicable only for some modules.", DEFAULT_PROP_CONSTANTS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("extra-operation", "Extra Operation", "Extra operation to perform on the inference data", GST_TYPE_VIDEO_CLASSIFICATION_OPERATION, GST_VIDEO_CLASSIFICATION_OPERATION_NONE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-sample-apps/gst-ai-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-classification/main.c)
- [gst-sample-apps/gst-ai-daisychain-detection-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-daisychain-detection-classification/main.c)
- [gst-sample-apps/gst-ai-face-recognition/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-face-recognition/main.c)

### qtimlvconverter

Raw video -> neural-network/tensors; chọn engine, disposition, mode, subpixel-layout và mean/sigma. Không phải raw-video scaler đầu ra NV12.

Implementation: [gst-plugin-mlvconverter/mlvconverter.c](../../../gst-plugins-qti-oss/gst-plugin-mlvconverter/mlvconverter.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlvconverter.c](../../../gst-plugins-qti-oss/gst-plugin-mlvconverter/mlvconverter.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("mode", "Mode", "Conversion mode", GST_TYPE_ML_CONVERSION_MODE, DEFAULT_PROP_CONVERSION_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("engine", "Engine", "Engine backend used for the conversion operations", GST_TYPE_VCE_BACKEND, DEFAULT_PROP_ENGINE_BACKEND, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("image-disposition", "Image Disposition", "Aspect Ratio and placement of the image inside the output tensor", GST_TYPE_ML_VIDEO_DISPOSITION, DEFAULT_PROP_IMAGE_DISPOSITION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("subpixel-layout", "Subpixel Layout", "Arrangement of the image pixels insize the output tensor", GST_TYPE_ML_VIDEO_PIXEL_LAYOUT, DEFAULT_PROP_SUBPIXEL_LAYOUT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("mean", "Mean Subtraction", "Channels mean subtraction values for FLOAT tensors " "('<R, G, B>', '<R, G, B, A>', '<G>')", g_param_spec_double ("value", "Mean Value", "One of B, G or R value.", 0.0, 255.0, DEFAULT_PROP_MEAN, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("sigma", "Sigma Values", "Channel divisor values for FLOAT tensors " "('<R, G, B>', '<R, G, B, A>', '<G>')", g_param_spec_double ("value", "Sigma Value", "One of B, G or R value.", 0.0, 255.0, DEFAULT_PROP_SIGMA, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-snpe-yolo-ssd-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-snpe-yolo-ssd-display-example/main.cc)
- [gst-plugin-examples/gst-snpe-yolo-ssd-encode-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-snpe-yolo-ssd-encode-example/main.cc)
- [gst-plugin-examples/gst-tflite-posenet-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-posenet-display-example/main.cc)
- [gst-plugin-examples/gst-tflite-yolo-ssd-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-yolo-ssd-display-example/main.cc)
- [gst-python-examples/gst-ai-object-detection.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-ai-object-detection.py)

### qtimlvdetection

Tensor -> detection representation; module/labels/results/threshold/constants/stabilization. Output caps/geometry phải kiểm theo module.

Implementation: [gst-plugin-mlvdetection/mlvdetection.c](../../../gst-plugins-qti-oss/gst-plugin-mlvdetection/mlvdetection.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlvdetection.c](../../../gst-plugins-qti-oss/gst-plugin-mlvdetection/mlvdetection.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for processing the tensors", GST_TYPE_ML_MODULES, DEFAULT_PROP_MODULE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("labels", "Labels", "Labels filename", DEFAULT_PROP_LABELS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("results", "Results", "Number of results to display", 0, 50, DEFAULT_PROP_NUM_RESULTS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_double ("threshold", "Threshold", "Confidence threshold in %", 10.0F, 100.0F, DEFAULT_PROP_THRESHOLD, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("constants", "Constants", "Constants, offsets and coefficients used by the chosen module for " "post-processing of incoming tensors in GstStructure string format. " "Applicable only for some modules.", DEFAULT_PROP_CONSTANTS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("stabilization", "Stabilization enable", "Enable stabilization of bboxes", DEFAULT_PROP_STABILIZATION, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-snpe-yolo-ssd-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-snpe-yolo-ssd-display-example/main.cc)
- [gst-plugin-examples/gst-snpe-yolo-ssd-encode-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-snpe-yolo-ssd-encode-example/main.cc)
- [gst-plugin-examples/gst-tflite-yolo-ssd-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-yolo-ssd-display-example/main.cc)
- [gst-python-examples/gst-daisychain-detection-pose.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-daisychain-detection-pose.py)
- [gst-sample-apps/gst-ai-daisychain-detection-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-daisychain-detection-classification/main.c)

### qtimlvpose

Tensor -> pose/keypoints; module/labels/constants/threshold; cần đúng ontology landmark và mapping tọa độ.

Implementation: [gst-plugin-mlvpose/mlvpose.c](../../../gst-plugins-qti-oss/gst-plugin-mlvpose/mlvpose.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlvpose.c](../../../gst-plugins-qti-oss/gst-plugin-mlvpose/mlvpose.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for processing the tensors", GST_TYPE_ML_MODULES, DEFAULT_PROP_MODULE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("labels", "Labels", "Labels filename", DEFAULT_PROP_LABELS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("results", "Results", "Number of results to display", 0, 10, DEFAULT_PROP_NUM_RESULTS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_double ("threshold", "Threshold", "Confidence threshold in %", 10.0, 100.0, DEFAULT_PROP_THRESHOLD, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("constants", "Constants", "Constants, offsets and coefficients used by the chosen module for " "post-processing of incoming tensors in GstStructure string format. " "Applicable only for some modules.", DEFAULT_PROP_CONSTANTS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-tflite-posenet-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-posenet-display-example/main.cc)
- [gst-python-examples/gst-daisychain-detection-pose.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-daisychain-detection-pose.py)
- [gst-sample-apps/gst-ai-daisychain-detection-pose/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-daisychain-detection-pose/main.c)
- [gst-sample-apps/gst-ai-face-recognition/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-face-recognition/main.c)
- [gst-sample-apps/gst-ai-parallel-inference/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-parallel-inference/main.c)

### qtimlvsegmentation

Tensor -> segmentation output; module/labels/constants; output representation theo caps, không coi là detection.

Implementation: [gst-plugin-mlvsegmentation/mlvsegmentation.c](../../../gst-plugins-qti-oss/gst-plugin-mlvsegmentation/mlvsegmentation.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlvsegmentation.c](../../../gst-plugins-qti-oss/gst-plugin-mlvsegmentation/mlvsegmentation.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for processing the tensors", GST_TYPE_ML_MODULES, DEFAULT_PROP_MODULE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("labels", "Labels", "Labels filename", DEFAULT_PROP_LABELS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("constants", "Constants", "Constants, offsets and coefficients used by the chosen module for " "post-processing of incoming tensors in GstStructure string format. " "Applicable only for some modules.", DEFAULT_PROP_CONSTANTS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-sample-apps/gst-ai-monodepth/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-monodepth/main.c)
- [gst-sample-apps/gst-ai-multistream-batch-inference/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-multistream-batch-inference/main.c)
- [gst-sample-apps/gst-ai-parallel-inference/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-parallel-inference/main.c)
- [gst-sample-apps/gst-ai-segmentation/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-segmentation/main.c)

### qtimlvsuperresolution

Tensor -> super-resolution output theo module/constants; model input/output khác detector.

Implementation: [gst-plugin-mlvsuperresolution/mlvsuperresolution.c](../../../gst-plugins-qti-oss/gst-plugin-mlvsuperresolution/mlvsuperresolution.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [mlvsuperresolution.c](../../../gst-plugins-qti-oss/gst-plugin-mlvsuperresolution/mlvsuperresolution.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("module", "Module", "Module name that is going to be used for processing the tensors", GST_TYPE_ML_MODULES, DEFAULT_PROP_MODULE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("constants", "Constants", "Constants, offsets and coefficients used by the chosen module for " "post-processing of incoming tensors in GstStructure string format. " "Applicable only for some modules.", DEFAULT_PROP_CONSTANTS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-sample-apps/gst-ai-superresolution/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-superresolution/main.c)

### qtimsgpub

Publish payload qua protocol module/host/port/topic/config; không tự có durability/idempotency/entitlement của hệ thống.

Implementation: [gst-plugin-msgbroker/msgpub/msgpub.c](../../../gst-plugins-qti-oss/gst-plugin-msgbroker/msgpub/msgpub.c).

Pads: `sink, SINK, ALWAYS`.

Properties tại [msgpub.c](../../../gst-plugins-qti-oss/gst-plugin-msgbroker/msgpub/msgpub.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("protocol", "protocol", "Message protocol (mqtt .etc).", DEFAULT_MSG_PUB_PROTOCOL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

g_param_spec_string ("host", "host", "The IP address to send packets to.", DEFAULT_MSG_PUB_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

g_param_spec_int ("port", "port", "The port to send packets to.", 0, G_MAXINT, DEFAULT_MAG_PUB_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

g_param_spec_string ("topic", "topic", "The topic to publish to.", DEFAULT_MSG_PUB_TOPIC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("message", "message from commandline", "The message from commandline to publish.", DEFAULT_MSG_PUB_MESSAGE_CMD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("config", "config file", "The absolute path of protocol config file.", DEFAULT_MSG_PUB_CONFIG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

g_param_spec_boolean ("json", "json format", "Send message in json format", DEFAULT_MSG_PUB_JSON, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtimsgsub

Subscribe topic qua protocol module; cấu hình transport/auth bằng config phù hợp, không tin payload điều khiển chưa xác thực.

Implementation: [gst-plugin-msgbroker/msgsub/msgsub.c](../../../gst-plugins-qti-oss/gst-plugin-msgbroker/msgsub/msgsub.c).

Pads: `src, SRC, ALWAYS`.

Properties tại [msgsub.c](../../../gst-plugins-qti-oss/gst-plugin-msgbroker/msgsub/msgsub.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("protocol", "protocol", "Message protocol (mqtt .etc).", DEFAULT_MSG_SUB_PROTOCOL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

g_param_spec_string ("host", "host", "The IP address to send packets to.", DEFAULT_MSG_SUB_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

g_param_spec_int ("port", "port", "The port to send packets to.", 0, G_MAXINT, DEFAULT_MAG_SUB_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

g_param_spec_string ("topic", "topic", "The topic to sublish to.", DEFAULT_MSG_SUB_TOPIC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("config", "config file", "The absolute path of protocol config file.", DEFAULT_MSG_SUB_CONFIG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtiobjtracker

Tracking trên metadata, algo + parameters. Không tự có detector; thuật toán/model dependency không nhất thiết chạy hardware.

Implementation: [gst-plugin-objtracker/objtracker.c](../../../gst-plugins-qti-oss/gst-plugin-objtracker/objtracker.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [objtracker.c](../../../gst-plugins-qti-oss/gst-plugin-objtracker/objtracker.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("algo", "Algorithm", "Algorithm name that used for the object tracker", GST_TYPE_OBJTRACKER_BACKEND, DEFAULT_PROP_ALGO_BACKEND, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("parameters", "Parameters", "Parameters, parameters used by chosen object tracker algorithm " "in GstStructure string format. " "Applicable only for some algorithms.", DEFAULT_PROP_PARAMETERS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtioverlay

Render text/image/bbox/graph trên video; property overlay-* và engine riêng. Khác qtivoverlay, không tráo cấu hình.

Implementation: [gst-plugin-overlay/gstoverlay.cc](../../../gst-plugins-qti-oss/gst-plugin-overlay/gstoverlay.cc).

Pads: `src, SRC, ALWAYS`; `sink, SINK, ALWAYS`.

Properties tại [gstoverlay.cc](../../../gst-plugins-qti-oss/gst-plugin-overlay/gstoverlay.cc):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("overlay-text", "Text Overlay", "Renders text on top of video stream.", DEFAULT_PROP_OVERLAY_TEXT, static_cast <GParamFlags> ( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

g_param_spec_string ("overlay-date", "Date Overlay", "Renders date and time on top of video stream.", DEFAULT_PROP_OVERLAY_DATE, static_cast <GParamFlags> ( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

g_param_spec_string ("overlay-simg", "Static Image Overlay", "Renders static image on top of video stream.", DEFAULT_PROP_OVERLAY_DATE, static_cast <GParamFlags> ( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

g_param_spec_string ("overlay-bbox", "Bounding Box Overlay", "Renders bounding box and label on top of video stream.", DEFAULT_PROP_OVERLAY_TEXT, static_cast <GParamFlags> ( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

g_param_spec_string ("overlay-mask", "Privacy Mask Overlay", "Renders privacy mask on top of video stream.", DEFAULT_PROP_OVERLAY_TEXT, static_cast <GParamFlags> ( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

g_param_spec_uint ("bbox-color", "BBox color", "Bounding box overlay color", 0, G_MAXUINT, DEFAULT_PROP_OVERLAY_BBOX_COLOR, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("date-color", "Date color", "Date overlay color", 0, G_MAXUINT, DEFAULT_PROP_OVERLAY_DATE_COLOR, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("text-color", "Text color", "Text overlay color", 0, G_MAXUINT, DEFAULT_PROP_OVERLAY_TEXT_COLOR, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("pose-color", "Pose color", "Pose overlay color", 0, G_MAXUINT, DEFAULT_PROP_OVERLAY_POSE_COLOR, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("arrows-color", "Arrows color", "Arrows overlay color", 0, G_MAXUINT, DEFAULT_PROP_OVERLAY_ARROWS_COLOR, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("bbox-font-size", "BBox font size", "Bounding box overlay font size", 1, G_MAXUINT, DEFAULT_PROP_OVERLAY_BBOX_FONT_SIZE, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("date-font-size", "Date font size", "Date overlay font size", 1, G_MAXUINT, DEFAULT_PROP_OVERLAY_DATE_FONT_SIZE, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("text-font-size", "Text font size", "Text overlay font size", 1, G_MAXUINT, DEFAULT_PROP_OVERLAY_TEXT_FONT_SIZE, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

gst_param_spec_array ("dest-rect-ml-text", "Destination Rectangle for ML Detection overlay", "Destination rectangle params for ML Detection overlay. " "The Start-X, Start-Y , Width, Height of the destination rectangle " "format is <X, Y, WIDTH, HEIGHT>", g_param_spec_int ("coord", "Coordinate", "One of X, Y, Width, Height value.", 0, G_MAXINT, 0, static_cast <GParamFlags> (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)), static_cast <GParamFlags> (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("arrows-ft-mv", "MV filter", "Arrows mv filter", 0, G_MAXUINT, 0, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("arrows-ft-sad", "SAD filter", "Arrows sad filter", 0, G_MAXUINT, 0, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("arrows-ft-var", "VAR filter", "Arrows var filter", 0, G_MAXUINT, 0, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_uint ("bbox-stroke-width", "Bounding box stroke width", "Set the width of the bounding box rectangle", 1, G_MAXUINT, DEFAULT_PROP_OVERLAY_BBOX_STROKE_WIDTH, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

g_param_spec_enum ("engine", "Engine type", "Set the engine used for blit", GST_TYPE_OVERLAY_ENGINE, DEFAULT_PROP_OVERLAY_ENGINE, static_cast<GParamFlags>( G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtiqmmfsrc

Camera source, request video_%u/image_%u; camera ID, 3A, sensor/HDR/EIS và pad settings. Chỉ FW sở hữu source production.

Implementation: [gst-plugin-qmmfsrc/qmmf_source.c](../../../gst-plugins-qti-oss/gst-plugin-qmmfsrc/qmmf_source.c).

Pads: `video_%u: SRC REQUEST`; `image_%u: SRC REQUEST`.

Properties tại [qmmf_source_video_pad.c](../../../gst-plugins-qti-oss/gst-plugin-qmmfsrc/qmmf_source_video_pad.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_int ("source-index", "Source index", "Index of the source video pad to which this pad will be linked", -1, G_MAXINT, DEFAULT_PROP_SOURCE_INDEX, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_boolean ("reprocess-enable", "Reprocess pad", "Indicates realtime video pad which will be used as " "input for reprocess", DEFAULT_PROP_REPROCESS_PAD, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_double ("framerate", "Framerate", "Target framerate in frames per second for displaying", 0.0, 30.0, DEFAULT_PROP_FRAMERATE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

gst_param_spec_array ("crop", "Crop rectangle", "Crop rectangle ('<X, Y, WIDTH, HEIGHT>'). Applicable only for " "JPEG and YUY2 formats", g_param_spec_int ("value", "Crop Value", "One of X, Y, WIDTH or HEIGHT value.", 0, G_MAXINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_uint ("extra-buffers", "Extra Buffers", "Number of additional buffers that will be allocated.", 0, G_MAXUINT, DEFAULT_PROP_EXTRA_BUFFERS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_enum ("rotate", "Rotate", "Set Orientation Angle for Video Stream", GST_TYPE_QMMFSRC_ROTATE, DEFAULT_PROP_ROTATE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("super-buffer-mode", "Super Buffer Mode", "Enable Super Buffer Mode for HFR mode, in this mode, each video pad" "works on super buffer mode, allowing one buffer to hold multiple frames" "of data. The number of frames in a single super buffer is determined by" "the ratio of the framerate to the superframerate. The default superframerate" "is 60fps", DEFAULT_PROP_SUPER_BUFFER, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_enum ("type", "Type", "The type of the stream.", GST_TYPE_QMMFSRC_VIDEO_TYPE, DEFAULT_PROP_VIDEO_TYPE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("type", "Type", "The type of the stream.", GST_TYPE_QMMFSRC_VIDEO_TYPE, DEFAULT_PROP_VIDEO_TYPE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("logical-stream-type", "Stream type for logical camera", "Type of the stream for logical camera.", GST_TYPE_QMMFSRC_PAD_LOGICAL_STREAM_TYPE, DEFAULT_PROP_LOGICAL_STREAM_TYPE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));
```

</details>

Properties tại [qmmf_source_image_pad.c](../../../gst-plugins-qti-oss/gst-plugin-qmmfsrc/qmmf_source_image_pad.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("rotate", "Rotate", "Set Orientation Angle for Image Stream", GST_TYPE_QMMFSRC_ROTATE, DEFAULT_PROP_ROTATE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("logical-stream-type", "Stream type for logical camera", "Type of stream to select specific physical camera or layout to " "stitch images.", GST_TYPE_QMMFSRC_PAD_LOGICAL_STREAM_TYPE, DEFAULT_PROP_LOGICAL_STREAM_TYPE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PAUSED));
```

</details>

Properties tại [qmmf_source.c](../../../gst-plugins-qti-oss/gst-plugin-qmmfsrc/qmmf_source.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_uint ("camera", "Camera ID", "Camera device ID to be used by video/image pads", 0, 32, DEFAULT_PROP_CAMERA_ID, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("slave", "Slave mode", "Set camera as slave device", DEFAULT_PROP_CAMERA_SLAVE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("ldc", "LDC", "Lens Distortion Correction", DEFAULT_PROP_CAMERA_LDC_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("lcac", "LCAC", "Lateral Chromatic Aberration Correction", DEFAULT_PROP_CAMERA_LCAC_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("eis", "EIS", "Electronic Image Stabilization mode to reduce the effects of camera shake", DEFAULT_PROP_CAMERA_EIS_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_enum ("eis", "EIS", "Electronic Image Stabilization mode to reduce the effects of camera shake", GST_TYPE_QMMFSRC_EIS_MODE, DEFAULT_PROP_CAMERA_EIS_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("shdr", "SHDR", "Super High Dynamic Range Imaging", DEFAULT_PROP_CAMERA_SHDR_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("vhdr", "VHDR", "Video High Dynamic Range Imaging Modes", GST_TYPE_QMMFSRC_VHDR_MODE, DEFAULT_PROP_CAMERA_VHDR_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_boolean ("adrc", "ADRC", "Automatic Dynamic Range Compression", DEFAULT_PROP_CAMERA_ADRC, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("control-mode", "Control Mode", "Overall mode of 3A (auto-exposure, auto-white-balance, auto-focus) " "control routines. This is a top-level 3A control switch. When set " "to OFF, all 3A control by the camera device is disabled.", GST_TYPE_QMMFSRC_CONTROL_MODE, DEFAULT_PROP_CAMERA_CONTROL_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("effect", "Effect", "Effect applied on the camera frames", GST_TYPE_QMMFSRC_EFFECT_MODE, DEFAULT_PROP_CAMERA_EFFECT_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("scene", "Scene", "Camera optimizations depending on the scene", GST_TYPE_QMMFSRC_SCENE_MODE, DEFAULT_PROP_CAMERA_SCENE_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("antibanding", "Antibanding", "Camera antibanding routine for the current illumination condition", GST_TYPE_QMMFSRC_ANTIBANDING, DEFAULT_PROP_CAMERA_ANTIBANDING, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int ("sharpness", "Sharpness", "Image Sharpness Strength", 0, 6, DEFAULT_PROP_CAMERA_SHARPNESS, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int ("contrast", "Contrast", "Image Contrast Strength", 1, 10, DEFAULT_PROP_CAMERA_CONTRAST, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int ("saturation", "Saturation", "Image Saturation Strength", 0, 10, DEFAULT_PROP_CAMERA_SATURATION, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("iso-mode", "ISO Mode", "ISO exposure mode", GST_TYPE_QMMFSRC_ISO_MODE, DEFAULT_PROP_CAMERA_ISO_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int ("manual-iso-value", "Manual ISO Value", "Manual exposure ISO value. Used when the ISO mode is set to 'manual'", 100, 3200, DEFAULT_PROP_CAMERA_ISO_VALUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("exposure-mode", "Exposure Mode", "The desired mode for the camera's exposure routine.", GST_TYPE_QMMFSRC_EXPOSURE_MODE, DEFAULT_PROP_CAMERA_EXPOSURE_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_boolean ("exposure-lock", "Exposure Lock", "Locks current camera exposure routine values from changing.", DEFAULT_PROP_CAMERA_EXPOSURE_LOCK, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("exposure-metering", "Exposure Metering", "The desired mode for the camera's exposure metering routine.", GST_TYPE_QMMFSRC_EXPOSURE_METERING, DEFAULT_PROP_CAMERA_EXPOSURE_METERING, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int ("exposure-compensation", "Exposure Compensation", "Adjust (Compensate) camera images target brightness. Adjustment is " "measured as a count of steps.", -12, 12, DEFAULT_PROP_CAMERA_EXPOSURE_COMPENSATION, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int64 ("manual-exposure-time", "Manual Exposure Time", "Manual exposure time in nanoseconds. Used when the Exposure mode" " is set to 'off'.", 0, G_MAXINT64, DEFAULT_PROP_CAMERA_EXPOSURE_TIME, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("custom-exposure-table", "Custom Exposure Table", "A GstStructure describing custom exposure table", DEFAULT_PROP_CAMERA_EXPOSURE_TABLE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("white-balance-mode", "White Balance Mode", "The desired mode for the camera's white balance routine.", GST_TYPE_QMMFSRC_WHITE_BALANCE_MODE, DEFAULT_PROP_CAMERA_WHITE_BALANCE_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_boolean ("white-balance-lock", "White Balance Lock", "Locks current White Balance values from changing. Affects only " "non-manual white balance modes.", DEFAULT_PROP_CAMERA_WHITE_BALANCE_LOCK, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("manual-wb-settings", "Manual WB Settings", "Manual White Balance settings such as color correction temperature " "and R/G/B gains. Used in manual white balance modes.", DEFAULT_PROP_CAMERA_MANUAL_WB_SETTINGS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("focus-mode", "Focus Mode", "Whether auto-focus is currently enabled, and in what mode it is.", GST_TYPE_QMMFSRC_FOCUS_MODE, DEFAULT_PROP_CAMERA_FOCUS_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("noise-reduction", "Noise Reduction", "Noise reduction filter mode", GST_TYPE_QMMFSRC_NOISE_REDUCTION, DEFAULT_PROP_CAMERA_NOISE_REDUCTION, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("noise-reduction-tuning", "Noise Reduction Tuning", "A GstStructure describing noise reduction tuning", DEFAULT_PROP_CAMERA_NOISE_REDUCTION_TUNING, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

gst_param_spec_array ("zoom", "Zoom Rectangle", "Camera zoom rectangle ('<X, Y, WIDTH, HEIGHT >') in sensor active " "pixel array coordinates. Defaults to active-sensor-size values" " for 1x or no zoom", g_param_spec_int ("value", "Zoom Value", "One of X, Y, WIDTH or HEIGHT value.", 0, G_MAXINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("defog-table", "Defog Table", "A GstStructure describing defog table", DEFAULT_PROP_CAMERA_DEFOG_TABLE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("ltm-data", "LTM Data", "A GstStructure describing local tone mapping data", DEFAULT_PROP_CAMERA_LOCAL_TONE_MAPPING, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("infrared-mode", "IR Mode", "Infrared Mode", GST_TYPE_QMMFSRC_IR_MODE, DEFAULT_PROP_CAMERA_IR_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

gst_param_spec_array ("active-sensor-size", "Active Sensor Size", "The active pixel array of the camera sensor ('<X, Y, WIDTH, HEIGHT >')" " and it is filled only when the plugin is in READY or above state", g_param_spec_int ("value", "Sensor Value", "One of X, Y, WIDTH or HEIGHT value.", 0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int ("sensor-mode", "Sensor Mode", "Force set Sensor Mode index (0-15). -1 for Auto selection", -1, 15, DEFAULT_PROP_CAMERA_SENSOR_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_pointer ("video-metadata", "Video Metadata", "Settings and parameters used for submitting capture requests for " "video streams in the form of CameraMetadata object. " "Caller is responsible for releasing the object.", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_pointer ("image-metadata", "Image Metadata", "Settings and parameters used for submitting capture requests for high " "quality images via the capture-image signal in the form of " "CameraMetadata object. Caller is responsible for releasing the object.", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_pointer ("static-metadata", "Static Metadata", "Supported camera capabilities as CameraMetadata object. " "Caller is responsible for releasing the object.", G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_pointer ("session-metadata", "Session Metadata", "Settings parameters used for configure stream as " "CameraMetadata object. Caller is responsible for releasing the object.", G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("frc-mode", "Frame rate control", "Stream frame rate control mode.", GST_TYPE_QMMFSRC_FRC_MODE, DEFAULT_PROP_CAMERA_FRC_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("ife-direct-stream", "IFE direct stream", "IFE direct stream support, with this param, ISP will generate" "output stream from IFE directly and skip others ISP modules" "like IPE", DEFAULT_PROP_CAMERA_IFE_DIRECT_STREAM, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boxed ("static-metas", "Static Metadata's", "It contains the map of each connected camera and its metadata", G_TYPE_HASH_TABLE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("multi-camera-exp-time", "Multi Camera Exposure Time", "The exposure time (in nano-seconds) for each camera in multi camera" " setup ('<exp-time-1, exp-time-2>') and it is used only when" " exposure-mode is OFF", g_param_spec_int ("exp-time", "Exposure Time", "One of exp-time-1, exp-time-2 value.", 0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

gst_param_spec_array ("multi-camera-exp-time", "Multi Camera Exposure Time", "The exposure time (in nano-seconds) for each camera in multi camera" " setup ('<exp-time-1, exp-time-2>') and it is used only when" " exposure-mode is OFF", g_param_spec_int ("exp-time", "Exposure Time", "One of exp-time-1, exp-time-2 value.", 0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_flags ("op-mode", "Camera operation mode", "provide camera operation mode to support specified camera function " "support mode : none, frameselection and fastswitch" "by default camera operation mode is none.", GST_TYPE_QMMFSRC_CAM_OPMODE, DEFAULT_PROP_CAMERA_OPERATION_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("input-roi-enable", "Input ROI reprocess enable", "Input ROI if enabled, Input ROI reprocess usecase will be selected", DEFAULT_PROP_CAMERA_MULTI_ROI, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

gst_param_spec_array ("input-roi-info", "Input ROI info", "Applicable only if input-roi-enable property is set." "input-roi-info is array for each roi ('<X1, Y1, WIDTH1, HEIGHT1" " X2, Y2, WIDTH2, HEIGHT2, ...>')" " it needs to be filled for the no. of Input ROI's" " in playing state", g_param_spec_int ("value", "Input ROI coordinates", "One of X, Y, WIDTH, HEIGHT values.", 0, G_MAXINT, 0, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_int ("camera-switch-index", "set camera index for " "logical camera", "logica camera is a camera having a group of two" "or more physical sensors. logical camera includes several modes, " "SAT mode is where logical camera output the same size as any one of " "the physical sensor. the property is used to switch physical sensor's " "index within logical camera's all available physical sensors in SAT mode" "this property can be used to switch between different physical camera" "by their indexes. for example, camera-index=-1 will set " "next valid physical camera index, and camera-index=2 will select" "physical camera index 2", -1, 10, DEFAULT_PROP_CAMERA_PHYSICAL_CAMERA_SWITCH, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("video-pads-activation-mode", "Video Pad Activation Mode", "set video pad activation mode, by default is normal, use \"signal\" to " "control video pad activation by plugin signal \"video-pads-activation\" " "together with gst_pad_set_active() ", GST_TYPE_QMMFSRC_PAD_ACTIVATION_MODE, DEFAULT_PROP_CAMERA_PAD_ACTIVAION_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_boolean ("multicamera-hint", "multicamera-hint", "multicamera-hint if enabled, this flag will make camera hardwares " "to work in offline which is useful when camera sensors are more then " "camera hardwares, it has impact on memory usage and latency.", DEFAULT_PROP_CAMERA_MULTICAMERA_HINT, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("sw-tnr", "SW TNR", "this flag will enable sw based TNR.", DEFAULT_PROP_CAMERA_SW_TNR, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-activate-deactivate-streams-runtime/main.c](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-activate-deactivate-streams-runtime/main.c)
- [gst-plugin-examples/gst-add-remove-streams-runtime/main.c](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-add-remove-streams-runtime/main.c)
- [gst-plugin-examples/gst-add-streams-as-bundle-example/main.c](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-add-streams-as-bundle-example/main.c)
- [gst-plugin-examples/gst-appsink/main.c](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-appsink/main.c)
- [gst-plugin-examples/gst-appsink-raw-plus-yuv/main.c](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-appsink-raw-plus-yuv/main.c)

### qtiredissink

ML data -> Redis channel; host/port/auth/channel. Bảo vệ secret, không cấu hình password trong log/command history.

Implementation: [gst-plugin-redissink/redissink.c](../../../gst-plugins-qti-oss/gst-plugin-redissink/redissink.c).

Pads: `sink, SINK, ALWAYS`.

Properties tại [redissink.c](../../../gst-plugins-qti-oss/gst-plugin-redissink/redissink.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("host", "Redis service hostname", "Hostname of REDIS service", DEFAULT_PROP_HOSTNAME, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("port", "Redis service port", "Redis service TCP port", 0, G_MAXUINT, DEFAULT_PROP_PORT, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_string ("username", "Redis hostname", "Hostname of REDIS service", DEFAULT_PROP_USERNAME, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_string ("password", "Redis hostname", "Hostname of REDIS service", DEFAULT_PROP_PASSWORD, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_string ("channel", "Redis channels definition", "Redis channels definition", DEFAULT_PROP_CHANNEL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtirestrictedzonedbg

Debug filter theo polygon zone-config. Tên factory kết thúc dbg; không thay intrusion feature production/contract.

Implementation: [gst-plugin-restricted-zone/gstrestrictedzone.c](../../../gst-plugins-qti-oss/gst-plugin-restricted-zone/gstrestrictedzone.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [gstrestrictedzone.c](../../../gst-plugins-qti-oss/gst-plugin-restricted-zone/gstrestrictedzone.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("zone-config", "Restricted Zone config", "Restricted zone configuration" "The format is in GstStructure string. Example multiple Zones can be passed as" "zone-config=\"Zones,zone1=<<100,700>,<750,700>,<750,1000>,<550,1050>,<100,900>>," "zone2=<<1200,700>,<1850,700>,<1850,1000>,<1350,1050>,<1200,900>>;\"", DEFAULT_PROP_ZONE_CONFIG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtirtspbin

RTSP serving bin request sink_%u; mode/address/port/mpoint. Phân biệt address/port strings của plugin với config hệ thống.

Implementation: [gst-plugin-rtspbin/rtspbin.c](../../../gst-plugins-qti-oss/gst-plugin-rtspbin/rtspbin.c).

Pads: `sink_%u, SINK, REQUEST`.

Properties tại [rtspbin.c](../../../gst-plugins-qti-oss/gst-plugin-rtspbin/rtspbin.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("mode", "Mode", "Operational mode", GST_TYPE_RTSPBIN_MODE, DEFAULT_PROP_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_string ("address", "Address", "IP address of the server", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("port", "Port", "Port to listening", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("mpoint", "MPoint", "Mounting point", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py)
- [gst-sample-apps/gst-ai-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-classification/main.c)
- [gst-sample-apps/gst-ai-daisychain-detection-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-daisychain-detection-classification/main.c)
- [gst-sample-apps/gst-ai-daisychain-detection-pose/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-daisychain-detection-pose/main.c)
- [gst-sample-apps/gst-ai-multi-input-output-object-detection/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-multi-input-output-object-detection/main.c)

### qtismartvencbin

Raw + ML/control metadata -> adaptive encoding; smart-framerate/smart-gop/ROI quality. FW owns encode policy.

Implementation: [gst-plugin-smartvencbin/vencbin.c](../../../gst-plugins-qti-oss/gst-plugin-smartvencbin/vencbin.c).

Pads: `sink_ml, SINK, ALWAYS`; `src, SRC, ALWAYS`; `gst_pad_template_new ( sink, SINK, ALWAYS`; `gst_pad_template_new ( sink_ctrl, SINK, ALWAYS`; `gst_pad_template_new ( sink, SINK, ALWAYS`; `gst_pad_template_new ( sink_ctrl, SINK, ALWAYS`.

Properties tại [vencbin.c](../../../gst-plugins-qti-oss/gst-plugin-smartvencbin/vencbin.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("encoder", "Encoder", "Encoder to use (Callable only in NULL state)", GST_TYPE_VENC_BIN_ENCODER, DEFAULT_PROP_ENCODER, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("max-bitrate", "Max bitrate", "Max bitrate in bits per second", 0, G_MAXUINT, DEFAULT_PROP_MAX_BITRATE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_boolean ("smart-framerate", "Smart framerate enable", "Enable/Disable smart framerate functionality", DEFAULT_PROP_SMART_FRAMERATE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_boolean ("smart-gop", "Smart GOP enable", "Enable/Disable smart GOP functionality", DEFAULT_PROP_SMART_GOP, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("default-gop", "Default GOP length", "Default GOP length", 0, G_MAXUINT, DEFAULT_PROP_DEFAULT_GOP_LENGTH, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint ("max-gop", "Max GOP length", "Max GOP length", 0, G_MAXUINT, DEFAULT_PROP_MAX_GOP_LENGTH, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_string ("levels-override", "Levels override", "Override bitrate and FR levels " "e.g. \"LevelsOverride,bitrate_static=160000,bitrate_low=358000," "bitrate_medium=700000,bitrate_high=1400000,fr_static=15,fr_low=3," "fr_medium=1,fr_high=0;\"", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("roi-quality-cfg", "ROI Quality Config", "ROI Quality Config " "e.g. \"ROIQPs,car=2,person=1,tree=-2;\"", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("min-buffers", "Min Buffers", "Min buffers the enc requires to adapt to the set enc properties. " "The default value is overriden based on the video stream.", 0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));
```

</details>

Examples (factory mention):

- [gst-sample-apps/gst-ai-smartcodec-example/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-smartcodec-example/main.c)
- [gst-sample-apps/gst-smartcodec-example/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-smartcodec-example/main.c)

### qtisocketsink

GstBuffer -> Unix SOCK_SEQPACKET/FD transport; socket path. Có return-buffer protocol riêng; không mặc định tương thích Camera IPC hiện tại.

Implementation: [gst-plugin-socket/qtisocketsink.c](../../../gst-plugins-qti-oss/gst-plugin-socket/qtisocketsink.c).

Pads: `sink, SINK, ALWAYS`.

Properties tại [qtisocketsink.c](../../../gst-plugins-qti-oss/gst-plugin-socket/qtisocketsink.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("socket", "Socket Location", "Location of the Unix Domain Socket", NULL, G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtisocketsrc

Nhận protocol của qtisocketsink; FD-backed memory và timestamps. timeout có code paths dùng đơn vị khác nhau: phải kiểm phiên bản target trước đặt timeout nonzero.

Implementation: [gst-plugin-socket/qtisocketsrc.c](../../../gst-plugins-qti-oss/gst-plugin-socket/qtisocketsrc.c).

Pads: `src, SRC, ALWAYS`.

Properties tại [qtisocketsrc.c](../../../gst-plugins-qti-oss/gst-plugin-socket/qtisocketsrc.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("socket", "Socket Location", "Location of the Unix Domain Socket", DEFAULT_SOCKET, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

g_param_spec_uint64 ("timeout", "Socket timeout", "Socket post timeout", 0, G_MAXUINT64, DEFAULT_TIMEOUT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY | G_PARAM_CONSTRUCT));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtivcomposer

Compose nhiều raw streams; request sink_%u có position/dimensions/crop/alpha/zorder/rotation; properties này đặt trên pad.

Implementation: [gst-plugin-vcomposer/videocomposer.c](../../../gst-plugins-qti-oss/gst-plugin-vcomposer/videocomposer.c).

Pads: `sink_%u: SINK REQUEST`; `src: SRC ALWAYS`.

Properties tại [videocomposersinkpad.c](../../../gst-plugins-qti-oss/gst-plugin-vcomposer/videocomposersinkpad.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_int ("zorder", "Z order", "Z axis order, default will be order of creation", (-1), G_MAXINT, DEFAULT_PROP_Z_ORDER, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING | G_PARAM_EXPLICIT_NOTIFY));

gst_param_spec_array ("crop", "Crop rectangle", "The crop rectangle ('<X, Y, WIDTH, HEIGHT >')", g_param_spec_int ("value", "Crop Value", "One of X, Y, WIDTH or HEIGHT value.", 0, G_MAXINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING | G_PARAM_EXPLICIT_NOTIFY));

gst_param_spec_array ("position", "Destination rectangle position", "The X and Y coordinates of the destination rectangle top left " "corner ('<X, Y>')", g_param_spec_int ("coord", "Coordinate", "One of X, Y value.", 0, G_MAXINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING | G_PARAM_EXPLICIT_NOTIFY));

gst_param_spec_array ("dimensions", "Destination rectangle dimensions", "The destination rectangle width and height, if left as '0' they " "will be the same as input dimensions ('<WIDTH, HEIGHT>')", g_param_spec_int ("dim", "Dimension", "One of WIDTH or HEIGHT value.", 0, G_MAXINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING | G_PARAM_EXPLICIT_NOTIFY));

g_param_spec_double ("alpha", "Alpha", "Alpha channel value", 0, 1.0, DEFAULT_PROP_ALPHA, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING | G_PARAM_EXPLICIT_NOTIFY));

g_param_spec_boolean ("flip-horizontal", "Flip horizontally", "Flip video horizontally", DEFAULT_PROP_FLIP_HORIZONTAL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING | G_PARAM_EXPLICIT_NOTIFY));

g_param_spec_boolean ("flip-vertical", "Flip vertically", "Flip video vertically", DEFAULT_PROP_FLIP_VERTICAL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING | G_PARAM_EXPLICIT_NOTIFY));

g_param_spec_enum ("rotate", "Rotate", "Rotate video", GST_TYPE_VIDEO_COMPOSER_ROTATE, DEFAULT_PROP_ROTATE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING | G_PARAM_EXPLICIT_NOTIFY));
```

</details>

Properties tại [videocomposer.c](../../../gst-plugins-qti-oss/gst-plugin-vcomposer/videocomposer.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("engine", "Engine", "Engine backend used for the conversion operations", GST_TYPE_VCE_BACKEND, DEFAULT_PROP_ENGINE_BACKEND, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_uint ("background", "Background", "Background color", 0, 0xFFFFFFFF, DEFAULT_PROP_BACKGROUND, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-snpe-yolo-ssd-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-snpe-yolo-ssd-display-example/main.cc)
- [gst-plugin-examples/gst-snpe-yolo-ssd-encode-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-snpe-yolo-ssd-encode-example/main.cc)
- [gst-plugin-examples/gst-tflite-posenet-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-posenet-display-example/main.cc)
- [gst-plugin-examples/gst-tflite-yolo-ssd-display-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-tflite-yolo-ssd-display-example/main.cc)
- [gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py)

### qtivideotemplate

Hook custom-lib-name/custom-params; library ABI riêng, không là project plugin C ABI.

Implementation: [gst-plugin-videotemplate/qtivideotemplate.c](../../../gst-plugins-qti-oss/gst-plugin-videotemplate/qtivideotemplate.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [qtivideotemplate.c](../../../gst-plugins-qti-oss/gst-plugin-videotemplate/qtivideotemplate.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_string ("custom-lib-name", "Custom library name", "Custom library name eg \"custom-lib.so\"", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("custom-params", "Custom params", "Custom params to configure functionality", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Không tìm thấy factory mention trong example/sample C/C++/Python đã index.

### qtivoverlay

Vẽ bbox/text/timestamp/image/mask từ meta hoặc property string; engine explicit. Chỉ dùng buffer writable thuộc output branch.

Implementation: [gst-plugin-voverlay/overlay.c](../../../gst-plugins-qti-oss/gst-plugin-voverlay/overlay.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [overlay.c](../../../gst-plugins-qti-oss/gst-plugin-voverlay/overlay.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("engine", "Engine", "Engine backend used for the blitting operations", GST_TYPE_VCE_BACKEND, DEFAULT_PROP_ENGINE_BACKEND, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_string ("bboxes", "BBoxes", "Manually set multiple custom bounding boxes in list of GstStructures " "with unique name and 3 parameters 'position', 'dimensions' and 'color'. " "The 'position' and 'dimensions' are mandatory if struct entry is new " "e.g. \"{(structure)\\\"Box1,position=<100,100>,dimensions=<640,480>;" "\\\", (structure)\\\"Box2,position=<1000,100>,dimensions=<300,300>," "color=0xFF0000FF;\\\"}\"", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("timestamps", "Timestamps", "Manually set various timestamps as GstStructures with 'Date/Time' as" " keyword for displaying date and/or time with 4 optional parameters" " 'format', 'fontsize', 'position', and 'color'. And use 'PTS/DTS' " "as keyword dispalying buffer timestamp with 3 optional parameters " "'fontsize', 'position', and 'color' e.g. \"{(structure)\\\"Date/Time" ",format=\\\\\\\"%d/%m/%Y\\ %H:%M:%S\\\\\\\",fontsize=12," "position=<0,0>,color=0xRRGGBBAA;\\\", (structure)\\\"PTS/DTS," "fontsize=12,position=<0,0>,color=0xRRGGBBAA;\\\"}\"", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("strings", "Strings", "Manually set multiple custom strings in list of GstStructures with " "unique name and 4 parameters 'contents', 'fontsize', 'position', " "and 'color'. The 'contents' is mandatory if struct entry is new " "e.g. \"{(structure)\\\"Text1,contents=\\\\\\\"Example\\ 1\\\\\\\"," "fontsize=12,position=<0,0>,color=0xRRGGBBAA;\\\"}\"", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("images", "Images", "Manually set multiple custom BGRA images in list of GstStructures with " "unique name and 3 parameters 'path', 'resolution', 'destination'. " "All 3 are mandatory if struct entry is new e.g. \"{(structure)\\\"" "Image1,path=/data/image1.bgra,resolution=<480,360>,destination=" "<0,0,640,480>;\\\", (structure)\\\"Image2,path=/data/image2.bgra," "resolution=<240,180>,destination=<100,100,480,360>;\\\"}\"", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_string ("masks", "Masks", "Manually set multiple masks in list of GstStructures with unique " "name and 2 parameters 'color' and either 'circle=<X, Y, RADIUS>' or " "'rectangle=<X, Y, WIDTH, HEIGHT>'. Either circle or rectangle must " "be provided if struct entry is new e.g. \"{(structure)" "\\\"Mask1,color=0xRRGGBBAA,circle=<400,400,200>;\\\",(structure)" "\\\"Mask2,color=0xRRGGBBAA,rectangle=<0,0,20,10>;\\\",(structure)" "\\\"Mask3,color=0xRRGGBBAA,polygon=<<2,2>,<2,4>,<4,4>>;\\\"}\"", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
```

</details>

Examples (factory mention):

- [gst-python-examples/gst-ai-object-detection.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-ai-object-detection.py)
- [gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-three-stream-encode-file-detection-display-classification-rtsp.py)
- [gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-two-stream-detection-and-classification-side-by-side.py)
- [gst-python-examples/gst-camera-two-stream-encode-file-detection-display.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-two-stream-encode-file-detection-display.py)
- [gst-python-examples/gst-daisychain-detection-pose.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-daisychain-detection-pose.py)

### qtivsplit

Một raw stream -> request src_%u, mode thuộc output pad; dùng ROI/meta pipeline, không chỉ generic tee.

Implementation: [gst-plugin-vsplit/videosplit.c](../../../gst-plugins-qti-oss/gst-plugin-vsplit/videosplit.c).

Pads: `sink: SINK ALWAYS`; `src_%u: SRC REQUEST`.

Properties tại [videosplitpads.c](../../../gst-plugins-qti-oss/gst-plugin-vsplit/videosplitpads.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("mode", "Mode", "Operational mode", GST_TYPE_VIDEO_SPLIT_MODE, DEFAULT_PROP_MODE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));
```

</details>

Properties tại [videosplit.c](../../../gst-plugins-qti-oss/gst-plugin-vsplit/videosplit.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("engine", "Engine", "Engine backend used for the conversion operations", GST_TYPE_VCE_BACKEND, DEFAULT_PROP_ENGINE_BACKEND, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
```

</details>

Examples (factory mention):

- [gst-python-examples/gst-daisychain-detection-pose.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-daisychain-detection-pose.py)
- [gst-python-examples/gst-gui-launcher-app.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-gui-launcher-app.py)
- [gst-sample-apps/gst-ai-daisychain-detection-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-daisychain-detection-classification/main.c)
- [gst-sample-apps/gst-ai-daisychain-detection-pose/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-daisychain-detection-pose/main.c)

### qtivtransform

Raw video -> raw video: crop/resize/color/flip/rotate. Output width/height/format do capsfilter, engine explicit; crop/destination là arrays.

Implementation: [gst-plugin-vtransform/videotransform.c](../../../gst-plugins-qti-oss/gst-plugin-vtransform/videotransform.c).

Pads: `sink, SINK, ALWAYS`; `src, SRC, ALWAYS`.

Properties tại [videotransform.c](../../../gst-plugins-qti-oss/gst-plugin-vtransform/videotransform.c):

<details>
<summary>Type, description, default expression, flags</summary>

```c
g_param_spec_enum ("engine", "Engine", "Engine backend used for the conversion operations", GST_TYPE_VCE_BACKEND, DEFAULT_PROP_ENGINE_BACKEND, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

g_param_spec_boolean ("flip-horizontal", "Flip horizontally", "Flip video image horizontally", DEFAULT_PROP_FLIP_HORIZONTAL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_boolean ("flip-vertical", "Flip vertically", "Flip video image vertically", DEFAULT_PROP_FLIP_VERTICAL, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_enum ("rotate", "Rotate", "Rotate video image", GST_TYPE_VIDEO_TRANSFORM_ROTATE, DEFAULT_PROP_ROTATE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

gst_param_spec_array ("crop", "Crop rectangle", "The crop rectangle inside the input ('<X, Y, WIDTH, HEIGHT >')", g_param_spec_int ("value", "Crop Value", "One of X, Y, WIDTH or HEIGHT value.", 0, G_MAXINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

gst_param_spec_array ("destination", "Destination rectangle", "Destination rectangle inside the output ('<X, Y, WIDTH, HEIGHT >')", g_param_spec_int ("value", "Crop Value", "One of X, Y, WIDTH or HEIGHT value.", 0, G_MAXINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS), G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

g_param_spec_uint ("background", "Background", "Background color", 0, 0xFFFFFFFF, DEFAULT_PROP_BACKGROUND, G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
```

</details>

Examples (factory mention):

- [gst-plugin-examples/gst-timelapse-example/main.cc](../../../gst-plugins-qti-oss/gst-plugin-examples/gst-timelapse-example/main.cc)
- [gst-python-examples/gst-camera-rotate-downscale-file.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-camera-rotate-downscale-file.py)
- [gst-python-examples/gst-gui-launcher-app.py](../../../gst-plugins-qti-oss/gst-python-examples/gst-gui-launcher-app.py)
- [gst-sample-apps/gst-ai-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-classification/main.c)
- [gst-sample-apps/gst-ai-daisychain-detection-classification/main.c](../../../gst-plugins-qti-oss/gst-sample-apps/gst-ai-daisychain-detection-classification/main.c)

## 10. Các thư mục không phải element factory

gst-plugin-base: allocator/GBM/gfx/video converter/tensor metadata/module loading/
utils; là thư viện nền, không một factory có tên gst-plugin-base.
gst-plugin-tools, gst-plugin-mltools: tools và test apps; không deploy như feature.
gst-plugin-examples, gst-sample-apps, gst-python-examples: sample wiring, configs.
gst-test-framework: test infrastructure; gst-docker-ref/debian: build/package;
gst-umd-daemon: daemon/support, không tự là AI runtime project này.
Không mọi thư mục gst-plugin-* tương ứng đúng một factory (codec2 có bốn).

## 11. Quy tắc cập nhật

Khi source/image đổi: ghi commit + gst-inspect version, đối chiếu factory/property/
enum/pads/default/mutability; update reference trước code adapter. Không thay
model/preprocess/ABI âm thầm. Snapshot này không tự chứng minh image 1.8 dùng đúng
commit source; link pinned checkout để truy vết và probe runtime khi triển khai.

Chưa build hoặc chạy lệnh target trong đợt này theo yêu cầu người dùng.
