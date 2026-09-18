#pragma once
#include <stdint.h>
#include "vqec_vision_dsp_legacy_post_common.h"

/* tensors in model output order: score8 [12800,1], score16 [3200,1], score32 [800,1], bbox8 [12800,4], bbox16, bbox32, kps8 [12800,10], kps16, kps32.
   quant: 18 floats (scale, offset) x 9 in that order. out_boxes: VQEC_BOX_FLOATS * max; out_kps: VQEC_KPS_FLOATS * max (source pixels). */
int post_face_scrfd(CandList* scratch, const uint16_t* const t[9], const float* quant, const float* params, float* out_boxes, float* out_kps, int max_boxes);
#define SCRFD_INPUT 640
#define SCRFD_ANCHORS_PER_CELL 2
