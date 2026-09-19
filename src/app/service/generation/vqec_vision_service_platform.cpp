#include "vqec_vision_service_platform.hpp"

#include <algorithm>
#include <fstream>
#include <new>
#include <utility>

#include "vqec_vision_hardware_admission_profile.hpp"
#include "vqec_vision_service_fixture.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_mebibyte = 1024ULL * 1024ULL;

hardware_admission_profile vqec_vision_ai_appl_svplt_make_fixture_profile() {
    hardware_admission_profile profile;
    profile.profile_id_ = "device_free_test_fixture";
    profile.target_id_ = "qcs6490_qlinux_1_8";
    profile.measurement_reference_ = "fixture-only-not-a-board-measurement";
    profile.revision_ = 1;
    profile.max_total_resident_bytes_ = 4096ULL * g_mebibyte;
    profile.max_frame_pool_bytes_ = 1024ULL * g_mebibyte;
    profile.max_tensor_pool_bytes_ = 1024ULL * g_mebibyte;
    profile.max_encoder_pool_bytes_ = 512ULL * g_mebibyte;
    profile.max_cascade_roi_bytes_ = 512ULL * g_mebibyte;
    profile.max_ddr_bandwidth_mbps_ = 12000;
    profile.max_fw_concurrency_slots_ = 16;
    profile.max_worker_concurrency_ = 64;
    profile.min_thermal_headroom_pct_ = 10;
    return profile;
}

status vqec_vision_ai_appl_svplt_resolve_hardware_profile(
    const parsed_arguments& _args, bool _production,
    hardware_admission_profile& _profile) {
    if (_args.hardware_profile_path.empty()) {
        if (_production) {
            return {status_code::unsupported,
                "Qualcomm production requires --hardware-profile"};
        }
        _profile = vqec_vision_ai_appl_svplt_make_fixture_profile();
        return {};
    }
    std::ifstream stream(_args.hardware_profile_path, std::ios::binary);
    if (!stream) {
        return {status_code::io_error, "cannot open hardware admission profile"};
    }
    return vqec_vision_ai_admis_hwprf_load(stream, _profile);
}

const model_catalog_entry* vqec_vision_ai_appl_svplt_find_model(
    const model_catalog& _catalog, const std::string& _model_id) noexcept {
    const auto found = std::find_if(_catalog.models_.begin(), _catalog.models_.end(),
        [&_model_id](const auto& _model) { return _model.model_id_ == _model_id; });
    return found == _catalog.models_.end() ? nullptr : &*found;
}

model_outputs vqec_vision_ai_appl_svplt_make_fixture_outputs(
    const model_catalog_entry& _model) {
    model_outputs outputs;
    outputs.model_id_ = _model.model_id_;
    outputs.model_version_ = _model.model_version_;
    outputs.artifact_sha256_ = _model.artifact_sha256_;
    outputs.decoder_contract_ = _model.decoder_contract_;
    outputs.max_output_bytes_ = service_harness::g_output_bytes;
    outputs.outputs_.push_back({service_harness::g_box_tensor_name,
        {1, service_harness::g_box_elements}, tensor_element_type::float32, {}});
    return outputs;
}

status vqec_vision_ai_appl_svplt_build_activations(
    const parsed_arguments& _args, const deployment_config& _deployment,
    const model_catalog& _catalog, production_platform& _production,
    const std::vector<raw_source_port*>& _sources, bool _production_enabled,
    const std::string& _tracker_contract,
    std::vector<std::unique_ptr<reference_inference_graph>>& _reference_graphs,
    runtime_composition_activation& _activation) {
    for (std::uint16_t source_slot = 0; source_slot < _activation.source_count_;
         ++source_slot) {
        const auto& source = _deployment.sources_[source_slot];
        auto& source_activation = _activation.sources_[source_slot];
        source_activation.source_id_ = source.source_id_;
        source_activation.source_ = _sources[source_slot];
        source_activation.model_count_ =
            static_cast<std::uint16_t>(source.model_ids_.size());
        for (std::uint16_t model_slot = 0;
             model_slot < source_activation.model_count_; ++model_slot) {
            const auto* model = vqec_vision_ai_appl_svplt_find_model(
                _catalog, source.model_ids_[model_slot]);
            if (model == nullptr) {
                return {status_code::invalid_argument,
                    "deployment references an unknown model"};
            }
            auto& model_activation = source_activation.models_[model_slot];
            model_activation.model_id_ = model->model_id_;
            if (_production_enabled) {
                auto* graph = _production.vqec_vision_ai_appl_pdplt_graph(
                    source_slot, model->model_id_);
                auto* processor = _production.vqec_vision_ai_appl_pdplt_processor(
                    source_slot, model->model_id_);
                const auto* outputs =
                    _production.vqec_vision_ai_appl_pdplt_outputs(model->model_id_);
                if (graph == nullptr || processor == nullptr || outputs == nullptr) {
                    return {status_code::invalid_argument,
                        "production platform model owner is incomplete"};
                }
                model_activation.graph_ = graph;
                model_activation.processor_ = processor;
                model_activation.outputs_ = *outputs;
                model_activation.paths_ =
                    _production.vqec_vision_ai_appl_pdplt_paths(model->model_id_);
            } else {
                _reference_graphs.push_back(
                    std::make_unique<reference_inference_graph>());
                model_activation.graph_ = _reference_graphs.back().get();
                model_activation.paths_.model_id_ = model->model_id_;
                model_activation.paths_.target_id_ = model->target_id_;
                model_activation.paths_.artifact_ref_ = model->artifact_ref_;
                const auto& root = _args.model_root.empty() ?
                    service_harness::g_fixture_model_root : _args.model_root;
                model_activation.paths_.model_path_ = root + model->model_id_ + ".bin";
                model_activation.paths_.backend_path_ =
                    _args.qnn_backend_library.empty() ?
                        service_harness::g_fixture_backend_library :
                        _args.qnn_backend_library;
                model_activation.paths_.system_path_ =
                    _args.qnn_system_library.empty() ?
                        service_harness::g_fixture_system_library :
                        _args.qnn_system_library;
                model_activation.outputs_ =
                    vqec_vision_ai_appl_svplt_make_fixture_outputs(*model);
            }
            model_activation.resolved_output_manifest_ref_ =
                model->output_manifest_ref_;
            model_activation.tracker_contract_ = _tracker_contract;
            model_activation.binding_.width_ = source.profile_.width_;
            model_activation.binding_.height_ = source.profile_.height_;
            model_activation.binding_.fps_numerator_ = source.profile_.fps_numerator_;
            model_activation.binding_.fps_denominator_ = source.profile_.fps_denominator_;
            model_activation.binding_.memory_kind_ = source_memory_kind::dmabuf;
            model_activation.binding_.layout_ = source_memory_layout::linear_nv12;
            model_activation.binding_.sync_mode_ = source_sync_mode::implicit_ready;
            model_activation.binding_.color_profile_ = source_color_profile::bt709_limited;
            model_activation.binding_.chroma_site_ = source_chroma_site::mpeg2;
            model_activation.binding_.fw_memory_contract_ =
                service_harness::g_fw_dmabuf_contract;
            model_activation.binding_.backend_memory_contract_ =
                service_harness::g_qcom_dmabuf_contract;
            model_activation.binding_.preprocess_contract_ = model->preprocess_contract_;
            model_activation.cycle_id_ = static_cast<std::uint64_t>(source_slot) *
                    service_harness::g_cycle_id_stride + model_slot + 1U;
            model_activation.job_timeout_ns_ =
                submission_limits::g_default_job_timeout_ns;
        }
    }
    return {};
}

}  // namespace

status service_platform::vqec_vision_ai_appl_svplt_prepare(
    const parsed_arguments& _args, const deployment_config& _deployment,
    const model_catalog& _catalog, const feature_catalog& _features,
    const model_package_registry& _model_packages,
    bool _use_reference_platform, bool _use_production_platform) {
    if (prepared_ || _deployment.sources_.empty()) {
        return {status_code::invalid_state, "service platform cannot be prepared"};
    }
    if (_use_production_platform && !_args.output_ring_id.empty()) {
        for (const auto& source : _deployment.sources_) {
            if (_args.output_surface_count != source.memory_.preview_surface_count_) {
                return {status_code::invalid_argument,
                    "preview surface count differs from admitted deployment envelope"};
            }
        }
    }
    auto current = compiled_feature_factories_.
        vqec_vision_ai_appl_sfreg_register_compiled(
            _features, feature_registry_, platform_features_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    const auto source_width = _deployment.sources_.front().profile_.width_;
    const auto source_height = _deployment.sources_.front().profile_.height_;
    if (_use_production_platform) {
        production_platform_config config;
        config.model_packages_ = _model_packages;
        config.execution_policy_ = _args.execution_policy;
        config.backend_library_ = _args.qnn_backend_library;
        config.system_library_ = _args.qnn_system_library;
        config.allow_qaic_copy_input_ = _args.allow_qaic_copy_input;
        config.model_root_ = _args.model_root;
        config.dsp_v1_skel_dir_ = _args.dsp_v1_skel_dir;
        config.dsp_legacy_skel_dir_ = _args.dsp_legacy_skel_dir;
        config.dsp_legacy_clock_corner_ = _args.dsp_legacy_clock_corner;
        config.dsp_legacy_latency_us_ = _args.dsp_legacy_latency_us;
        config.dsp_enable_unsigned_pd_ = _args.dsp_enable_unsigned_pd;
        config.max_artifact_bytes_ = _args.max_artifact_bytes > 0 ?
            _args.max_artifact_bytes :
            production_platform_limits::g_default_max_artifact_bytes;
        config.socket_dir_ = _args.camera_socket_dir;
        config.producer_uid_ = _args.camera_producer_uid;
        config.nv12_format_value_ = _args.nv12_format_value;
        config.preprocess_output_timeout_ns_ =
            submission_limits::g_default_job_timeout_ns;
        config.tracker_contract_ = _args.tracker_contract;
        config.event_schema_id_ = _args.event_schema_id;
        config.event_schema_version_ = _args.event_schema_version;
        config.consumer_id_prefix_ = _args.consumer_id_prefix;
        config.output_ring_id_ = _args.output_ring_id;
        config.output_fps_ = _args.output_fps;
        config.output_bitrate_bps_ = _args.output_bitrate_bps;
        config.output_keyframe_interval_frames_ =
            _args.output_keyframe_interval_frames;
        config.output_box_color_rgba_ = _args.output_box_color_rgba;
        config.output_surface_count_ = _args.output_surface_count;
        config.output_colorimetry_ = _args.output_colorimetry;
        config.output_interlace_mode_ = _args.output_interlace_mode;
        current = production_.vqec_vision_ai_appl_pdplt_configure(config);
        if (current.code_ == status_code::ok) {
            current = production_.vqec_vision_ai_appl_pdplt_prepare(
                _deployment, _catalog);
        }
        if (current.code_ == status_code::ok) {
            current = production_.vqec_vision_ai_appl_pdplt_register_decoders(
                _catalog, decoders_);
        }
        if (current.code_ == status_code::ok) {
            current = production_.vqec_vision_ai_appl_pdplt_register_tracker(trackers_);
        }
        if (current.code_ == status_code::ok) {
            current = production_.vqec_vision_ai_appl_pdplt_register_features(
                platform_features_, feature_registry_);
        }
        if (current.code_ != status_code::ok) {
            return current;
        }
        tracker_contract_ = production_.vqec_vision_ai_appl_pdplt_get_tracker_contract();
        attribute_schema_id_ =
            production_.vqec_vision_ai_appl_pdplt_get_attribute_schema_id();
    } else if (_use_reference_platform) {
        current = reference_.vqec_vision_ai_appl_rplat_configure(
            {source_width, source_height});
        if (current.code_ == status_code::ok) {
            current = reference_.vqec_vision_ai_appl_rplat_register_decoders(
                _catalog, decoders_);
        }
        if (current.code_ == status_code::ok) {
            current = reference_.vqec_vision_ai_appl_rplat_register_tracker(trackers_);
        }
        if (current.code_ == status_code::ok) {
            current = reference_.vqec_vision_ai_appl_rplat_register_features(
                platform_features_, feature_registry_);
        }
        if (current.code_ != status_code::ok) {
            return current;
        }
        tracker_contract_ = reference_.vqec_vision_ai_appl_rplat_get_tracker_contract();
        attribute_schema_id_ = reference_.vqec_vision_ai_appl_rplat_get_attribute_schema_id();
    } else {
        current = fake_.vqec_vision_ai_appl_fkplt_configure(
            {source_width, source_height});
        if (current.code_ == status_code::ok) {
            current = fake_.vqec_vision_ai_appl_fkplt_register_decoders(
                _catalog, decoders_);
        }
        if (current.code_ == status_code::ok) {
            current = fake_.vqec_vision_ai_appl_fkplt_register_tracker(trackers_);
        }
        if (current.code_ == status_code::ok) {
            current = fake_.vqec_vision_ai_appl_fkplt_register_features(
                platform_features_, feature_registry_);
        }
        if (current.code_ != status_code::ok) {
            return current;
        }
        tracker_contract_ = fake_.vqec_vision_ai_appl_fkplt_get_tracker_contract();
        attribute_schema_id_ = fake_.vqec_vision_ai_appl_fkplt_get_config().attribute_schema_id_;
    }

    sources_.reserve(_deployment.sources_.size());
    if (_use_production_platform) {
        for (std::uint16_t slot = 0; slot < _deployment.sources_.size(); ++slot) {
            auto* source = production_.vqec_vision_ai_appl_pdplt_source(slot);
            if (source == nullptr) {
                return {status_code::invalid_state,
                    "production platform source owner is missing"};
            }
            sources_.push_back(source);
        }
    } else {
        for (const auto& source : _deployment.sources_) {
            reference_sources_.push_back(std::make_unique<reference_raw_source>(
                reference_source_config{source.profile_.width_, source.profile_.height_,
                    source.profile_.fps_numerator_, source.profile_.fps_denominator_}));
            sources_.push_back(reference_sources_.back().get());
        }
    }

    activation_.source_count_ =
        static_cast<std::uint16_t>(_deployment.sources_.size());
    activation_.startup_timeout_ns_ = service_harness::g_default_startup_timeout_ns;
    activation_.stop_timeout_ns_ = service_harness::g_default_stop_timeout_ns;
    activation_.rpc_timeout_ms_ = service_harness::g_default_rpc_timeout_ms;
    activation_.use_session_workers_ = _args.use_session_workers;
    activation_.use_model_workers_ = _args.use_model_workers;
    current = vqec_vision_ai_appl_svplt_resolve_hardware_profile(
        _args, _use_production_platform, activation_.hardware_profile_);
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_appl_svplt_build_activations(
            _args, _deployment, _catalog, production_, sources_,
            _use_production_platform, tracker_contract_, reference_graphs_, activation_);
    }
    if (current.code_ != status_code::ok) {
        return current;
    }
    prepared_ = true;
    return {};
}

production_platform& service_platform::
vqec_vision_ai_appl_svplt_get_production() noexcept {
    return production_;
}

model_decoder_registry& service_platform::
vqec_vision_ai_appl_svplt_get_decoders() noexcept {
    return decoders_;
}

tracker_registry& service_platform::
vqec_vision_ai_appl_svplt_get_trackers() noexcept {
    return trackers_;
}

feature_processor_registry& service_platform::
vqec_vision_ai_appl_svplt_get_features() noexcept {
    return feature_registry_;
}

runtime_composition_activation& service_platform::
vqec_vision_ai_appl_svplt_get_activation() noexcept {
    return activation_;
}

const std::string& service_platform::
vqec_vision_ai_appl_svplt_get_tracker_contract() const noexcept {
    return tracker_contract_;
}

const std::string& service_platform::
vqec_vision_ai_appl_svplt_get_attribute_schema() const noexcept {
    return attribute_schema_id_;
}

}  // namespace vqec::vision::ai
