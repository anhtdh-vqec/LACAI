add_library(vqec_vision_ai_fire_smoke_feature STATIC
    fire_smoke/vqec_vision_fire_smoke_alarm.cpp
    fire_smoke/vqec_vision_fire_smoke_factory.cpp)
target_include_directories(vqec_vision_ai_fire_smoke_feature PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fire_smoke>)
target_link_libraries(vqec_vision_ai_fire_smoke_feature
    PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(vqec_vision_ai_fire_smoke_feature PROPERTIES CXX_EXTENSIONS OFF)

