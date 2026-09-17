#include "post_face_scrfd.h"
#include "post_common.h"
#include <math.h>

int post_face_scrfd(CandList* scratch, const uint16_t* const t[9], const float* quant, const float* params, float* out_boxes, float* out_kps, int max_boxes) {
  if (!scratch || !t || !quant || !params || !out_boxes || !out_kps || max_boxes <= 0) return -1;
  for (int i = 0; i < 9; ++i) if (!t[i]) return -1;
  const float conf_thr = params[VQEC_PP_CONF], nms_thr = params[VQEC_PP_NMS], scale = params[VQEC_PP_SCALE];
  const float W = params[VQEC_PP_SRC_W], H = params[VQEC_PP_SRC_H];
  if (scale <= 0 || W <= 0 || H <= 0) return -1;
  const int strides[3] = {8, 16, 32};

  CandList* l = scratch; l->n = 0; l->truncated = 0;

  for (int L = 0; L < 3; ++L) {
    const int stride = strides[L];
    const int g = SCRFD_INPUT / stride;
    const int count = g * g * SCRFD_ANCHORS_PER_CELL;
    const uint16_t* score = t[L];
    const uint16_t* bbox = t[3 + L];
    const uint16_t* kps = t[6 + L];
    const float s_scale = quant[2 * L], s_off = quant[2 * L + 1];
    const float b_scale = quant[2 * (3 + L)], b_off = quant[2 * (3 + L) + 1];
    const float k_scale = quant[2 * (6 + L)], k_off = quant[2 * (6 + L) + 1];
    const uint32_t q_thr = vq_quant_threshold(conf_thr, s_scale, s_off);

    for (int i = 0; i < count; ++i) {
      if (score[i] < q_thr) continue;
      const float sc = vq_dequant(score[i], s_scale, s_off);
      if (sc < conf_thr) continue; /* q_thr saturates at 65535 when thr is unreachable; recheck the real value */
      const int cell = i / SCRFD_ANCHORS_PER_CELL;
      const float cx = (float)(cell % g) * stride, cy = (float)(cell / g) * stride;
      const uint16_t* b = bbox + i * 4;
      const float b0 = vq_dequant(b[0], b_scale, b_off), b1 = vq_dequant(b[1], b_scale, b_off);
      const float b2 = vq_dequant(b[2], b_scale, b_off), b3 = vq_dequant(b[3], b_scale, b_off);
      float x1 = (cx - b0 * stride) / scale, y1 = (cy - b1 * stride) / scale;
      float x2 = (cx + b2 * stride) / scale, y2 = (cy + b3 * stride) / scale;
      x1 = fminf(fmaxf(x1, 0), W); y1 = fminf(fmaxf(y1, 0), H); x2 = fminf(fmaxf(x2, 0), W); y2 = fminf(fmaxf(y2, 0), H);
      if (x2 <= x1 || y2 <= y1) continue;
      const int ci = cand_push(l, x1, y1, x2, y2, sc, 0);   /* -1: the cap dropped this one */
      if (ci < 0) continue;

      const uint16_t* k = kps + i * 10;
      float* okps = l->kps[ci];
      for (int j = 0; j < 5; ++j) {
        const float kdx = vq_dequant(k[2 * j], k_scale, k_off), kdy = vq_dequant(k[2 * j + 1], k_scale, k_off);
        float kx = (cx + kdx * stride) / scale, ky = (cy + kdy * stride) / scale;
        kx = fminf(fmaxf(kx, 0), W); ky = fminf(fmaxf(ky, 0), H);
        okps[2 * j] = kx; okps[2 * j + 1] = ky;
      }
    }
  }

  int idx[VQEC_MAX_BOXES];
  const int n = nms_greedy(l, nms_thr, 0, idx, max_boxes < VQEC_MAX_BOXES ? max_boxes : VQEC_MAX_BOXES);
  boxes_write(l, idx, n, W, H, out_boxes);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < VQEC_KPS_FLOATS; ++j) out_kps[i * VQEC_KPS_FLOATS + j] = l->kps[idx[i]][j];
  }
  return n;
}
