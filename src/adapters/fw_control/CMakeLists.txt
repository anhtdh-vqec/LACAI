# Optional AI-owned DBus control adapter. Neutral enrollment contracts remain free of GIO.
if(UNIX)
    add_library(vqec_vision_ai_image_path_authorizer STATIC
        enrollment/vqec_vision_image_path_authorizer.cpp)
    target_include_directories(vqec_vision_ai_image_path_authorizer PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/enrollment>)
    target_link_libraries(vqec_vision_ai_image_path_authorizer PUBLIC vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_image_path_authorizer PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_FACE_ENROLLMENT_DBUS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(face_enrollment_gio REQUIRED IMPORTED_TARGET gio-2.0)
    add_library(vqec_vision_ai_face_enrollment_dbus STATIC
        enrollment/vqec_vision_face_enrollment_dbus.cpp)
    target_include_directories(vqec_vision_ai_face_enrollment_dbus PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/enrollment>)
    target_link_libraries(vqec_vision_ai_face_enrollment_dbus PUBLIC
        vqec_vision_ai_embedding PRIVATE PkgConfig::face_enrollment_gio)
    set_target_properties(vqec_vision_ai_face_enrollment_dbus PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_USECASE_CONTROL_DBUS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(usecase_control_gio REQUIRED IMPORTED_TARGET gio-2.0)
    add_library(vqec_vision_ai_usecase_control_dbus STATIC
        usecase/vqec_vision_usecase_control_dbus.cpp)
    target_include_directories(vqec_vision_ai_usecase_control_dbus PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/usecase>)
    target_link_libraries(vqec_vision_ai_usecase_control_dbus PUBLIC
        vqec_vision_ai_core PkgConfig::usecase_control_gio)
    set_target_properties(vqec_vision_ai_usecase_control_dbus PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_APP_MANAGER_DBUS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(app_manager_gio REQUIRED IMPORTED_TARGET gio-unix-2.0)
    add_library(vqec_vision_ai_app_manager_dbus STATIC
        app_manager/vqec_vision_app_manager_dbus.cpp)
    target_include_directories(vqec_vision_ai_app_manager_dbus PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/app_manager>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/lifecycle>)
    target_link_libraries(vqec_vision_ai_app_manager_dbus PUBLIC
        vqec_vision_ai_app_manifest PkgConfig::app_manager_gio)
    set_target_properties(vqec_vision_ai_app_manager_dbus PROPERTIES CXX_EXTENSIONS OFF)
endif()
