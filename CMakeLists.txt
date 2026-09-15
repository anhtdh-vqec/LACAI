cmake_minimum_required(VERSION 3.16)
project(vqec_vision_ai VERSION 0.1.0 LANGUAGES CXX)

option(VQEC_VISION_AI_ENABLE_FW_RING "Enable wrapper for externally supplied FW ring SDK" OFF)
set(VQEC_VISION_AI_FW_RING_TARGET "camera_ai_common" CACHE STRING "Existing version-pinned FW SDK target")

option(VQEC_VISION_AI_ENABLE_QUALCOMM "Enable the private GStreamer Qualcomm adapter" OFF)
option(VQEC_VISION_AI_ENABLE_FASTCV "Build the Qualcomm FastCV image alignment adapter" OFF)
option(VQEC_VISION_AI_ENABLE_QNN_ENGINE "Enable the LACAI-owned QNN engine adapter" OFF)
set(VQEC_VISION_AI_QAIRT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/qairt" CACHE PATH
    "Approved QAIRT SDK root (headers under include/QNN, libraries under lib)")
option(VQEC_VISION_AI_ENABLE_CAMERA "Enable Linux legacy camera FD receiver" OFF)
option(VQEC_VISION_AI_ENABLE_CAMERA_DBUS "Enable Camera Service GIO D-Bus transport" OFF)
option(VQEC_VISION_AI_ENABLE_FACE_ENROLLMENT_DBUS
    "Enable AI-owned face enrollment GIO D-Bus control adapter" OFF)
option(VQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE "Enable Linux FD to GstMemory bridge" OFF)
option(VQEC_VISION_AI_ENABLE_MODEL_MANIFEST "Enable bounded JSON output manifest loading" OFF)
option(VQEC_VISION_AI_ENABLE_MODEL_CATALOG "Enable bounded JSON model catalog loading" OFF)
option(VQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG "Enable bounded JSON deployment configuration loading" OFF)
option(VQEC_VISION_AI_ENABLE_FEATURE_CATALOG "Enable bounded JSON feature catalog loading" OFF)
option(VQEC_VISION_AI_ENABLE_ZVEC "Build the Zvec embedding index backend" ON)
set(VQEC_VISION_AI_ZVEC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zvec/sdk" CACHE PATH
    "External Zvec installation root (contains include/zvec and lib)")
option(VQEC_VISION_AI_BUILD_MANIFEST_CHECK "Build read-only model metadata checker" OFF)
option(VQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST "Enable bounded artifact SHA-256 comparison" OFF)
option(VQEC_VISION_AI_WERROR "Treat project warnings as errors" ON)
option(VQEC_VISION_AI_BUILD_FUZZERS "Build libFuzzer harnesses (host Clang, not eSDK)" OFF)
option(VQEC_VISION_AI_SANITIZE "Build with ASan+UBSan (host toolchain, not eSDK)" OFF)

if(VQEC_VISION_AI_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

if(VQEC_VISION_AI_BUILD_MANIFEST_CHECK AND NOT VQEC_VISION_AI_ENABLE_MODEL_MANIFEST)
    message(FATAL_ERROR "Manifest checker requires VQEC_VISION_AI_ENABLE_MODEL_MANIFEST")
endif()

if(VQEC_VISION_AI_ENABLE_MODEL_MANIFEST OR VQEC_VISION_AI_ENABLE_MODEL_CATALOG OR
   VQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG OR VQEC_VISION_AI_ENABLE_FEATURE_CATALOG)
    # Prefer an installed package; otherwise use the pinned in-tree single header.
    # The eSDK AArch64 sysroot does not currently provide nlohmann_json 3.12.0.
    find_package(nlohmann_json 3.12.0 EXACT CONFIG QUIET)
    if(NOT nlohmann_json_FOUND)
        add_library(vqec_vision_ai_nlohmann_json_vendored INTERFACE)
        # SYSTEM keeps -Werror focused on project code, not the vendored header.
        target_include_directories(vqec_vision_ai_nlohmann_json_vendored SYSTEM INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/include")
        add_library(nlohmann_json::nlohmann_json ALIAS
            vqec_vision_ai_nlohmann_json_vendored)
        message(STATUS
            "nlohmann_json package not found; using vendored 3.12.0 single header")
    endif()
endif()

# Project warning policy, applied to every target in this directory and every subdirectory
# added after this point (module CMake files under src/ and tests/). Centralized so a new
# target cannot silently miss -Wall/-Wextra/-Wpedantic or -Werror. Imported/vendor targets
# are not affected because they are not created in these directories.
add_compile_options("$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/W4>")
if(VQEC_VISION_AI_WERROR)
    add_compile_options("$<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/WX>")
endif()

find_package(Threads REQUIRED)

if(VQEC_VISION_AI_ENABLE_ZVEC)
    if(NOT VQEC_VISION_AI_ZVEC_ROOT)
        message(FATAL_ERROR "VQEC_VISION_AI_ENABLE_ZVEC requires VQEC_VISION_AI_ZVEC_ROOT")
    endif()
    find_path(VQEC_VISION_AI_ZVEC_INCLUDE_DIR zvec/c_api.h
        PATHS "${VQEC_VISION_AI_ZVEC_ROOT}/include"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    find_library(VQEC_VISION_AI_ZVEC_LIBRARY zvec_c_api
        PATHS "${VQEC_VISION_AI_ZVEC_ROOT}/lib"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    if(NOT VQEC_VISION_AI_ZVEC_INCLUDE_DIR OR NOT VQEC_VISION_AI_ZVEC_LIBRARY)
        message(FATAL_ERROR "Zvec C API SDK missing. Run bash tools/vqec_vision_prepare_zvec.sh or supply a reviewed VQEC_VISION_AI_ZVEC_ROOT.")
    endif()
    add_library(vqec_vision_ai_zvec_external INTERFACE)
    target_include_directories(vqec_vision_ai_zvec_external SYSTEM INTERFACE
        "${VQEC_VISION_AI_ZVEC_INCLUDE_DIR}")
    target_link_libraries(vqec_vision_ai_zvec_external INTERFACE
        "${VQEC_VISION_AI_ZVEC_LIBRARY}")
endif()

add_subdirectory(src/core)

add_subdirectory(src/perception)
if(VQEC_VISION_AI_ENABLE_ZVEC)
    add_subdirectory(src/adapters/zvec)
endif()

add_subdirectory(src/runtime)

add_subdirectory(src/adapters/camera)
add_subdirectory(src/adapters/fw_control)

if(VQEC_VISION_AI_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "VQEC_VISION_AI_BUILD_FUZZERS requires a host Clang toolchain with libFuzzer")
    endif()
    add_executable(vqec_vision_ai_legacy_wire_fuzz
        tests/fuzz/vqec_vision_legacy_wire_fuzz.cpp)
    target_link_libraries(vqec_vision_ai_legacy_wire_fuzz
        PRIVATE vqec_vision_ai_camera_wire)
    target_compile_options(vqec_vision_ai_legacy_wire_fuzz
        PRIVATE -g -O1 -fsanitize=fuzzer,address,undefined)
    target_link_options(vqec_vision_ai_legacy_wire_fuzz
        PRIVATE -fsanitize=fuzzer,address,undefined)
    set_target_properties(vqec_vision_ai_legacy_wire_fuzz PROPERTIES CXX_EXTENSIONS OFF)

    add_executable(vqec_vision_ai_dense_decoder_fuzz
        tests/fuzz/vqec_vision_dense_decoder_fuzz.cpp)
    target_link_libraries(vqec_vision_ai_dense_decoder_fuzz
        PRIVATE vqec_vision_ai_detection)
    target_compile_options(vqec_vision_ai_dense_decoder_fuzz
        PRIVATE -g -O1 -fsanitize=fuzzer,address,undefined)
    target_link_options(vqec_vision_ai_dense_decoder_fuzz
        PRIVATE -fsanitize=fuzzer,address,undefined)
    set_target_properties(vqec_vision_ai_dense_decoder_fuzz PROPERTIES CXX_EXTENSIONS OFF)

    if(TARGET vqec_vision_ai_model_catalog_config)
        add_executable(vqec_vision_ai_model_catalog_fuzz
            tests/fuzz/vqec_vision_model_catalog_fuzz.cpp)
        target_link_libraries(vqec_vision_ai_model_catalog_fuzz
            PRIVATE vqec_vision_ai_model_catalog_config)
        target_compile_options(vqec_vision_ai_model_catalog_fuzz
            PRIVATE -g -O1 -fsanitize=fuzzer,address,undefined)
        target_link_options(vqec_vision_ai_model_catalog_fuzz
            PRIVATE -fsanitize=fuzzer,address,undefined)
        set_target_properties(vqec_vision_ai_model_catalog_fuzz
            PROPERTIES CXX_EXTENSIONS OFF)
    endif()

    if(TARGET vqec_vision_ai_model_manifest)
        add_executable(vqec_vision_ai_manifest_fuzz
            tests/fuzz/vqec_vision_manifest_fuzz.cpp)
        target_link_libraries(vqec_vision_ai_manifest_fuzz
            PRIVATE vqec_vision_ai_model_manifest)
        target_compile_options(vqec_vision_ai_manifest_fuzz
            PRIVATE -g -O1 -fsanitize=fuzzer,address,undefined)
        target_link_options(vqec_vision_ai_manifest_fuzz
            PRIVATE -fsanitize=fuzzer,address,undefined)
        set_target_properties(vqec_vision_ai_manifest_fuzz PROPERTIES CXX_EXTENSIONS OFF)
    endif()
endif()

add_subdirectory(src/adapters/qualcomm)

if(UNIX)
    add_subdirectory(src/adapters/reference)
endif()


include(CTest)
add_subdirectory(src/adapters/fw_output)
add_subdirectory(src/outputs)
add_subdirectory(src/app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# No camera service executable, installation/IPK target or implicit board execution yet.
