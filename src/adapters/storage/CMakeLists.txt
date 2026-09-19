find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
find_package(SQLite3 REQUIRED)

add_library(vqec_vision_ai_encrypted_gallery_store STATIC
    identity/vqec_vision_encrypted_face_gallery_store.cpp)
target_include_directories(vqec_vision_ai_encrypted_gallery_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/identity>)
target_link_libraries(vqec_vision_ai_encrypted_gallery_store
    PUBLIC vqec_vision_ai_core PRIVATE OpenSSL::Crypto)
set_target_properties(vqec_vision_ai_encrypted_gallery_store
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_sqlite_metadata_store STATIC
    metadata/vqec_vision_sqlite_metadata_store.cpp)
target_include_directories(vqec_vision_ai_sqlite_metadata_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/metadata>)
target_link_libraries(vqec_vision_ai_sqlite_metadata_store
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_sqlite_metadata_store
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_spatiotemporal_store STATIC
    metadata/vqec_vision_spatiotemporal_store.cpp
    metadata/vqec_vision_spatiotemporal_store_projection.cpp)
target_include_directories(vqec_vision_ai_spatiotemporal_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/metadata>)
target_link_libraries(vqec_vision_ai_spatiotemporal_store
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_spatiotemporal_store
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_sqlite_app_inventory STATIC
    app_lifecycle/vqec_vision_sqlite_app_inventory.cpp)
target_include_directories(vqec_vision_ai_sqlite_app_inventory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/app_lifecycle>)
target_link_libraries(vqec_vision_ai_sqlite_app_inventory
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_sqlite_app_inventory
    PROPERTIES CXX_EXTENSIONS OFF)

if(TARGET vqec_vision_ai_artifact_digest)
    add_library(vqec_vision_ai_app_content_store STATIC
        app_lifecycle/vqec_vision_app_content_store.cpp)
    target_include_directories(vqec_vision_ai_app_content_store PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/app_lifecycle>)
    target_link_libraries(vqec_vision_ai_app_content_store
        PUBLIC vqec_vision_ai_core PRIVATE vqec_vision_ai_artifact_digest)
    set_target_properties(vqec_vision_ai_app_content_store
        PROPERTIES CXX_EXTENSIONS OFF)
endif()

add_library(vqec_vision_ai_sqlite_evidence_outbox STATIC
    evidence/vqec_vision_sqlite_evidence_outbox.cpp)
target_include_directories(vqec_vision_ai_sqlite_evidence_outbox PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/evidence>)
target_link_libraries(vqec_vision_ai_sqlite_evidence_outbox
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_sqlite_evidence_outbox
    PROPERTIES CXX_EXTENSIONS OFF)
