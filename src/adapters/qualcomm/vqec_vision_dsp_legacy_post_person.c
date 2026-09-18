#include "vqec_vision_dsp_legacy_post_person.h"
#include "vqec_vision_dsp_legacy_post_common.h"
#include <math.h>

int post_person_yolov8n(CandList* scratch, const uint16_t* boxes_t, const uint16_t* conf_t, const float* quant, const float* params, float* out, int max_boxes) {
  if (!scratch || !boxes_t || !conf_t || !quant || !params || !out || max_boxes <= 0) return -1;
  const float bs = quant[0], bo = quant[1], cs = quant[2], co = quant[3];
  const float conf_thr = params[VQEC_PP_CONF], nms_thr = params[VQEC_PP_NMS], scale = params[VQEC_PP_SCALE];
  const float pad_x = params[VQEC_PP_PAD_X], pad_y = params[VQEC_PP_PAD_Y], W = params[VQEC_PP_SRC_W], H = params[VQEC_PP_SRC_H];
  if (scale <= 0 || W <= 0 || H <= 0) return -1;
  const uint32_t q_thr = vq_quant_threshold(conf_thr, cs, co);
  CandList* l = scratch; l->n = 0; l->truncated = 0;
  const int N = YOLOV8N_PERSON_PREDICTIONS;
  for (int i = 0; i < N; ++i) {
    if (conf_t[i] < q_thr) continue;
    const float conf = vq_dequant(conf_t[i], cs, co);
    if (conf < conf_thr) continue; /* q_thr saturates at 65535 when thr is unreachable; recheck the real value */
    const float cx = vq_dequant(boxes_t[0 * N + i], bs, bo), cy = vq_dequant(boxes_t[1 * N + i], bs, bo);
    const float w = vq_dequant(boxes_t[2 * N + i], bs, bo), h = vq_dequant(boxes_t[3 * N + i], bs, bo);
    float x1 = (cx - w * 0.5f - pad_x) / scale, y1 = (cy - h * 0.5f - pad_y) / scale;
    float x2 = (cx + w * 0.5f - pad_x) / scale, y2 = (cy + h * 0.5f - pad_y) / scale;
    x1 = fminf(fmaxf(x1, 0), W - 1); y1 = fminf(fmaxf(y1, 0), H - 1); x2 = fminf(fmaxf(x2, 0), W - 1); y2 = fminf(fmaxf(y2, 0), H - 1);
    if (x2 <= x1 || y2 <= y1) continue;
    cand_push(l, x1, y1, x2, y2, conf, 0);   /* keeps the best VQEC_MAX_CAND, counts overflow in l->truncated */
  }
  int idx[VQEC_MAX_BOXES];
  const int n = nms_greedy(l, nms_thr, 0, idx, max_boxes < VQEC_MAX_BOXES ? max_boxes : VQEC_MAX_BOXES);
  boxes_write(l, idx, n, W, H, out);
  return n;
}
