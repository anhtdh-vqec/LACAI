# Perception targets. Target names are unchanged from the former root definitions.

add_library(vqec_vision_ai_detection STATIC
    detection/vqec_vision_model_decode_stage.cpp
    detection/vqec_vision_model_decoder_registry.cpp
    detection/vqec_vision_tensor_reader.cpp
    detection/vqec_vision_anchor_distance_decoder.cpp
    detection/vqec_vision_dense_decoder.cpp
    detection/vqec_vision_yolov8_decoder.cpp)
target_include_directories(vqec_vision_ai_detection PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/detection>)
target_link_libraries(vqec_vision_ai_detection PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_detection PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_tracking STATIC
    tracking/vqec_vision_tracking_stage.cpp
    tracking/vqec_vision_tracker_registry.cpp)
target_include_directories(vqec_vision_ai_tracking PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tracking>)
target_link_libraries(vqec_vision_ai_tracking PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_tracking PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_attributes STATIC
    attributes/vqec_vision_attribute_reader.cpp)
target_include_directories(vqec_vision_ai_attributes PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/attributes>)
target_link_libraries(vqec_vision_ai_attributes PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_attributes PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_embedding STATIC
    embedding/vqec_vision_exact_embedding_index.cpp)
target_include_directories(vqec_vision_ai_embedding PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/embedding>)
target_link_libraries(vqec_vision_ai_embedding PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_embedding PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_embedding_decoder STATIC
    embedding/vqec_vision_embedding_decoder.cpp)
target_include_directories(vqec_vision_ai_embedding_decoder PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/embedding>)
target_link_libraries(vqec_vision_ai_embedding_decoder
    PUBLIC vqec_vision_ai_core vqec_vision_ai_detection)
set_target_properties(vqec_vision_ai_embedding_decoder PROPERTIES CXX_EXTENSIONS OFF)
