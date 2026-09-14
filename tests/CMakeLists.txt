# Test targets. Moved out of the root CMake file; target names and test names are
# unchanged. Evaluated only when BUILD_TESTING is enabled by the root file.

    add_executable(vqec_vision_ai_feature_processor_registry_test
        contract/vqec_vision_feature_processor_registry_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_processor_registry_test
        PRIVATE vqec_vision_ai_feature_runtime)
    set_target_properties(vqec_vision_ai_feature_processor_registry_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_processor_registry_contract
        COMMAND vqec_vision_ai_feature_processor_registry_test)
    add_executable(vqec_vision_ai_feature_activation_manager_test
        contract/vqec_vision_feature_activation_manager_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_activation_manager_test
        PRIVATE vqec_vision_ai_feature_runtime)
    set_target_properties(vqec_vision_ai_feature_activation_manager_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_activation_manager_contract
        COMMAND vqec_vision_ai_feature_activation_manager_test)
    add_executable(vqec_vision_ai_tracker_registry_test
        contract/vqec_vision_tracker_registry_test.cpp)
    target_link_libraries(vqec_vision_ai_tracker_registry_test
        PRIVATE vqec_vision_ai_tracking)
    set_target_properties(vqec_vision_ai_tracker_registry_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tracker_registry_contract
        COMMAND vqec_vision_ai_tracker_registry_test)
    if(TARGET vqec_vision_ai_feature_catalog_config)
        add_executable(vqec_vision_ai_feature_catalog_loader_test
            unit/vqec_vision_feature_catalog_loader_test.cpp)
        target_link_libraries(vqec_vision_ai_feature_catalog_loader_test
            PRIVATE vqec_vision_ai_feature_catalog_config)
        set_target_properties(vqec_vision_ai_feature_catalog_loader_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME feature_catalog_loading
            COMMAND vqec_vision_ai_feature_catalog_loader_test)
    endif()
    add_executable(vqec_vision_ai_feature_catalog_test
        unit/vqec_vision_feature_catalog_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_catalog_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_feature_catalog_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_catalog_contract COMMAND vqec_vision_ai_feature_catalog_test)
    add_executable(vqec_vision_ai_multi_model_feature_pipeline_test
        contract/vqec_vision_multi_model_feature_pipeline_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_feature_pipeline_test
        PRIVATE vqec_vision_ai_multi_model_feature_pipeline)
    set_target_properties(vqec_vision_ai_multi_model_feature_pipeline_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_feature_pipeline
        COMMAND vqec_vision_ai_multi_model_feature_pipeline_test)
    add_executable(vqec_vision_ai_multi_model_result_router_test
        contract/vqec_vision_multi_model_result_router_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_result_router_test
        PRIVATE vqec_vision_ai_multi_model_result_router)
    set_target_properties(vqec_vision_ai_multi_model_result_router_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_result_routing
        COMMAND vqec_vision_ai_multi_model_result_router_test)
    add_executable(vqec_vision_ai_source_perception_factory_test
        contract/vqec_vision_source_perception_factory_test.cpp)
    target_link_libraries(vqec_vision_ai_source_perception_factory_test
        PRIVATE vqec_vision_ai_multi_model_result_router)
    set_target_properties(vqec_vision_ai_source_perception_factory_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME source_perception_factory_contract
        COMMAND vqec_vision_ai_source_perception_factory_test)
    add_executable(vqec_vision_ai_runtime_composition_factory_test
        contract/vqec_vision_runtime_composition_factory_test.cpp)
    target_link_libraries(vqec_vision_ai_runtime_composition_factory_test
        PRIVATE vqec_vision_ai_runtime_composition_factory)
    set_target_properties(vqec_vision_ai_runtime_composition_factory_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME runtime_composition_factory_contract
        COMMAND vqec_vision_ai_runtime_composition_factory_test)
    add_executable(vqec_vision_ai_feature_fanout_test
        contract/vqec_vision_feature_fanout_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_fanout_test
        PRIVATE vqec_vision_ai_feature_fanout)
    set_target_properties(vqec_vision_ai_feature_fanout_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_fanout_isolation COMMAND vqec_vision_ai_feature_fanout_test)
    add_executable(vqec_vision_ai_perception_result_stage_test
        contract/vqec_vision_perception_result_stage_test.cpp)
    target_link_libraries(vqec_vision_ai_perception_result_stage_test
        PRIVATE vqec_vision_ai_perception_result_stage)
    set_target_properties(vqec_vision_ai_perception_result_stage_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME perception_result_correlation
        COMMAND vqec_vision_ai_perception_result_stage_test)
    add_executable(vqec_vision_ai_perception_stage_factory_test
        contract/vqec_vision_perception_stage_factory_test.cpp)
    target_link_libraries(vqec_vision_ai_perception_stage_factory_test
        PRIVATE vqec_vision_ai_perception_result_stage)
    set_target_properties(vqec_vision_ai_perception_stage_factory_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME perception_stage_factory_contract
        COMMAND vqec_vision_ai_perception_stage_factory_test)
    add_executable(vqec_vision_ai_feature_event_dispatch_test
        contract/vqec_vision_feature_event_dispatch_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_event_dispatch_test
        PRIVATE vqec_vision_ai_feature_event_dispatch)
    target_include_directories(vqec_vision_ai_feature_event_dispatch_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/outputs>)
    set_target_properties(vqec_vision_ai_feature_event_dispatch_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_event_dispatch_authorization
        COMMAND vqec_vision_ai_feature_event_dispatch_test)
    add_executable(vqec_vision_ai_feature_stage_test
        contract/vqec_vision_feature_stage_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_stage_test
        PRIVATE vqec_vision_ai_feature_runtime)
    target_include_directories(vqec_vision_ai_feature_stage_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/feature_manager>)
    set_target_properties(vqec_vision_ai_feature_stage_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_stage_contract COMMAND vqec_vision_ai_feature_stage_test)
    add_executable(vqec_vision_ai_feature_processor_test
        contract/vqec_vision_feature_processor_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_processor_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_feature_processor_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_processor_contract COMMAND vqec_vision_ai_feature_processor_test)
    add_executable(vqec_vision_ai_feature_event_test
        unit/vqec_vision_feature_event_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_event_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_feature_event_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_event_contract COMMAND vqec_vision_ai_feature_event_test)
    add_executable(vqec_vision_ai_attribute_reader_test
        contract/vqec_vision_attribute_reader_test.cpp)
    target_link_libraries(vqec_vision_ai_attribute_reader_test
        PRIVATE vqec_vision_ai_attributes)
    target_include_directories(vqec_vision_ai_attribute_reader_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/attributes>)
    set_target_properties(vqec_vision_ai_attribute_reader_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME attribute_reader_contract COMMAND vqec_vision_ai_attribute_reader_test)
    add_executable(vqec_vision_ai_tracking_stage_test
        contract/vqec_vision_tracking_stage_test.cpp)
    target_link_libraries(vqec_vision_ai_tracking_stage_test PRIVATE vqec_vision_ai_tracking)
    target_include_directories(vqec_vision_ai_tracking_stage_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/tracking>)
    set_target_properties(vqec_vision_ai_tracking_stage_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tracking_stage_contract COMMAND vqec_vision_ai_tracking_stage_test)
    add_executable(vqec_vision_ai_tracker_port_test
        contract/vqec_vision_tracker_port_test.cpp)
    target_link_libraries(vqec_vision_ai_tracker_port_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_tracker_port_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tracker_port_contract COMMAND vqec_vision_ai_tracker_port_test)
    add_executable(vqec_vision_ai_tensor_reader_test
        contract/vqec_vision_tensor_reader_test.cpp)
    target_link_libraries(vqec_vision_ai_tensor_reader_test PRIVATE vqec_vision_ai_detection)
    target_include_directories(vqec_vision_ai_tensor_reader_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/detection>)
    set_target_properties(vqec_vision_ai_tensor_reader_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tensor_reader_contract COMMAND vqec_vision_ai_tensor_reader_test)
    add_executable(vqec_vision_ai_overlay_preparation_test
        contract/vqec_vision_overlay_preparation_test.cpp)
    target_link_libraries(vqec_vision_ai_overlay_preparation_test
        PRIVATE vqec_vision_ai_overlay_preparation)
    target_include_directories(vqec_vision_ai_overlay_preparation_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/outputs>)
    set_target_properties(vqec_vision_ai_overlay_preparation_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME overlay_preparation_authorization
        COMMAND vqec_vision_ai_overlay_preparation_test)
    add_executable(vqec_vision_ai_model_decoder_registry_test
        contract/vqec_vision_model_decoder_registry_test.cpp)
    target_link_libraries(vqec_vision_ai_model_decoder_registry_test
        PRIVATE vqec_vision_ai_detection)
    target_include_directories(vqec_vision_ai_model_decoder_registry_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/detection>)
    set_target_properties(vqec_vision_ai_model_decoder_registry_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_decoder_registry_contract
        COMMAND vqec_vision_ai_model_decoder_registry_test)
    add_executable(vqec_vision_ai_model_decode_stage_test
        contract/vqec_vision_model_decode_stage_test.cpp)
    target_link_libraries(vqec_vision_ai_model_decode_stage_test
        PRIVATE vqec_vision_ai_detection)
    target_include_directories(vqec_vision_ai_model_decode_stage_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/detection>)
    set_target_properties(vqec_vision_ai_model_decode_stage_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_decode_stage_contract COMMAND vqec_vision_ai_model_decode_stage_test)
    add_executable(vqec_vision_ai_model_decoder_test
        contract/vqec_vision_model_decoder_test.cpp)
    target_link_libraries(vqec_vision_ai_model_decoder_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_model_decoder_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_decoder_contract COMMAND vqec_vision_ai_model_decoder_test)
    add_executable(vqec_vision_ai_application_composition_test
        contract/vqec_vision_application_composition_test.cpp)
    target_link_libraries(vqec_vision_ai_application_composition_test
        PRIVATE vqec_vision_ai_application_composition)
    target_include_directories(vqec_vision_ai_application_composition_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/app>)
    set_target_properties(vqec_vision_ai_application_composition_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME application_composition_lifecycle
        COMMAND vqec_vision_ai_application_composition_test)
    add_executable(vqec_vision_ai_model_cadence_test
        unit/vqec_vision_model_cadence_test.cpp)
    target_link_libraries(vqec_vision_ai_model_cadence_test
        PRIVATE vqec_vision_ai_scheduler)
    set_target_properties(vqec_vision_ai_model_cadence_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_cadence_selection COMMAND vqec_vision_ai_model_cadence_test)
    add_executable(vqec_vision_ai_multi_model_pump_test
        unit/vqec_vision_multi_model_pump_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_pump_test
        PRIVATE vqec_vision_ai_multi_model_pump)
    set_target_properties(vqec_vision_ai_multi_model_pump_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_frame_fanout COMMAND vqec_vision_ai_multi_model_pump_test)
    add_executable(vqec_vision_ai_multi_model_pump_tensor_test
        unit/vqec_vision_multi_model_pump_tensor_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_pump_tensor_test
        PRIVATE vqec_vision_ai_multi_model_pump)
    set_target_properties(vqec_vision_ai_multi_model_pump_tensor_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_pump_tensor_path
        COMMAND vqec_vision_ai_multi_model_pump_tensor_test)
    add_executable(vqec_vision_ai_multi_model_session_test
        unit/vqec_vision_multi_model_session_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_session_test
        PRIVATE vqec_vision_ai_multi_model_session)
    set_target_properties(vqec_vision_ai_multi_model_session_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_source_lifecycle
        COMMAND vqec_vision_ai_multi_model_session_test)
    add_executable(vqec_vision_ai_multi_source_supervisor_test
        unit/vqec_vision_multi_source_supervisor_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_source_supervisor_test
        PRIVATE vqec_vision_ai_source_supervisor)
    set_target_properties(vqec_vision_ai_multi_source_supervisor_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_source_supervision
        COMMAND vqec_vision_ai_multi_source_supervisor_test)
    add_executable(vqec_vision_ai_multi_source_supervisor_async_test
        unit/vqec_vision_multi_source_supervisor_async_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_source_supervisor_async_test
        PRIVATE vqec_vision_ai_source_supervisor Threads::Threads)
    set_target_properties(vqec_vision_ai_multi_source_supervisor_async_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_source_supervision_async
        COMMAND vqec_vision_ai_multi_source_supervisor_async_test)
    add_executable(vqec_vision_ai_activation_snapshot_test
        unit/vqec_vision_activation_snapshot_test.cpp)
    target_link_libraries(vqec_vision_ai_activation_snapshot_test
        PRIVATE vqec_vision_ai_admission)
    set_target_properties(vqec_vision_ai_activation_snapshot_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME activation_snapshot_validation COMMAND vqec_vision_ai_activation_snapshot_test)
    add_executable(vqec_vision_ai_model_catalog_test
        unit/vqec_vision_model_catalog_test.cpp)
    target_link_libraries(vqec_vision_ai_model_catalog_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_model_catalog_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_catalog_validation COMMAND vqec_vision_ai_model_catalog_test)
    if(TARGET vqec_vision_ai_model_catalog_config)
        add_executable(vqec_vision_ai_model_catalog_loader_test
            unit/vqec_vision_model_catalog_loader_test.cpp)
        target_link_libraries(vqec_vision_ai_model_catalog_loader_test
            PRIVATE vqec_vision_ai_model_catalog_config)
        set_target_properties(vqec_vision_ai_model_catalog_loader_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME model_catalog_loading COMMAND vqec_vision_ai_model_catalog_loader_test)
    endif()
    add_executable(vqec_vision_ai_deployment_config_test
        unit/vqec_vision_deployment_config_test.cpp)
    target_link_libraries(vqec_vision_ai_deployment_config_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_deployment_config_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME deployment_config_validation COMMAND vqec_vision_ai_deployment_config_test)
    if(TARGET vqec_vision_ai_deployment_config)
        add_executable(vqec_vision_ai_deployment_loader_test
            unit/vqec_vision_deployment_loader_test.cpp)
        target_link_libraries(vqec_vision_ai_deployment_loader_test
            PRIVATE vqec_vision_ai_deployment_config)
        set_target_properties(vqec_vision_ai_deployment_loader_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME deployment_config_loading COMMAND vqec_vision_ai_deployment_loader_test)
    endif()
    add_executable(vqec_vision_ai_observation_test unit/vqec_vision_observation_test.cpp)
    target_link_libraries(vqec_vision_ai_observation_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_observation_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME observation_contract COMMAND vqec_vision_ai_observation_test)
    add_executable(vqec_vision_ai_encoder_contract_test
        unit/vqec_vision_encoder_contract_test.cpp)
    target_link_libraries(vqec_vision_ai_encoder_contract_test PRIVATE vqec_vision_ai_core)
    add_test(NAME encoder_input_validation COMMAND vqec_vision_ai_encoder_contract_test)
    add_executable(vqec_vision_ai_output_generation_test
        unit/vqec_vision_output_generation_test.cpp)
    target_link_libraries(vqec_vision_ai_output_generation_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_output_generation_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME output_generation_identity COMMAND vqec_vision_ai_output_generation_test)
    if(TARGET vqec_vision_ai_fw_ring)
        add_executable(vqec_vision_ai_ring_sink_test contract/vqec_vision_ring_sink_test.cpp)
        target_link_libraries(vqec_vision_ai_ring_sink_test PRIVATE vqec_vision_ai_fw_ring
            ${VQEC_VISION_AI_FW_RING_TARGET})
        set_target_properties(vqec_vision_ai_ring_sink_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME fw_ring_sink_guards COMMAND vqec_vision_ai_ring_sink_test)
    endif()
    add_executable(vqec_vision_ai_encoded_dispatch_test contract/vqec_vision_encoded_dispatch_test.cpp)
    target_link_libraries(vqec_vision_ai_encoded_dispatch_test PRIVATE vqec_vision_ai_encoded_dispatch)
    set_target_properties(vqec_vision_ai_encoded_dispatch_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoded_dispatch_authorization COMMAND vqec_vision_ai_encoded_dispatch_test)
    add_executable(vqec_vision_ai_encoded_output_test unit/vqec_vision_encoded_output_test.cpp)
    target_link_libraries(vqec_vision_ai_encoded_output_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_encoded_output_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoded_output_ownership COMMAND vqec_vision_ai_encoded_output_test)
    add_executable(vqec_vision_ai_encoder_preparation_test
        contract/vqec_vision_encoder_preparation_test.cpp)
    target_link_libraries(vqec_vision_ai_encoder_preparation_test PRIVATE vqec_vision_ai_encoder_preparation)
    set_target_properties(vqec_vision_ai_encoder_preparation_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoder_preparation_contract COMMAND vqec_vision_ai_encoder_preparation_test)
    add_executable(vqec_vision_ai_preview_pool_test unit/vqec_vision_preview_pool_test.cpp)
    target_link_libraries(vqec_vision_ai_preview_pool_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_preview_pool_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME preview_pool_ownership COMMAND vqec_vision_ai_preview_pool_test)
    add_executable(vqec_vision_ai_encoder_window_test unit/vqec_vision_encoder_window_test.cpp)
    target_link_libraries(vqec_vision_ai_encoder_window_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_encoder_window_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoder_window_lifecycle COMMAND vqec_vision_ai_encoder_window_test)
    add_executable(vqec_vision_ai_preview_surface_test unit/vqec_vision_preview_surface_test.cpp)
    target_link_libraries(vqec_vision_ai_preview_surface_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_preview_surface_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME preview_surface_ownership COMMAND vqec_vision_ai_preview_surface_test)
    add_executable(vqec_vision_ai_preview_contract_test unit/vqec_vision_preview_contract_test.cpp)
    target_link_libraries(vqec_vision_ai_preview_contract_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_preview_contract_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME preview_contract_validation COMMAND vqec_vision_ai_preview_contract_test)
    add_executable(vqec_vision_ai_output_gate_test unit/vqec_vision_output_gate_test.cpp)
    target_link_libraries(vqec_vision_ai_output_gate_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_output_gate_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME output_gate_authorization COMMAND vqec_vision_ai_output_gate_test)
    if(TARGET vqec_vision_ai_artifact_digest)
        add_executable(vqec_vision_ai_artifact_digest_test unit/vqec_vision_artifact_digest_test.cpp)
        target_link_libraries(vqec_vision_ai_artifact_digest_test
            PRIVATE vqec_vision_ai_artifact_digest)
        set_target_properties(vqec_vision_ai_artifact_digest_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME artifact_digest_validation COMMAND vqec_vision_ai_artifact_digest_test)
        if(UNIX)
            add_executable(vqec_vision_ai_artifact_resolver_test
                contract/vqec_vision_artifact_resolver_test.cpp)
            target_link_libraries(vqec_vision_ai_artifact_resolver_test
                PRIVATE vqec_vision_ai_artifact_digest)
            set_target_properties(vqec_vision_ai_artifact_resolver_test
                PROPERTIES CXX_EXTENSIONS OFF)
            add_test(NAME artifact_resolver_containment
                COMMAND vqec_vision_ai_artifact_resolver_test)
        endif()
    endif()
    if(TARGET vqec_vision_ai_model_manifest)
        add_executable(vqec_vision_ai_output_manifest_test unit/vqec_vision_output_manifest_test.cpp)
        target_link_libraries(vqec_vision_ai_output_manifest_test PRIVATE vqec_vision_ai_model_manifest)
        set_target_properties(vqec_vision_ai_output_manifest_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME output_manifest_validation COMMAND vqec_vision_ai_output_manifest_test)
        if(TARGET vqec_vision_ai_manifest_check)
            add_test(NAME output_manifest_example COMMAND vqec_vision_ai_manifest_check
                ${CMAKE_SOURCE_DIR}/manifests/models/output_fixture.json)
        endif()
    endif()
    add_executable(vqec_vision_ai_inference_execution_test
        unit/vqec_vision_inference_execution_test.cpp)
    target_link_libraries(vqec_vision_ai_inference_execution_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_inference_execution_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME inference_execution_validation
        COMMAND vqec_vision_ai_inference_execution_test)
    add_executable(vqec_vision_ai_inference_worker_test
        unit/vqec_vision_inference_worker_test.cpp)
    target_link_libraries(vqec_vision_ai_inference_worker_test
        PRIVATE vqec_vision_ai_inference_worker)
    set_target_properties(vqec_vision_ai_inference_worker_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME inference_worker_bounded_execution
        COMMAND vqec_vision_ai_inference_worker_test)
    add_executable(vqec_vision_ai_tensor_pool_test
        unit/vqec_vision_tensor_pool_test.cpp)
    target_link_libraries(vqec_vision_ai_tensor_pool_test
        PRIVATE vqec_vision_ai_core Threads::Threads)
    set_target_properties(vqec_vision_ai_tensor_pool_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tensor_pool_bounded_reuse COMMAND vqec_vision_ai_tensor_pool_test)
    add_executable(vqec_vision_ai_recovery_controller_test
        unit/vqec_vision_recovery_controller_test.cpp)
    target_link_libraries(vqec_vision_ai_recovery_controller_test
        PRIVATE vqec_vision_ai_recovery_controller)
    set_target_properties(vqec_vision_ai_recovery_controller_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME recovery_controller_backoff
        COMMAND vqec_vision_ai_recovery_controller_test)
    add_executable(vqec_vision_ai_secondary_inference_test
        unit/vqec_vision_secondary_inference_test.cpp)
    target_link_libraries(vqec_vision_ai_secondary_inference_test
        PRIVATE vqec_vision_ai_scheduler)
    set_target_properties(vqec_vision_ai_secondary_inference_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(vqec_vision_ai_preprocess_spec_test
        unit/vqec_vision_preprocess_spec_test.cpp)
    target_link_libraries(vqec_vision_ai_preprocess_spec_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_preprocess_spec_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME preprocess_spec_validation COMMAND vqec_vision_ai_preprocess_spec_test)

    add_executable(vqec_vision_ai_model_io_manifest_test
        unit/vqec_vision_model_io_manifest_test.cpp)
    target_link_libraries(vqec_vision_ai_model_io_manifest_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_model_io_manifest_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_io_manifest_matching COMMAND vqec_vision_ai_model_io_manifest_test)

    add_test(NAME secondary_inference_scheduling
        COMMAND vqec_vision_ai_secondary_inference_test)
    add_executable(vqec_vision_ai_lifecycle_invariants_test
        unit/vqec_vision_lifecycle_invariants_test.cpp)
    target_link_libraries(vqec_vision_ai_lifecycle_invariants_test
        PRIVATE vqec_vision_ai_core vqec_vision_ai_inference_worker Threads::Threads)
    set_target_properties(vqec_vision_ai_lifecycle_invariants_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME lifecycle_invariants_and_leak
        COMMAND vqec_vision_ai_lifecycle_invariants_test)
    add_executable(vqec_vision_ai_dense_decoder_test
        unit/vqec_vision_dense_decoder_test.cpp)
    target_link_libraries(vqec_vision_ai_dense_decoder_test
        PRIVATE vqec_vision_ai_detection)
    set_target_properties(vqec_vision_ai_dense_decoder_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME dense_decoder_detection
        COMMAND vqec_vision_ai_dense_decoder_test)
    add_executable(vqec_vision_ai_device_free_harness_test
        unit/vqec_vision_device_free_harness_test.cpp)
    target_link_libraries(vqec_vision_ai_device_free_harness_test
        PRIVATE vqec_vision_ai_core vqec_vision_ai_inference_worker Threads::Threads)
    set_target_properties(vqec_vision_ai_device_free_harness_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME device_free_harness_stress
        COMMAND vqec_vision_ai_device_free_harness_test)
    add_executable(vqec_vision_ai_multi_model_pump_qos_test
        unit/vqec_vision_multi_model_pump_qos_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_pump_qos_test
        PRIVATE vqec_vision_ai_multi_model_pump)
    set_target_properties(vqec_vision_ai_multi_model_pump_qos_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_pump_qos_mailbox
        COMMAND vqec_vision_ai_multi_model_pump_qos_test)
    add_executable(vqec_vision_ai_source_session_worker_test
        unit/vqec_vision_source_session_worker_test.cpp)
    target_link_libraries(vqec_vision_ai_source_session_worker_test
        PRIVATE vqec_vision_ai_source_session_worker Threads::Threads)
    set_target_properties(vqec_vision_ai_source_session_worker_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME source_session_worker_isolation
        COMMAND vqec_vision_ai_source_session_worker_test)
    add_executable(vqec_vision_ai_tensor_contract_test unit/vqec_vision_tensor_contract_test.cpp)
    target_link_libraries(vqec_vision_ai_tensor_contract_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_tensor_contract_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tensor_contract_validation COMMAND vqec_vision_ai_tensor_contract_test)
    if(VQEC_VISION_AI_ENABLE_CAMERA AND VQEC_VISION_AI_ENABLE_QUALCOMM)
        add_executable(vqec_vision_ai_camera_session_test contract/vqec_vision_camera_session_test.cpp)
        target_link_libraries(vqec_vision_ai_camera_session_test
            PRIVATE vqec_vision_ai_camera_graph_pump vqec_vision_ai_camera
                    vqec_vision_ai_qualcomm)
        set_target_properties(vqec_vision_ai_camera_session_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME camera_session_coordination COMMAND vqec_vision_ai_camera_session_test)
        set_tests_properties(camera_session_coordination PROPERTIES TIMEOUT 5)
        add_executable(vqec_vision_ai_camera_graph_pump_test
            contract/vqec_vision_camera_graph_pump_test.cpp)
        target_link_libraries(vqec_vision_ai_camera_graph_pump_test
            PRIVATE vqec_vision_ai_camera_graph_pump vqec_vision_ai_camera
                    vqec_vision_ai_qualcomm)
        set_target_properties(vqec_vision_ai_camera_graph_pump_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME camera_graph_pump_guards COMMAND vqec_vision_ai_camera_graph_pump_test)
    endif()
    add_executable(vqec_vision_ai_source_binding_test unit/vqec_vision_source_binding_test.cpp)
    target_link_libraries(vqec_vision_ai_source_binding_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_source_binding_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME source_binding_validation COMMAND vqec_vision_ai_source_binding_test)
    if(VQEC_VISION_AI_ENABLE_QUALCOMM)
        add_executable(vqec_vision_ai_plugin_graph_test contract/vqec_vision_plugin_graph_test.cpp)
        target_link_libraries(vqec_vision_ai_plugin_graph_test PRIVATE vqec_vision_ai_qualcomm)
        set_target_properties(vqec_vision_ai_plugin_graph_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME plugin_graph_state_guards COMMAND vqec_vision_ai_plugin_graph_test)
    endif()
    add_executable(vqec_vision_ai_inference_plan_test unit/vqec_vision_inference_plan_test.cpp)
    target_link_libraries(vqec_vision_ai_inference_plan_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_inference_plan_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME inference_plan_validation COMMAND vqec_vision_ai_inference_plan_test)
    add_executable(vqec_vision_ai_submission_window_test unit/vqec_vision_submission_window_test.cpp)
    target_link_libraries(vqec_vision_ai_submission_window_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_submission_window_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME submission_window COMMAND vqec_vision_ai_submission_window_test)
    add_executable(vqec_vision_ai_legacy_wire_test unit/vqec_vision_legacy_wire_test.cpp)
    target_link_libraries(vqec_vision_ai_legacy_wire_test PRIVATE vqec_vision_ai_camera_wire)
    set_target_properties(vqec_vision_ai_legacy_wire_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME legacy_camera_wire COMMAND vqec_vision_ai_legacy_wire_test)
    add_executable(vqec_vision_ai_camera_control_test unit/vqec_vision_camera_control_test.cpp)
    target_link_libraries(vqec_vision_ai_camera_control_test PRIVATE vqec_vision_ai_camera_control)
    set_target_properties(vqec_vision_ai_camera_control_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME camera_control_state COMMAND vqec_vision_ai_camera_control_test)
    if(TARGET vqec_vision_ai_gst_frame_bridge)
        find_package(Threads REQUIRED)
        # Separate binary: fixture entrypoint cannot be linked into the production backend.
        add_library(vqec_vision_ai_graph_fixture STATIC ${CMAKE_SOURCE_DIR}/src/adapters/qualcomm/vqec_vision_plugin_graph.cpp)
        target_compile_definitions(vqec_vision_ai_graph_fixture
            PUBLIC VQEC_VISION_AI_GRAPH_TEST_FIXTURE)
        target_link_libraries(vqec_vision_ai_graph_fixture PUBLIC vqec_vision_ai_gst_frame_bridge)
        set_target_properties(vqec_vision_ai_graph_fixture PROPERTIES CXX_EXTENSIONS OFF)
        add_executable(vqec_vision_ai_graph_lifecycle_test contract/vqec_vision_graph_lifecycle_test.cpp)
        target_link_libraries(vqec_vision_ai_graph_lifecycle_test
            PRIVATE vqec_vision_ai_graph_fixture Threads::Threads)
        set_target_properties(vqec_vision_ai_graph_lifecycle_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME graph_submission_lifecycle COMMAND vqec_vision_ai_graph_lifecycle_test)
        set_tests_properties(graph_submission_lifecycle PROPERTIES TIMEOUT 15)
        add_executable(vqec_vision_ai_frame_submission_test
            contract/vqec_vision_frame_submission_test.cpp)
        target_link_libraries(vqec_vision_ai_frame_submission_test
            PRIVATE vqec_vision_ai_gst_frame_bridge)
        set_target_properties(vqec_vision_ai_frame_submission_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME frame_submission_contract COMMAND vqec_vision_ai_frame_submission_test)
        set_tests_properties(frame_submission_contract PROPERTIES TIMEOUT 5)
        add_executable(vqec_vision_ai_tensor_output_test contract/vqec_vision_tensor_output_test.cpp)
        target_link_libraries(vqec_vision_ai_tensor_output_test PRIVATE vqec_vision_ai_gst_frame_bridge)
        set_target_properties(vqec_vision_ai_tensor_output_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME tensor_output_contract COMMAND vqec_vision_ai_tensor_output_test)
        add_executable(vqec_vision_ai_dmabuf_bridge_test contract/vqec_vision_dmabuf_bridge_test.cpp)
        target_link_libraries(vqec_vision_ai_dmabuf_bridge_test PRIVATE vqec_vision_ai_gst_frame_bridge)
        set_target_properties(vqec_vision_ai_dmabuf_bridge_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME dmabuf_memory_ownership COMMAND vqec_vision_ai_dmabuf_bridge_test)
        set_tests_properties(dmabuf_memory_ownership PROPERTIES TIMEOUT 5)
    endif()
    if(VQEC_VISION_AI_ENABLE_CAMERA)
        add_executable(vqec_vision_ai_raw_source_resolver_test
            unit/vqec_vision_raw_source_resolver_test.cpp)
        target_link_libraries(vqec_vision_ai_raw_source_resolver_test
            PRIVATE vqec_vision_ai_camera)
        set_target_properties(vqec_vision_ai_raw_source_resolver_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME raw_source_resolution COMMAND vqec_vision_ai_raw_source_resolver_test)
        add_executable(vqec_vision_ai_camera_receiver_test contract/vqec_vision_camera_receiver_test.cpp)
        target_link_libraries(vqec_vision_ai_camera_receiver_test PRIVATE vqec_vision_ai_camera)
        target_compile_definitions(vqec_vision_ai_camera_receiver_test PRIVATE _GNU_SOURCE)
        set_target_properties(vqec_vision_ai_camera_receiver_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME camera_receiver_contract COMMAND vqec_vision_ai_camera_receiver_test)
        set_tests_properties(camera_receiver_contract PROPERTIES TIMEOUT 5)
    endif()
    if(TARGET vqec_vision_ai_reference)
        add_executable(vqec_vision_ai_reference_tracker_test
            unit/vqec_vision_reference_tracker_test.cpp)
        target_link_libraries(vqec_vision_ai_reference_tracker_test
            PRIVATE vqec_vision_ai_reference)
        set_target_properties(vqec_vision_ai_reference_tracker_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME reference_tracker_association
            COMMAND vqec_vision_ai_reference_tracker_test)
        add_executable(vqec_vision_ai_reference_feature_test
            unit/vqec_vision_reference_feature_test.cpp)
        target_link_libraries(vqec_vision_ai_reference_feature_test
            PRIVATE vqec_vision_ai_reference)
        set_target_properties(vqec_vision_ai_reference_feature_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME reference_feature_events
            COMMAND vqec_vision_ai_reference_feature_test)
        add_executable(vqec_vision_ai_preprocess_conformance_test
            unit/vqec_vision_preprocess_conformance_test.cpp)
        target_link_libraries(vqec_vision_ai_preprocess_conformance_test
            PRIVATE vqec_vision_ai_reference)
        set_target_properties(vqec_vision_ai_preprocess_conformance_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME preprocess_conformance
            COMMAND vqec_vision_ai_preprocess_conformance_test)
        add_executable(vqec_vision_ai_reference_output_test
            unit/vqec_vision_reference_output_test.cpp)
        target_link_libraries(vqec_vision_ai_reference_output_test
            PRIVATE vqec_vision_ai_reference)
        set_target_properties(vqec_vision_ai_reference_output_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME reference_output_pipeline
            COMMAND vqec_vision_ai_reference_output_test)
        add_executable(vqec_vision_ai_runtime_executor_test
            contract/vqec_vision_runtime_executor_test.cpp)
        target_link_libraries(vqec_vision_ai_runtime_executor_test PRIVATE
            vqec_vision_ai_runtime_executor
            vqec_vision_ai_runtime_composition_factory
            vqec_vision_ai_reference
            vqec_vision_ai_feature_fanout
            vqec_vision_ai_feature_runtime)
        set_target_properties(vqec_vision_ai_runtime_executor_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME runtime_executor_e2e COMMAND vqec_vision_ai_runtime_executor_test)
        set_tests_properties(runtime_executor_e2e PROPERTIES TIMEOUT 10)
        add_executable(vqec_vision_ai_reference_graph_test
            contract/vqec_vision_reference_graph_test.cpp)
        target_link_libraries(vqec_vision_ai_reference_graph_test
            PRIVATE vqec_vision_ai_reference)
        set_target_properties(vqec_vision_ai_reference_graph_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME reference_graph_dtypes COMMAND vqec_vision_ai_reference_graph_test)
        add_executable(vqec_vision_ai_reference_processor_test
            unit/vqec_vision_reference_processor_test.cpp)
        target_link_libraries(vqec_vision_ai_reference_processor_test
            PRIVATE vqec_vision_ai_reference)
        set_target_properties(vqec_vision_ai_reference_processor_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME reference_processor_preprocess
            COMMAND vqec_vision_ai_reference_processor_test)
    endif()
    if(TARGET vqec_ai_vision_applications)
        add_test(NAME service_executable_smoke COMMAND vqec_ai_vision_applications
            --deployment ${CMAKE_CURRENT_SOURCE_DIR}/golden/service_smoke_deployment.json
            --model-catalog ${CMAKE_CURRENT_SOURCE_DIR}/golden/service_smoke_catalog.json
            --feature-catalog ${CMAKE_CURRENT_SOURCE_DIR}/golden/service_smoke_features.json
            --steps 160 --require-sources 2)
        set_tests_properties(service_executable_smoke PROPERTIES TIMEOUT 20)
        # Production mode must fail closed rather than substitute the fixture harness.
        add_test(NAME service_production_fail_closed COMMAND vqec_ai_vision_applications
            --mode production
            --deployment ${CMAKE_CURRENT_SOURCE_DIR}/golden/service_smoke_deployment.json
            --model-catalog ${CMAKE_CURRENT_SOURCE_DIR}/golden/service_smoke_catalog.json
            --steps 5)
        set_tests_properties(service_production_fail_closed PROPERTIES WILL_FAIL TRUE TIMEOUT 10)
        # Production mode runs end to end through the explicitly selected fake platform.
        add_test(NAME service_production_fake_smoke COMMAND vqec_ai_vision_applications
            --mode production --platform fake
            --deployment ${CMAKE_CURRENT_SOURCE_DIR}/golden/service_smoke_deployment.json
            --model-catalog ${CMAKE_CURRENT_SOURCE_DIR}/golden/service_smoke_catalog.json
            --feature-catalog ${CMAKE_CURRENT_SOURCE_DIR}/golden/service_smoke_features.json
            --steps 160 --require-sources 2)
        set_tests_properties(service_production_fake_smoke PROPERTIES TIMEOUT 20)
    endif()
