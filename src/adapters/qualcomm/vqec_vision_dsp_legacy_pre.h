#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t* y;
    uint8_t* uv;
    uint8_t* rgb;
    int w;
    int h;
} PreScratch;

int pre_scratch_ensure(PreScratch* s, int w, int h);
void pre_scratch_free(PreScratch* s);

/* Letterbox resize NV12 to u16 NHWC RGB tensor.
   geom is 12 int32 entries: SRC_W, SRC_H, Y_STRIDE, UV_OFFSET, UV_STRIDE,
   TENSOR_W, TENSOR_H, DST_X, DST_Y, DST_W, DST_H, PAD.
   Returns 0 on success, or non-zero error code. */
int pre_letterbox_rgb_u16(PreScratch* s, const uint8_t* frame, int frame_len,
                          const int32_t* geom, uint16_t* tensor, int tensor_len);

#ifdef __cplusplus
}
#endif
