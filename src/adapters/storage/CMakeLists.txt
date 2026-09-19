find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
find_package(SQLite3 REQUIRED)

add_library(vqec_vision_ai_encrypted_gallery_store STATIC
    vqec_vision_encrypted_face_gallery_store.cpp)
target_include_directories(vqec_vision_ai_encrypted_gallery_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_encrypted_gallery_store
    PUBLIC vqec_vision_ai_core PRIVATE OpenSSL::Crypto)
set_target_properties(vqec_vision_ai_encrypted_gallery_store
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_sqlite_metadata_store STATIC
    vqec_vision_sqlite_metadata_store.cpp)
target_include_directories(vqec_vision_ai_sqlite_metadata_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_sqlite_metadata_store
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_sqlite_metadata_store
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_spatiotemporal_store STATIC
    vqec_vision_spatiotemporal_store.cpp
    vqec_vision_spatiotemporal_store_projection.cpp)
target_include_directories(vqec_vision_ai_spatiotemporal_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_spatiotemporal_store
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_spatiotemporal_store
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_sqlite_app_inventory STATIC
    vqec_vision_sqlite_app_inventory.cpp)
target_include_directories(vqec_vision_ai_sqlite_app_inventory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_sqlite_app_inventory
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_sqlite_app_inventory
    PROPERTIES CXX_EXTENSIONS OFF)

if(TARGET vqec_vision_ai_artifact_digest)
    add_library(vqec_vision_ai_app_content_store STATIC
        vqec_vision_app_content_store.cpp)
    target_include_directories(vqec_vision_ai_app_content_store PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_link_libraries(vqec_vision_ai_app_content_store
        PUBLIC vqec_vision_ai_core PRIVATE vqec_vision_ai_artifact_digest)
    set_target_properties(vqec_vision_ai_app_content_store
        PROPERTIES CXX_EXTENSIONS OFF)
endif()

add_library(vqec_vision_ai_sqlite_evidence_outbox STATIC
    vqec_vision_sqlite_evidence_outbox.cpp)
target_include_directories(vqec_vision_ai_sqlite_evidence_outbox PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_sqlite_evidence_outbox
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_sqlite_evidence_outbox
    PROPERTIES CXX_EXTENSIONS OFF)
