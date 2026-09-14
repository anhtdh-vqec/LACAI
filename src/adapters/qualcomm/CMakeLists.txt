# Qualcomm adapter targets. Target names are unchanged from the former root definitions.

if(VQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE OR VQEC_VISION_AI_ENABLE_QUALCOMM)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "GStreamer DMA-BUF bridge requires Linux")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(frame_gstreamer REQUIRED IMPORTED_TARGET
        gstreamer-1.0 gstreamer-video-1.0 gstreamer-allocators-1.0 gstreamer-app-1.0)
    add_library(vqec_vision_ai_gst_frame_bridge STATIC
        vqec_vision_dmabuf_bridge.cpp vqec_vision_tensor_output.cpp
        vqec_vision_frame_submission.cpp)
    target_include_directories(vqec_vision_ai_gst_frame_bridge PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_link_libraries(vqec_vision_ai_gst_frame_bridge
        PUBLIC vqec_vision_ai_core PkgConfig::frame_gstreamer)
    set_target_properties(vqec_vision_ai_gst_frame_bridge PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_QUALCOMM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)
    add_library(vqec_vision_ai_qualcomm STATIC
        vqec_vision_plugin_graph.cpp
        vqec_vision_inference_graph.cpp)
    target_compile_features(vqec_vision_ai_qualcomm PUBLIC cxx_std_17)
    target_include_directories(vqec_vision_ai_qualcomm PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_link_libraries(vqec_vision_ai_qualcomm
        PUBLIC vqec_vision_ai_core PRIVATE PkgConfig::gstreamer vqec_vision_ai_gst_frame_bridge)
    set_target_properties(vqec_vision_ai_qualcomm PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_QNN_ENGINE)
    if(NOT EXISTS "${VQEC_VISION_AI_QAIRT_ROOT}/include/QNN/QnnInterface.h")
        message(FATAL_ERROR
            "QNN headers not found under VQEC_VISION_AI_QAIRT_ROOT="
            "${VQEC_VISION_AI_QAIRT_ROOT}")
    endif()
    add_library(vqec_vision_ai_qnn_engine STATIC
        vqec_vision_sdk_loader.cpp
        vqec_vision_qnn_engine.cpp
        vqec_vision_qnn_inference_graph.cpp
        vqec_vision_backend_factory.cpp)
    target_include_directories(vqec_vision_ai_qnn_engine PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
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
