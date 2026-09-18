#include "vqec_vision_dsp_legacy_pre.h"
#include "vqec_vision_dsp_legacy_types.h"
#include "vqec_vision_dsp_legacy_codes.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

int pre_scratch_ensure(PreScratch* s, int w, int h) {
    if (s == NULL) return -1;
    if (s->y && s->uv && s->rgb && s->w == w && s->h == h) return 0;
    pre_scratch_free(s);
    uint8_t* y = (uint8_t*)malloc((size_t)w * (size_t)h);
    uint8_t* uv = (uint8_t*)malloc((size_t)(w / 2) * (size_t)(h / 2) * 2);
    uint8_t* rgb = (uint8_t*)malloc((size_t)w * (size_t)h * 3);
    if (!y || !uv || !rgb) {
        free(y);
        free(uv);
        free(rgb);
        return -1;
    }
    s->y = y;
    s->uv = uv;
    s->rgb = rgb;
    s->w = w;
    s->h = h;
    return 0;
}

void pre_scratch_free(PreScratch* s) {
    if (s == NULL) return;
    free(s->y);
    free(s->uv);
    free(s->rgb);
    s->y = NULL;
    s->uv = NULL;
    s->rgb = NULL;
    s->w = 0;
    s->h = 0;
}

static uint8_t round_avg(uint32_t sum, uint32_t cnt) {
    return (uint8_t)((sum + cnt / 2) / cnt);
}

static void scalar_ScaleDownMNu8(const uint8_t* src, uint32_t srcW, uint32_t srcH, uint32_t srcStride,
                                uint8_t* dst, uint32_t dstW, uint32_t dstH, uint32_t dstStride) {
    for (uint32_t dy = 0; dy < dstH; ++dy) {
        uint32_t y0 = (uint32_t)((uint64_t)dy * srcH / dstH);
        uint32_t y1 = (uint32_t)((uint64_t)(dy + 1) * srcH / dstH);
        if (y1 <= y0) y1 = y0 + 1;
        for (uint32_t dx = 0; dx < dstW; ++dx) {
            uint32_t x0 = (uint32_t)((uint64_t)dx * srcW / dstW);
            uint32_t x1 = (uint32_t)((uint64_t)(dx + 1) * srcW / dstW);
            if (x1 <= x0) x1 = x0 + 1;
            uint32_t sum = 0, cnt = 0;
            for (uint32_t y = y0; y < y1; ++y) {
                for (uint32_t x = x0; x < x1; ++x) {
                    sum += src[y * srcStride + x];
                    ++cnt;
                }
            }
            dst[dy * dstStride + dx] = round_avg(sum, cnt);
        }
    }
}

static void scalar_ScaleDownMNInterleaveu8(const uint8_t* src, uint32_t srcW, uint32_t srcH, uint32_t srcStride,
                                          uint8_t* dst, uint32_t dstW, uint32_t dstH, uint32_t dstStride) {
    for (uint32_t dy = 0; dy < dstH; ++dy) {
        uint32_t y0 = (uint32_t)((uint64_t)dy * srcH / dstH);
        uint32_t y1 = (uint32_t)((uint64_t)(dy + 1) * srcH / dstH);
        if (y1 <= y0) y1 = y0 + 1;
        for (uint32_t dx = 0; dx < dstW; ++dx) {
            uint32_t x0 = (uint32_t)((uint64_t)dx * srcW / dstW);
            uint32_t x1 = (uint32_t)((uint64_t)(dx + 1) * srcW / dstW);
            if (x1 <= x0) x1 = x0 + 1;
            uint32_t sum0 = 0, sum1 = 0, cnt = 0;
            for (uint32_t y = y0; y < y1; ++y) {
                for (uint32_t x = x0; x < x1; ++x) {
                    const uint8_t* p = src + y * srcStride + x * 2;
                    sum0 += p[0];
                    sum1 += p[1];
                    ++cnt;
                }
            }
            uint8_t* dp = dst + dy * dstStride + dx * 2;
            dp[0] = round_avg(sum0, cnt);
            dp[1] = round_avg(sum1, cnt);
        }
    }
}

static uint8_t clamp_round(float v) {
    int i = (int)(v + (v >= 0.0f ? 0.5f : -0.5f));
    if (i < 0) return 0;
    if (i > 255) return 255;
    return (uint8_t)i;
}

static void scalar_ColorYCbCr420PseudoPlanarToRGB888u8(const uint8_t* srcY, const uint8_t* srcC,
                                                       uint32_t w, uint32_t h,
                                                       uint32_t yStride, uint32_t cStride,
                                                       uint8_t* dst, uint32_t dstStride) {
    for (uint32_t r = 0; r < h; ++r) {
        const uint8_t* yrow = srcY + r * yStride;
        const uint8_t* crow = srcC + (r / 2) * cStride;
        uint8_t* drow = dst + r * dstStride;
        for (uint32_t c = 0; c < w; ++c) {
            int yy = yrow[c];
            int cb = (int)crow[(c / 2) * 2] - 128;
            int cr = (int)crow[(c / 2) * 2 + 1] - 128;
            drow[c * 3 + 0] = clamp_round((float)yy + 1.402f * (float)cr);
            drow[c * 3 + 1] = clamp_round((float)yy - 0.344f * (float)cb - 0.714f * (float)cr);
            drow[c * 3 + 2] = clamp_round((float)yy + 1.772f * (float)cb);
        }
    }
}

int pre_letterbox_rgb_u16(PreScratch* s, const uint8_t* frame, int frame_len,
                          const int32_t* geom, uint16_t* tensor, int tensor_len) {
    if (s == NULL || frame == NULL || geom == NULL || tensor == NULL) return VQEC_DSP_E_GEOM;

    int src_w = geom[VQEC_GEOM_SRC_W], src_h = geom[VQEC_GEOM_SRC_H];
    int y_stride = geom[VQEC_GEOM_Y_STRIDE];
    int uv_offset = geom[VQEC_GEOM_UV_OFFSET], uv_stride = geom[VQEC_GEOM_UV_STRIDE];
    int tw = geom[VQEC_GEOM_TENSOR_W], th = geom[VQEC_GEOM_TENSOR_H];
    int dst_x = geom[VQEC_GEOM_DST_X], dst_y = geom[VQEC_GEOM_DST_Y];
    int dst_w = geom[VQEC_GEOM_DST_W], dst_h = geom[VQEC_GEOM_DST_H];
    int pad = geom[VQEC_GEOM_PAD];

    if (src_w <= 0 || src_h <= 0 || src_w > VQEC_MAX_FRAME_SIDE || src_h > VQEC_MAX_FRAME_SIDE) return VQEC_DSP_E_GEOM;
    if (dst_w <= 0 || dst_h <= 0 || (dst_w % 2) != 0 || (dst_h % 2) != 0) return VQEC_DSP_E_GEOM;
    if (dst_x < 0 || dst_y < 0 || dst_x + dst_w > tw || dst_y + dst_h > th) return VQEC_DSP_E_GEOM;
    if (y_stride < src_w || uv_stride < src_w) return VQEC_DSP_E_GEOM;
    if ((int64_t)uv_offset < (int64_t)y_stride * (int64_t)src_h) return VQEC_DSP_E_GEOM;
    if ((int64_t)frame_len < (int64_t)uv_offset + (int64_t)uv_stride * (int64_t)(src_h / 2)) return VQEC_DSP_E_SIZE;
    if ((int64_t)tensor_len < (int64_t)tw * (int64_t)th * 3) return VQEC_DSP_E_SIZE;

    if (pre_scratch_ensure(s, dst_w, dst_h) != 0) return VQEC_DSP_E_SIZE;

    const uint8_t* Y = frame;
    const uint8_t* UV = frame + uv_offset;
    scalar_ScaleDownMNu8(Y, (uint32_t)src_w, (uint32_t)src_h, (uint32_t)y_stride,
                         s->y, (uint32_t)dst_w, (uint32_t)dst_h, (uint32_t)dst_w);
    scalar_ScaleDownMNInterleaveu8(UV, (uint32_t)(src_w / 2), (uint32_t)(src_h / 2), (uint32_t)uv_stride,
                                  s->uv, (uint32_t)(dst_w / 2), (uint32_t)(dst_h / 2), (uint32_t)dst_w);
    scalar_ColorYCbCr420PseudoPlanarToRGB888u8(s->y, s->uv, (uint32_t)dst_w, (uint32_t)dst_h,
                                              (uint32_t)dst_w, (uint32_t)dst_w, s->rgb, (uint32_t)(dst_w * 3));

    uint16_t padv = (uint16_t)(pad * 257);
    for (int r = 0; r < dst_y; ++r) {
        uint16_t* trow = tensor + r * tw * 3;
        for (int c = 0; c < tw * 3; ++c) trow[c] = padv;
    }
    for (int r = dst_y + dst_h; r < th; ++r) {
        uint16_t* trow = tensor + r * tw * 3;
        for (int c = 0; c < tw * 3; ++c) trow[c] = padv;
    }
    for (int r = dst_y; r < dst_y + dst_h; ++r) {
        uint16_t* trow = tensor + r * tw * 3;
        for (int c = 0; c < dst_x * 3; ++c) trow[c] = padv;
        for (int c = (dst_x + dst_w) * 3; c < tw * 3; ++c) trow[c] = padv;
    }

    for (int r = 0; r < dst_h; ++r) {
        const uint8_t* rgbrow = s->rgb + r * dst_w * 3;
        uint16_t* trow = tensor + ((dst_y + r) * tw + dst_x) * 3;
        for (int c = 0; c < dst_w * 3; ++c) {
            uint8_t v = rgbrow[c];
            trow[c] = (uint16_t)(((uint16_t)v << 8) | v);
        }
    }
    return 0;
}
