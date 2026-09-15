# Optional AI-owned DBus control adapter. Neutral enrollment contracts remain free of GIO.
if(VQEC_VISION_AI_ENABLE_FACE_ENROLLMENT_DBUS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(face_enrollment_gio REQUIRED IMPORTED_TARGET gio-2.0)
    add_library(vqec_vision_ai_face_enrollment_dbus STATIC
        vqec_vision_face_enrollment_dbus.cpp)
    target_include_directories(vqec_vision_ai_face_enrollment_dbus PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_link_libraries(vqec_vision_ai_face_enrollment_dbus PUBLIC
        vqec_vision_ai_embedding PRIVATE PkgConfig::face_enrollment_gio)
    set_target_properties(vqec_vision_ai_face_enrollment_dbus PROPERTIES CXX_EXTENSIONS OFF)
endif()
