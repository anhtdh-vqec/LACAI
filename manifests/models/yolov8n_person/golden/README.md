# Golden references (pending)

MI-03/MI-04 require reference data produced by the model team's reference pipeline (or the
Python export pipeline), stored here and kept small / privacy-safe:

| File | Produced by | Consumed by |
|---|---|---|
| `frame_001.nv12` | a privacy-safe NV12 fixture | preprocess golden (MI-03) |
| `expected_input_tensor.bin` | reference preprocess | preprocess golden (MI-03) |
| `raw_boxes_out.bin`, `raw_conf_out.bin` | reference QNN run for `frame_001` | raw parity (MI-03/MI-04) |
| `expected_detections.json` | reference decode + NMS | decoded parity (MI-04) |

Cover at least: landscape, portrait, letterbox, odd aspect ratio and an object near a
border. Do not commit a model binary or any personal data. A tensor digest is a value
comparison, not artifact authentication.
