#pragma once
#include <stdint.h>
#include "vqec_dsp_types.h"

typedef struct { float x1, y1, x2, y2, score; int cls; } Cand;   /* pixels in source frame */
/* `truncated` counts the overflow events of the last decode (reset by every post_* entry point):
   how many candidates hit the VQEC_MAX_CAND cap, whether they replaced a weaker one or were
   dropped. It is reported to the host through the postprocess_* RPCs' `truncated` out-parameter. */
typedef struct { Cand c[VQEC_MAX_CAND]; float kps[VQEC_MAX_CAND][VQEC_KPS_FLOATS]; int n; int truncated; } CandList;   /* one per RPC handle (ctx), never static */

static inline float vq_dequant(uint16_t q, float scale, float offset) { return ((float)q + offset) * scale; }

/* raw threshold: smallest q whose dequantized value >= thr (saturates to 65535 if unreachable) */
uint32_t vq_quant_threshold(float thr, float scale, float offset);
float vq_sigmoid(float x);
float vq_logit(float p);                     /* ln(p/(1-p)), p clamped to [1e-6, 1-1e-6] */
/* Stores one candidate and returns the index it landed in (use it for kps), or -1 when the list
   is full and `score` is not better than the weakest candidate already held. Once the list is
   full the weakest candidate is replaced rather than dropping everything later in scan order, so
   the cap costs the lowest-scoring detections, not the last-scanned ones. Every overflow event
   (replacement or drop) increments l->truncated. */
int cand_push(CandList* l, float x1, float y1, float x2, float y2, float score, int cls);
/* greedy NMS by descending score; class_aware=1 suppresses only same class. Writes indices, returns count (<= max_out). */
int nms_greedy(const CandList* l, float iou_thr, int class_aware, int* out_idx, int max_out);
/* writes VQEC_BOX_FLOATS per kept box, normalized by src_w/src_h and clipped to [0,1] */
void boxes_write(const CandList* l, const int* idx, int n, float src_w, float src_h, float* out);
