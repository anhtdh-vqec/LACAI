# Device-free reference adapters. Target names are unchanged from the former root
# definitions; only the build definition moved next to the module.

add_library(vqec_vision_ai_reference STATIC
    vqec_vision_reference_source.cpp
    vqec_vision_reference_graph.cpp
    vqec_vision_reference_sink.cpp
    vqec_vision_reference_processor.cpp
    vqec_vision_reference_tracker.cpp
    vqec_vision_reference_feature.cpp
    vqec_vision_reference_encoder.cpp
    vqec_vision_reference_ring_sink.cpp)
target_include_directories(vqec_vision_ai_reference PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_reference PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_reference PROPERTIES CXX_EXTENSIONS OFF)
