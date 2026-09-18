#pragma once
/* Return codes of the vqec_dsp skel beyond AEEStdErr (host sees them verbatim as 32-bit values). */
#define VQEC_DSP_E_NOSLOT      0x80000601  /* prev_pts or next_pts is not in the history ring */
#define VQEC_DSP_E_TOOMANY     0x80000602  /* sum of max_points over the request exceeds VQEC_DSP_MAX_POINTS */
#define VQEC_DSP_E_GEOM        0x80000603  /* geometry inconsistent with buffer sizes */
#define VQEC_DSP_E_SIZE        0x80000604  /* sequence shorter than the layout requires */
#define VQEC_DSP_E_DLOPEN      0x80000610  /* libfastcvadsp.so not found (DSP_LIBRARY_PATH) */
#define VQEC_DSP_E_DLSYM_BASE  0x80000620  /* + index of the missing FastCV symbol */
#define VQEC_DSP_MAX_POINTS    4096
#define VQEC_DSP_ROI_INTS      9
#define VQEC_DSP_MAX_LEVELS    6
