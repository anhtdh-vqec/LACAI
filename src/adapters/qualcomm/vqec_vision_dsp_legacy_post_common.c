#include "vqec_vision_dsp_legacy_post_common.h"
#include <math.h>

uint32_t vq_quant_threshold(float thr, float scale, float offset) {
  float r = ceilf(thr / scale - offset); if (r < 0) return 0; if (r > 65535.f) return 65535; return (uint32_t)r;
}
float vq_sigmoid(float x) { return 1.f / (1.f + expf(-x)); }
float vq_logit(float p) { if (p < 1e-6f) p = 1e-6f; if (p > 1.f - 1e-6f) p = 1.f - 1e-6f; return logf(p / (1.f - p)); }
int cand_push(CandList* l, float x1, float y1, float x2, float y2, float score, int cls) {
  int slot;
  if (l->n < VQEC_MAX_CAND) {
    slot = l->n++;
  } else {
    /* Full: keep the best VQEC_MAX_CAND candidates instead of the first VQEC_MAX_CAND in scan
       order. The linear scan only runs once the cap is reached and is bounded by 512 entries. */
    int worst = 0;
    for (int i = 1; i < VQEC_MAX_CAND; ++i) if (l->c[i].score < l->c[worst].score) worst = i;
    ++l->truncated;
    if (score <= l->c[worst].score) return -1;
    slot = worst;
  }
  Cand* c = &l->c[slot]; c->x1 = x1; c->y1 = y1; c->x2 = x2; c->y2 = y2; c->score = score; c->cls = cls; return slot;
}
static float iou(const Cand* a, const Cand* b) {
  float ix = fminf(a->x2, b->x2) - fmaxf(a->x1, b->x1), iy = fminf(a->y2, b->y2) - fmaxf(a->y1, b->y1);
  if (ix <= 0 || iy <= 0) return 0;
  float inter = ix * iy, ua = (a->x2 - a->x1) * (a->y2 - a->y1) + (b->x2 - b->x1) * (b->y2 - b->y1) - inter;
  return ua > 0 ? inter / ua : 0;
}
int nms_greedy(const CandList* l, float iou_thr, int class_aware, int* out_idx, int max_out) {
  int order[VQEC_MAX_CAND]; int n = l->n;
  for (int i = 0; i < n; ++i) order[i] = i;
  for (int i = 1; i < n; ++i) { int k = order[i], j = i - 1; while (j >= 0 && l->c[order[j]].score < l->c[k].score) { order[j + 1] = order[j]; --j; } order[j + 1] = k; }  /* stable insertion sort, desc */
  int kept = 0;
  for (int i = 0; i < n && kept < max_out; ++i) {
    const Cand* c = &l->c[order[i]]; int sup = 0;
    for (int j = 0; j < kept && !sup; ++j) { const Cand* s = &l->c[out_idx[j]]; if ((!class_aware || s->cls == c->cls) && iou(c, s) > iou_thr) sup = 1; }
    if (!sup) out_idx[kept++] = order[i];
  }
  return kept;
}
static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
void boxes_write(const CandList* l, const int* idx, int n, float src_w, float src_h, float* out) {
  for (int i = 0; i < n; ++i) { const Cand* c = &l->c[idx[i]]; float* o = out + i * VQEC_BOX_FLOATS;
    o[0] = clamp01(c->x1 / src_w); o[1] = clamp01(c->y1 / src_h); o[2] = clamp01(c->x2 / src_w); o[3] = clamp01(c->y2 / src_h); o[4] = c->score; o[5] = (float)c->cls; }
}
