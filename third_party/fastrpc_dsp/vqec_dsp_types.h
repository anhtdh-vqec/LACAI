#pragma once
#define VQEC_GEOM_INTS 12
enum { VQEC_GEOM_SRC_W = 0, VQEC_GEOM_SRC_H, VQEC_GEOM_Y_STRIDE, VQEC_GEOM_UV_OFFSET, VQEC_GEOM_UV_STRIDE,
       VQEC_GEOM_TENSOR_W, VQEC_GEOM_TENSOR_H, VQEC_GEOM_DST_X, VQEC_GEOM_DST_Y, VQEC_GEOM_DST_W, VQEC_GEOM_DST_H, VQEC_GEOM_PAD };
#define VQEC_POST_PARAM_FLOATS 8
enum { VQEC_PP_CONF = 0, VQEC_PP_NMS, VQEC_PP_SCALE, VQEC_PP_PAD_X, VQEC_PP_PAD_Y, VQEC_PP_SRC_W, VQEC_PP_SRC_H, VQEC_PP_RESERVED };
#define VQEC_QUANT_FLOATS 2          /* scale, offset per tensor, in model output order */
#define VQEC_BOX_FLOATS 6            /* x1, y1, x2, y2 normalized to the source frame, score 0..1, class_id */
#define VQEC_KPS_FLOATS 10           /* 5 (x, y) pairs in source pixels */
#define VQEC_MAX_BOXES 64
#define VQEC_MAX_CAND 512
#define VQEC_MAX_FRAME_SIDE 8192      /* sanity bound on a source frame dimension (pre/compose geometry) */
#define VQEC_CGEOM_INTS 8            /* compose: src_w, src_h, src_y_stride, src_uv_offset, src_uv_stride, dst_y_stride, dst_uv_offset, dst_uv_stride */
enum { VQEC_CG_SRC_W = 0, VQEC_CG_SRC_H, VQEC_CG_SRC_Y_STRIDE, VQEC_CG_SRC_UV_OFFSET, VQEC_CG_SRC_UV_STRIDE, VQEC_CG_DST_Y_STRIDE, VQEC_CG_DST_UV_OFFSET, VQEC_CG_DST_UV_STRIDE };
#define VQEC_DRAW_FLOATS 8           /* x1, y1, x2, y2 normalized, color_idx, track_id, score_pct, label_idx */
enum { VQEC_DRAW_X1 = 0, VQEC_DRAW_Y1, VQEC_DRAW_X2, VQEC_DRAW_Y2, VQEC_DRAW_COLOR, VQEC_DRAW_TRACK, VQEC_DRAW_SCORE, VQEC_DRAW_LABEL };
enum { VQEC_COLOR_GREEN = 0, VQEC_COLOR_YELLOW, VQEC_COLOR_RED, VQEC_COLOR_ORANGE, VQEC_COLOR_WHITE, VQEC_COLOR_COUNT };
