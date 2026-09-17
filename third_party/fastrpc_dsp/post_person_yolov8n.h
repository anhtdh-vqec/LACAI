#pragma once
#include <stdint.h>
#include "post_common.h"

/* boxes_t: [1,4,8400] u16 (channel-major), conf_t: [1,1,8400] u16. quant: 4 floats (boxes scale/offset, conf scale/offset).
   params: VQEC_POST_PARAM_FLOATS. out: VQEC_BOX_FLOATS * max_boxes. Returns count, or -1 on bad args. */
int post_person_yolov8n(CandList* scratch, const uint16_t* boxes_t, const uint16_t* conf_t, const float* quant, const float* params, float* out, int max_boxes);
#define YOLOV8N_PERSON_PREDICTIONS 8400
