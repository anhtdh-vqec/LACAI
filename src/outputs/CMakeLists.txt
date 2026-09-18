# Output targets. Target names are unchanged from the former root definitions.

add_library(vqec_vision_ai_encoded_dispatch STATIC media/vqec_vision_encoded_dispatch.cpp)
target_include_directories(vqec_vision_ai_encoded_dispatch PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/media>)
target_link_libraries(vqec_vision_ai_encoded_dispatch PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_encoded_dispatch PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_feature_event_dispatch STATIC
    events/vqec_vision_feature_event_dispatch.cpp)
target_include_directories(vqec_vision_ai_feature_event_dispatch PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/events>)
target_link_libraries(vqec_vision_ai_feature_event_dispatch PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_feature_event_dispatch PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_overlay_preparation STATIC
    media/vqec_vision_overlay_preparation.cpp)
target_include_directories(vqec_vision_ai_overlay_preparation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/media>)
target_link_libraries(vqec_vision_ai_overlay_preparation PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_overlay_preparation PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_event_delivery_seam STATIC
    events/vqec_vision_event_delivery_seam.cpp)
target_include_directories(vqec_vision_ai_event_delivery_seam PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/events>)
target_link_libraries(vqec_vision_ai_event_delivery_seam PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_event_delivery_seam PROPERTIES CXX_EXTENSIONS OFF)
