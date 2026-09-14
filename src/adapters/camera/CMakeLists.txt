# Camera adapter targets. Target names are unchanged from the former root definitions.

add_library(vqec_vision_ai_camera_wire STATIC vqec_vision_legacy_wire.cpp)
target_link_libraries(vqec_vision_ai_camera_wire PUBLIC vqec_vision_ai_core)
target_include_directories(vqec_vision_ai_camera_wire PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_target_properties(vqec_vision_ai_camera_wire PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_camera_control STATIC vqec_vision_camera_control.cpp)
target_link_libraries(vqec_vision_ai_camera_control PUBLIC vqec_vision_ai_core)
target_include_directories(vqec_vision_ai_camera_control PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_target_properties(vqec_vision_ai_camera_control PROPERTIES CXX_EXTENSIONS OFF)

if(VQEC_VISION_AI_ENABLE_CAMERA_DBUS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(camera_gio REQUIRED IMPORTED_TARGET gio-2.0)
    add_library(vqec_vision_ai_camera_dbus STATIC vqec_vision_dbus_rpc.cpp)
    target_link_libraries(vqec_vision_ai_camera_dbus
        PUBLIC vqec_vision_ai_camera_control PRIVATE PkgConfig::camera_gio)
    target_include_directories(vqec_vision_ai_camera_dbus PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    set_target_properties(vqec_vision_ai_camera_dbus PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_CAMERA)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "Camera FD receiver requires Linux")
    endif()
    add_library(vqec_vision_ai_camera STATIC
        vqec_vision_frame_source.cpp
        vqec_vision_source_lifecycle.cpp
        vqec_vision_raw_source_resolver.cpp)
    target_link_libraries(vqec_vision_ai_camera
        PUBLIC vqec_vision_ai_camera_wire vqec_vision_ai_camera_control)
    target_include_directories(vqec_vision_ai_camera PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_compile_definitions(vqec_vision_ai_camera PRIVATE _GNU_SOURCE)
    set_target_properties(vqec_vision_ai_camera PROPERTIES CXX_EXTENSIONS OFF)
endif()
