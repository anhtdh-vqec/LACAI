#include <vqec_vision_dsp_v1.h>

#include <AEEStdErr.h>
#include <qurt_mutex.h>
#include <qurt_sclk.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vqec_vision_dsp_v1_service.h"

#define VQEC_VISION_AI_DSP_V1_SKELETON_MAX_SESSIONS 4U
#define VQEC_VISION_AI_DSP_V1_SKELETON_HANDLE_SLOT_MASK 0xffffffffULL

typedef struct vqec_vision_ai_dsp_v1_skeleton_context {
    uint32_t active;
    uint32_t generation;
    void* fastcv_library;
    vqec_vision_ai_dsp_v1_service service;
} vqec_vision_ai_dsp_v1_skeleton_context;

static qurt_mutex_t g_vqec_vision_ai_dsp_v1_pool_mutex = QURT_MUTEX_INIT;
static qurt_mutex_t
    g_vqec_vision_ai_dsp_v1_execution_mutexes[VQEC_VISION_AI_DSP_V1_SKELETON_MAX_SESSIONS] = {
        QURT_MUTEX_INIT, QURT_MUTEX_INIT, QURT_MUTEX_INIT, QURT_MUTEX_INIT};
static uint32_t g_vqec_vision_ai_dsp_v1_last_generation;
static vqec_vision_ai_dsp_v1_skeleton_context
    g_vqec_vision_ai_dsp_v1_contexts[VQEC_VISION_AI_DSP_V1_SKELETON_MAX_SESSIONS];

static int vqec_vision_ai_qcom_d1skl_load_image_backend(
    vqec_vision_ai_dsp_v1_skeleton_context* _context) {
    static const char g_fastcv_library_name[] = "libfastcvadsp.so";
    static const char g_scale_luma_symbol[] = "fcvScaleDownMNu8";
    static const char g_scale_chroma_symbol[] = "fcvScaleDownMNInterleaveu8";
    static const char g_color_symbol[] = "fcvColorYCbCr420PseudoPlanarToRGB888u8";
    vqec_vision_ai_dsp_v1_image_backend backend;
    memset(&backend, 0, sizeof(backend));
    _context->fastcv_library = dlopen(g_fastcv_library_name, RTLD_NOW);
    if (_context->fastcv_library == NULL) {
        return 0;
    }
    void* symbol = dlsym(_context->fastcv_library, g_scale_luma_symbol);
    memcpy(&backend.scale_luma, &symbol, sizeof(backend.scale_luma));
    symbol = dlsym(_context->fastcv_library, g_scale_chroma_symbol);
    memcpy(&backend.scale_chroma, &symbol, sizeof(backend.scale_chroma));
    symbol = dlsym(_context->fastcv_library, g_color_symbol);
    memcpy(&backend.convert_color, &symbol, sizeof(backend.convert_color));
    if (vqec_vision_ai_qcom_d1svc_configure_image_backend(&_context->service, &backend) !=
        vqec_vision_ai_dsp_v1_wire_ok) {
        dlclose(_context->fastcv_library);
        _context->fastcv_library = NULL;
        return 0;
    }
    return 1;
}

static uint32_t vqec_vision_ai_qcom_d1skl_next_generation(void) {
    uint32_t generation = (uint32_t)qurt_sysclock_get_hw_ticks();
    if (generation == 0U || generation == g_vqec_vision_ai_dsp_v1_last_generation) {
        generation = g_vqec_vision_ai_dsp_v1_last_generation + 1U;
        if (generation == 0U) {
            generation = 1U;
        }
    }
    g_vqec_vision_ai_dsp_v1_last_generation = generation;
    return generation;
}

static remote_handle64 vqec_vision_ai_qcom_d1skl_encode_handle(uint32_t _slot,
                                                               uint32_t _generation) {
    return ((remote_handle64)_generation << 32U) | (remote_handle64)(_slot + 1U);
}

static vqec_vision_ai_dsp_v1_skeleton_context*
vqec_vision_ai_qcom_d1skl_lock_context(remote_handle64 _handle) {
    const uint32_t encoded_slot =
        (uint32_t)(_handle & VQEC_VISION_AI_DSP_V1_SKELETON_HANDLE_SLOT_MASK);
    const uint32_t generation = (uint32_t)(_handle >> 32U);
    if (encoded_slot == 0U || encoded_slot > VQEC_VISION_AI_DSP_V1_SKELETON_MAX_SESSIONS ||
        generation == 0U) {
        return NULL;
    }
    vqec_vision_ai_dsp_v1_skeleton_context* context =
        &g_vqec_vision_ai_dsp_v1_contexts[encoded_slot - 1U];
    qurt_mutex_lock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
    if (context->active == 0U || context->generation != generation) {
        qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
        return NULL;
    }
    qurt_mutex_lock(&g_vqec_vision_ai_dsp_v1_execution_mutexes[encoded_slot - 1U]);
    qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
    return context;
}

static void
vqec_vision_ai_qcom_d1skl_unlock_context(vqec_vision_ai_dsp_v1_skeleton_context* _context) {
    const size_t slot = (size_t)(_context - g_vqec_vision_ai_dsp_v1_contexts);
    qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_execution_mutexes[slot]);
}

/* These four names are fixed external entry points generated from the project-owned IDL. */
int vqec_vision_dsp_v1_open(const char* _uri, remote_handle64* _handle) {
    if (_uri == NULL || _handle == NULL) {
        return AEE_EBADPARM;
    }
    qurt_mutex_lock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
    for (uint32_t slot = 0U; slot < VQEC_VISION_AI_DSP_V1_SKELETON_MAX_SESSIONS; ++slot) {
        vqec_vision_ai_dsp_v1_skeleton_context* context = &g_vqec_vision_ai_dsp_v1_contexts[slot];
        if (context->active != 0U) {
            continue;
        }
        const uint32_t generation = vqec_vision_ai_qcom_d1skl_next_generation();
        if (vqec_vision_ai_qcom_d1svc_initialize(&context->service, generation) !=
            vqec_vision_ai_dsp_v1_wire_ok) {
            qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
            return AEE_EFAILED;
        }
        (void)vqec_vision_ai_qcom_d1skl_load_image_backend(context);
        context->generation = generation;
        context->active = 1U;
        *_handle = vqec_vision_ai_qcom_d1skl_encode_handle(slot, generation);
        qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
        return AEE_SUCCESS;
    }
    qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
    return AEE_EITEMBUSY;
}

int vqec_vision_dsp_v1_close(remote_handle64 _handle) {
    const uint32_t encoded_slot =
        (uint32_t)(_handle & VQEC_VISION_AI_DSP_V1_SKELETON_HANDLE_SLOT_MASK);
    const uint32_t generation = (uint32_t)(_handle >> 32U);
    if (encoded_slot == 0U || encoded_slot > VQEC_VISION_AI_DSP_V1_SKELETON_MAX_SESSIONS ||
        generation == 0U) {
        return AEE_EBADHANDLE;
    }
    vqec_vision_ai_dsp_v1_skeleton_context* context =
        &g_vqec_vision_ai_dsp_v1_contexts[encoded_slot - 1U];
    qurt_mutex_lock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
    if (context->active == 0U || context->generation != generation) {
        qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
        return AEE_EBADHANDLE;
    }
    qurt_mutex_lock(&g_vqec_vision_ai_dsp_v1_execution_mutexes[encoded_slot - 1U]);
    context->active = 0U;
    vqec_vision_ai_qcom_d1svc_cleanup(&context->service);
    if (context->fastcv_library != NULL) {
        dlclose(context->fastcv_library);
        context->fastcv_library = NULL;
    }
    memset(&context->service, 0, sizeof(context->service));
    qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_execution_mutexes[encoded_slot - 1U]);
    qurt_mutex_unlock(&g_vqec_vision_ai_dsp_v1_pool_mutex);
    return AEE_SUCCESS;
}

AEEResult vqec_vision_dsp_v1_query_capabilities(remote_handle64 _handle, uint8* _response,
                                                int _response_length) {
    if (_response == NULL || _response_length != (int)VQEC_VISION_AI_DSP_V1_ENVELOPE_BYTES) {
        return AEE_EBADPARM;
    }
    vqec_vision_ai_dsp_v1_skeleton_context* context =
        vqec_vision_ai_qcom_d1skl_lock_context(_handle);
    if (context == NULL) {
        return AEE_EBADHANDLE;
    }
    const vqec_vision_ai_dsp_v1_wire_status status = vqec_vision_ai_qcom_d1svc_query_capabilities(
        &context->service, _response, (size_t)_response_length);
    vqec_vision_ai_qcom_d1skl_unlock_context(context);
    return status == vqec_vision_ai_dsp_v1_wire_ok ? AEE_SUCCESS : AEE_EFAILED;
}

AEEResult vqec_vision_dsp_v1_execute(remote_handle64 _handle, const uint8* _descriptor,
                                     int _descriptor_length, const uint8* _input, int _input_length,
                                     uint8* _output, int _output_length, uint8* _response,
                                     int _response_length) {
    if (_descriptor_length < 0 || _input_length < 0 || _output_length < 0 ||
        _response_length != (int)VQEC_VISION_AI_DSP_V1_RESPONSE_BYTES) {
        return AEE_EBADPARM;
    }
    vqec_vision_ai_dsp_v1_skeleton_context* context =
        vqec_vision_ai_qcom_d1skl_lock_context(_handle);
    if (context == NULL) {
        return AEE_EBADHANDLE;
    }
    const vqec_vision_ai_dsp_v1_wire_status status = vqec_vision_ai_qcom_d1svc_execute(
        &context->service, _descriptor, (size_t)_descriptor_length, _input, (size_t)_input_length,
        _output, (size_t)_output_length, _response, (size_t)_response_length);
    vqec_vision_ai_qcom_d1skl_unlock_context(context);
    return status == vqec_vision_ai_dsp_v1_wire_ok ? AEE_SUCCESS : AEE_EBADPARM;
}
