# Optional FW shared-memory ring adapter. Target name is unchanged from the former root
# definition. Requires an externally supplied pinned SDK target.

if(VQEC_VISION_AI_ENABLE_FW_RING)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "Released FW ring SDK requires Linux")
    endif()
    if(NOT TARGET ${VQEC_VISION_AI_FW_RING_TARGET})
        message(FATAL_ERROR "Provide the pinned FW SDK target before add_subdirectory(ai_applications)")
    endif()
    add_library(vqec_vision_ai_fw_ring STATIC vqec_vision_ring_sink.cpp)
    target_include_directories(vqec_vision_ai_fw_ring PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_link_libraries(vqec_vision_ai_fw_ring PUBLIC vqec_vision_ai_core
        PRIVATE ${VQEC_VISION_AI_FW_RING_TARGET})
    set_target_properties(vqec_vision_ai_fw_ring PROPERTIES CXX_EXTENSIONS OFF)
endif()
