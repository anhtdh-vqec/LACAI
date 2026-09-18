# Qualcomm adapter targets. Target names are unchanged from the former root definitions.

if(VQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE OR VQEC_VISION_AI_ENABLE_QUALCOMM)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "GStreamer DMA-BUF bridge requires Linux")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(frame_gstreamer REQUIRED IMPORTED_TARGET
        gstreamer-1.0 gstreamer-video-1.0 gstreamer-allocators-1.0 gstreamer-app-1.0)
    add_library(vqec_vision_ai_gst_frame_bridge STATIC
        gstreamer/vqec_vision_dmabuf_bridge.cpp
        gstreamer/vqec_vision_tensor_output.cpp
        gstreamer/vqec_vision_frame_submission.cpp)
    target_include_directories(vqec_vision_ai_gst_frame_bridge PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/gstreamer>)
    target_link_libraries(vqec_vision_ai_gst_frame_bridge
        PUBLIC vqec_vision_ai_core PkgConfig::frame_gstreamer)
    set_target_properties(vqec_vision_ai_gst_frame_bridge PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_QUALCOMM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(gstreamer REQUIRED IMPORTED_TARGET
        gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 gstreamer-allocators-1.0)

    find_library(VQEC_VISION_AI_CDSPRPC_LIBRARY cdsprpc)

    set(VQEC_VISION_AI_HEXAGON_SDK_ROOT "" CACHE PATH
        "Hexagon SDK root used only for QAIC generation of the frozen FastRPC ABI")
    if(NOT EXISTS "${VQEC_VISION_AI_HEXAGON_SDK_ROOT}/incs/stddef/AEEStdDef.idl" OR
       NOT EXISTS "${VQEC_VISION_AI_HEXAGON_SDK_ROOT}/incs/remote.idl")
        message(FATAL_ERROR "Qualcomm FastRPC requires an explicit valid "
            "VQEC_VISION_AI_HEXAGON_SDK_ROOT with QAIC IDL includes")
    endif()
    find_program(VQEC_VISION_AI_QAIC_EXECUTABLE NAMES qaic
        HINTS "${VQEC_VISION_AI_HEXAGON_SDK_ROOT}/ipc/fastrpc/qaic/Ubuntu20"
        NO_DEFAULT_PATH REQUIRED)
    execute_process(COMMAND "${VQEC_VISION_AI_QAIC_EXECUTABLE}" -v
        OUTPUT_VARIABLE vqec_vision_ai_qaic_version
        RESULT_VARIABLE vqec_vision_ai_qaic_result
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT vqec_vision_ai_qaic_result EQUAL 0 OR
       NOT vqec_vision_ai_qaic_version MATCHES "version 01\\.00\\.47")
        message(FATAL_ERROR "FastRPC compatibility stub requires reviewed QAIC 01.00.47")
    endif()

    set(vqec_vision_ai_qaic_output "${CMAKE_CURRENT_BINARY_DIR}/fastrpc_legacy")
    set(vqec_vision_ai_legacy_idl
        "${CMAKE_CURRENT_SOURCE_DIR}/dsp/legacy/vqec_vision_dsp_legacy.idl")
    add_custom_command(
        OUTPUT "${vqec_vision_ai_qaic_output}/vqec_vision_dsp_legacy.h"
               "${vqec_vision_ai_qaic_output}/vqec_dsp_stub.c"
        BYPRODUCTS "${vqec_vision_ai_qaic_output}/vqec_dsp_skel.c"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${vqec_vision_ai_qaic_output}"
        COMMAND "${VQEC_VISION_AI_QAIC_EXECUTABLE}" -mdll
            -o "${vqec_vision_ai_qaic_output}"
            -I "${VQEC_VISION_AI_HEXAGON_SDK_ROOT}/incs/stddef"
            -I "${VQEC_VISION_AI_HEXAGON_SDK_ROOT}/incs"
            "${vqec_vision_ai_legacy_idl}"
        DEPENDS "${vqec_vision_ai_legacy_idl}"
        VERBATIM)
    set(vqec_vision_ai_v1_idl
        "${CMAKE_CURRENT_SOURCE_DIR}/dsp/v1/vqec_vision_dsp_v1.idl")
    set(vqec_vision_ai_v1_qaic_output "${CMAKE_CURRENT_BINARY_DIR}/fastrpc_v1")
    add_custom_command(
        OUTPUT "${vqec_vision_ai_v1_qaic_output}/vqec_vision_dsp_v1.h"
               "${vqec_vision_ai_v1_qaic_output}/vqec_vision_dsp_v1_stub.c"
               "${vqec_vision_ai_v1_qaic_output}/vqec_vision_dsp_v1_skel.c"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${vqec_vision_ai_v1_qaic_output}"
        COMMAND "${VQEC_VISION_AI_QAIC_EXECUTABLE}" -mdll
            -o "${vqec_vision_ai_v1_qaic_output}"
            -I "${VQEC_VISION_AI_HEXAGON_SDK_ROOT}/incs/stddef"
            -I "${VQEC_VISION_AI_HEXAGON_SDK_ROOT}/incs"
            "${vqec_vision_ai_v1_idl}"
        DEPENDS "${vqec_vision_ai_v1_idl}"
        VERBATIM)
    add_custom_target(vqec_vision_ai_dsp_v1_idl_check ALL
        DEPENDS "${vqec_vision_ai_v1_qaic_output}/vqec_vision_dsp_v1.h"
                "${vqec_vision_ai_v1_qaic_output}/vqec_vision_dsp_v1_stub.c"
                "${vqec_vision_ai_v1_qaic_output}/vqec_vision_dsp_v1_skel.c")

    add_library(vqec_vision_ai_fastrpc_c STATIC
        dsp/legacy/vqec_vision_dsp_legacy_post_common.c
        dsp/legacy/vqec_vision_dsp_legacy_post_person.c
        dsp/legacy/vqec_vision_dsp_legacy_post_face.c
        dsp/legacy/vqec_vision_dsp_legacy_pre.c
        "${vqec_vision_ai_qaic_output}/vqec_dsp_stub.c")
    add_library(vqec_vision_ai_dsp_v1_wire STATIC
        dsp/v1/vqec_vision_dsp_v1_wire.c)
    target_include_directories(vqec_vision_ai_dsp_v1_wire PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/dsp/v1" "${CMAKE_SOURCE_DIR}/include")
    add_library(vqec_vision_ai_dsp_v1_dense STATIC
        dsp/v1/vqec_vision_dsp_v1_dense.c)
    target_include_directories(vqec_vision_ai_dsp_v1_dense PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/dsp/v1" "${CMAKE_SOURCE_DIR}/include")
    target_link_libraries(vqec_vision_ai_dsp_v1_dense
        PUBLIC vqec_vision_ai_dsp_v1_wire PRIVATE m)
    add_library(vqec_vision_ai_dsp_v1_service STATIC
        dsp/v1/vqec_vision_dsp_v1_service.c)
    target_include_directories(vqec_vision_ai_dsp_v1_service PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/dsp/v1" "${CMAKE_SOURCE_DIR}/include")
    target_link_libraries(vqec_vision_ai_dsp_v1_service
        PUBLIC vqec_vision_ai_dsp_v1_dense)
    target_include_directories(vqec_vision_ai_fastrpc_c PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/dsp/legacy" "${vqec_vision_ai_qaic_output}")
    set_source_files_properties("${vqec_vision_ai_qaic_output}/vqec_dsp_stub.c"
        PROPERTIES COMPILE_OPTIONS "-w")
    if(VQEC_VISION_AI_CDSPRPC_LIBRARY)
        target_link_libraries(vqec_vision_ai_fastrpc_c PRIVATE ${VQEC_VISION_AI_CDSPRPC_LIBRARY})
    endif()

    add_library(vqec_vision_ai_qualcomm STATIC
        gstreamer/vqec_vision_fastcv_processor.cpp
        media/vqec_vision_qtiv_color.cpp
        gstreamer/vqec_vision_plugin_graph.cpp
        gstreamer/vqec_vision_inference_graph.cpp
        media/vqec_vision_face_enrollment_image_source.cpp
        dsp/host/vqec_vision_dsp_session.cpp
        dsp/host/vqec_vision_rpcmem_pool.cpp
        dsp/host/vqec_vision_dsp_decoder.cpp
        dsp/host/vqec_vision_dsp_buffer_cache.cpp
        dsp/host/vqec_vision_dsp_preprocessor.cpp)
    target_compile_features(vqec_vision_ai_qualcomm PUBLIC cxx_std_17)
    target_include_directories(vqec_vision_ai_qualcomm PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/gstreamer>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/media>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dsp/host>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dsp/legacy>
        "${vqec_vision_ai_qaic_output}")
    target_link_libraries(vqec_vision_ai_qualcomm
        PUBLIC vqec_vision_ai_core vqec_vision_ai_fastrpc_c
        PRIVATE PkgConfig::gstreamer vqec_vision_ai_gst_frame_bridge)
    if(VQEC_VISION_AI_CDSPRPC_LIBRARY)
        target_link_libraries(vqec_vision_ai_qualcomm PRIVATE ${VQEC_VISION_AI_CDSPRPC_LIBRARY})
    endif()

    foreach(candidate
        "/home/a/Workspace/eSDK/tmp/sysroots-components/armv8-2a/dspservices-headers/usr/include"
        "$ENV{OECORE_TARGET_SYSROOT}/../sysroots-components/armv8-2a/dspservices-headers/usr/include"
        "$ENV{OECORE_TARGET_SYSROOT}/usr/include"
        "${CMAKE_SYSROOT}/usr/include")
        if(EXISTS "${candidate}/remote.h")
            target_include_directories(vqec_vision_ai_fastrpc_c PUBLIC "${candidate}")
            target_include_directories(vqec_vision_ai_qualcomm PUBLIC "${candidate}")
            target_compile_definitions(vqec_vision_ai_qualcomm PUBLIC VQEC_VISION_AI_HAVE_CDSP=1)
            break()
        endif()
    endforeach()
    set_target_properties(vqec_vision_ai_qualcomm PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_QNN_ENGINE)
    if(NOT EXISTS "${VQEC_VISION_AI_QAIRT_ROOT}/include/QNN/QnnInterface.h")
        message(FATAL_ERROR
            "QNN headers not found under VQEC_VISION_AI_QAIRT_ROOT="
            "${VQEC_VISION_AI_QAIRT_ROOT}")
    endif()
    add_library(vqec_vision_ai_qnn_engine STATIC
        qnn/vqec_vision_sdk_loader.cpp
        qnn/vqec_vision_qnn_engine.cpp
        qnn/vqec_vision_qnn_inference_graph.cpp
        qnn/vqec_vision_backend_factory.cpp)
    target_include_directories(vqec_vision_ai_qnn_engine PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qnn>)
    target_include_directories(vqec_vision_ai_qnn_engine PRIVATE
        "${VQEC_VISION_AI_QAIRT_ROOT}/include/QNN")
    target_link_libraries(vqec_vision_ai_qnn_engine
        PUBLIC vqec_vision_ai_core PRIVATE ${CMAKE_DL_LIBS})
    set_target_properties(vqec_vision_ai_qnn_engine PROPERTIES CXX_EXTENSIONS OFF)

    # Board-only smoke; not registered as a test and never run by the host/QEMU matrix.
    add_executable(vqec_vision_ai_qnn_engine_smoke
        ${CMAKE_SOURCE_DIR}/tools/vqec_vision_qnn_engine_smoke.cpp)
    target_link_libraries(vqec_vision_ai_qnn_engine_smoke
        PRIVATE vqec_vision_ai_qnn_engine)
    set_target_properties(vqec_vision_ai_qnn_engine_smoke PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Board-only smoke for the QtIV color-conversion offload. Not registered as a test.
if(VQEC_VISION_AI_ENABLE_QUALCOMM)
    add_executable(vqec_vision_ai_qtiv_color_smoke
        ${CMAKE_SOURCE_DIR}/tools/vqec_vision_qtiv_color_smoke.cpp)
    target_link_libraries(vqec_vision_ai_qtiv_color_smoke
        PRIVATE vqec_vision_ai_qualcomm)
    set_target_properties(vqec_vision_ai_qtiv_color_smoke PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Owned FastCV path. Currently a board-only affine smoke to establish the vendor warp
# convention; the alignment adapter and its runtime wiring follow once that is verified.
if(VQEC_VISION_AI_ENABLE_FASTCV)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "FastCV requires Linux")
    endif()
    find_path(VQEC_VISION_AI_FASTCV_INCLUDE_DIR fastcv/fastcv.h)
    find_library(VQEC_VISION_AI_FASTCV_LIBRARY fastcvopt)
    if(NOT VQEC_VISION_AI_FASTCV_INCLUDE_DIR OR NOT VQEC_VISION_AI_FASTCV_LIBRARY)
        message(FATAL_ERROR
            "FastCV headers/library not found in the toolchain sysroot; supply the "
            "qcom-fastcv-binaries sysroot")
    endif()
    add_executable(vqec_vision_ai_fastcv_affine_smoke
        ${CMAKE_SOURCE_DIR}/tools/vqec_vision_fastcv_affine_smoke.cpp)
    target_include_directories(vqec_vision_ai_fastcv_affine_smoke PRIVATE
        "${VQEC_VISION_AI_FASTCV_INCLUDE_DIR}")
    target_link_libraries(vqec_vision_ai_fastcv_affine_smoke PRIVATE
        "${VQEC_VISION_AI_FASTCV_LIBRARY}")

    add_library(vqec_vision_ai_fastcv_aligner STATIC
        media/vqec_vision_fastcv_aligner.cpp)
    target_include_directories(vqec_vision_ai_fastcv_aligner PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/media>)
    target_include_directories(vqec_vision_ai_fastcv_aligner PRIVATE
        "${VQEC_VISION_AI_FASTCV_INCLUDE_DIR}")
    target_link_libraries(vqec_vision_ai_fastcv_aligner
        PUBLIC vqec_vision_ai_core vqec_vision_ai_qualcomm
        PRIVATE "${VQEC_VISION_AI_FASTCV_LIBRARY}")
    set_target_properties(vqec_vision_ai_fastcv_aligner PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(vqec_vision_ai_fastcv_affine_smoke
        PRIVATE vqec_vision_ai_fastcv_aligner)
    set_target_properties(vqec_vision_ai_fastcv_affine_smoke PROPERTIES CXX_EXTENSIONS OFF)
endif()

# AI-owned encoded output adapter: qtivoverlay (hardware) + v4l2h264enc -> FW ring.
if(VQEC_VISION_AI_ENABLE_QUALCOMM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(qtiv_renderer_gst REQUIRED IMPORTED_TARGET
        gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0
        gstreamer-qcom-oss-allocators-1.0 gstreamer-qcom-oss-video-1.0)
    add_library(vqec_vision_ai_qtiv_renderer STATIC
        media/vqec_vision_qtiv_renderer.cpp)
    target_include_directories(vqec_vision_ai_qtiv_renderer PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/media>)
    target_include_directories(vqec_vision_ai_qtiv_renderer PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dsp/host>)
    target_link_libraries(vqec_vision_ai_qtiv_renderer
        PUBLIC vqec_vision_ai_core PRIVATE PkgConfig::qtiv_renderer_gst)
    set_target_properties(vqec_vision_ai_qtiv_renderer PROPERTIES CXX_EXTENSIONS OFF)
endif()
