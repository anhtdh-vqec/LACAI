#include "vqec_vision_service_enrollment_runtime.hpp"

#include <array>
#include <utility>

#include "vqec_vision_cascade_coordinator.hpp"
#include "vqec_vision_cascade_graph_session.hpp"
#include "vqec_vision_face_enrollment_controller.hpp"
#include "vqec_vision_face_enrollment_image_pipeline.hpp"
#include "vqec_vision_face_enrollment_image_source.hpp"
#include "vqec_vision_image_path_authorizer.hpp"
#include "vqec_vision_production_platform.hpp"
#include "vqec_vision_service_cascade_runtime.hpp"
#include "vqec_vision_service_options.hpp"
#include "vqec_vision_single_image_inference.hpp"

#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
#include "vqec_vision_face_enrollment_dbus.hpp"
#endif

namespace vqec::vision::ai {
namespace {

const model_catalog_entry* vqec_vision_ai_appl_svenr_find_model(
    const model_catalog& _catalog, const std::string& _model_id) {
    for (const auto& model : _catalog.models_) {
        if (model.model_id_ == _model_id) return &model;
    }
    return nullptr;
}

}  // namespace

struct service_enrollment_runtime::implementation {
    std::unique_ptr<face_enrollment_controller> controller_;
    std::unique_ptr<production_offline_model> detector_model_;
    std::unique_ptr<production_offline_model> embedding_model_;
    std::unique_ptr<cascade_graph_session> detector_graph_session_;
    std::unique_ptr<cascade_graph_session> embedding_graph_session_;
    std::unique_ptr<single_image_inference> detector_;
    std::unique_ptr<cascade_coordinator> cascade_;
    std::unique_ptr<image_path_authorizer> path_authorizer_;
    std::unique_ptr<qcom_face_enrollment_image_source> image_source_;
    std::unique_ptr<face_enrollment_image_pipeline> image_pipeline_;
    face_enrollment_port* port_{nullptr};
#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
    std::unique_ptr<face_enrollment_dbus_server> dbus_;
#endif
    bool is_configured_{false};
};

service_enrollment_runtime::service_enrollment_runtime()
    : implementation_(std::make_unique<implementation>()) {}

service_enrollment_runtime::~service_enrollment_runtime() = default;

status service_enrollment_runtime::vqec_vision_ai_appl_svenr_configure(
    const parsed_arguments& _arguments, recognition_session& _recognition,
    production_platform& _platform, std::uint16_t _source_slot,
    const source_deployment_config& _source, const model_catalog& _catalog,
    const model_catalog_entry& _embedding_model, std::uint64_t _cycle_id) {
    auto& owner = *implementation_;
    if (owner.is_configured_ || _cycle_id == 0) {
        return {status_code::invalid_state, "invalid enrollment runtime configuration"};
    }
    owner.controller_ = std::make_unique<face_enrollment_controller>(_recognition);
    owner.port_ = owner.controller_.get();
    if (!_arguments.enrollment_dbus) {
        owner.is_configured_ = true;
        return {};
    }
    const auto* detector_model = _embedding_model.depends_on_.size() == 1U ?
        vqec_vision_ai_appl_svenr_find_model(
            _catalog, _embedding_model.depends_on_[0].model_id_) : nullptr;
    if (detector_model == nullptr) {
        return {status_code::invalid_argument,
            "enrollment model dependency is incomplete"};
    }
    auto prepared = _platform.vqec_vision_ai_appl_pdplt_create_offline_model(
        _source_slot, detector_model->model_id_, owner.detector_model_);
    if (prepared.code_ == status_code::ok) {
        prepared = _platform.vqec_vision_ai_appl_pdplt_create_offline_model(
            _source_slot, _embedding_model.model_id_, owner.embedding_model_);
    }
    if (prepared.code_ == status_code::ok) {
        prepared = vqec_vision_ai_appl_svcsc_make_offline_graph_session(
            _source, *detector_model, *owner.detector_model_,
            owner.detector_graph_session_);
    }
    if (prepared.code_ == status_code::ok) {
        prepared = vqec_vision_ai_appl_svcsc_make_offline_graph_session(
            _source, _embedding_model, *owner.embedding_model_,
            owner.embedding_graph_session_);
    }
    if (prepared.code_ != status_code::ok) return prepared;

    owner.detector_ = std::make_unique<single_image_inference>();
    single_image_inference_config detector_config;
    detector_config.processor_ =
        owner.detector_model_->vqec_vision_ai_appl_pdplt_get_processor();
    detector_config.graph_ = owner.detector_model_->vqec_vision_ai_appl_pdplt_get_graph();
    detector_config.decoder_ =
        owner.detector_model_->vqec_vision_ai_appl_pdplt_get_decoder();
    detector_config.plan_ =
        &owner.detector_model_->vqec_vision_ai_appl_pdplt_get_binding().plan_;
    detector_config.geometry_ = {_source.profile_.width_, _source.profile_.height_};
    detector_config.camera_id_ = _source.camera_id_;
    detector_config.channel_id_ = _source.channel_id_;
    detector_config.cycle_id_ = _cycle_id;
    detector_config.job_timeout_ns_ = submission_limits::g_default_job_timeout_ns;
    prepared = owner.detector_->vqec_vision_ai_appl_siinf_configure(detector_config);

    owner.cascade_ = std::make_unique<cascade_coordinator>();
    cascade_coordinator_config cascade_config;
    const auto& embedding_binding =
        owner.embedding_model_->vqec_vision_ai_appl_pdplt_get_binding();
    cascade_config.aligner_ =
        owner.embedding_model_->vqec_vision_ai_appl_pdplt_get_aligner();
    cascade_config.embedding_graph_ =
        owner.embedding_model_->vqec_vision_ai_appl_pdplt_get_graph();
    cascade_config.embedding_decoder_ = owner.embedding_model_->
        vqec_vision_ai_appl_pdplt_get_embedding_decoder();
    cascade_config.template_ = embedding_binding.alignment_;
    cascade_config.normalize_offset_ = embedding_binding.preprocess_.offset_;
    cascade_config.normalize_scale_ = embedding_binding.preprocess_.scale_;
    cascade_config.cycle_id_ = _cycle_id + 1U;
    cascade_config.job_timeout_ns_ = submission_limits::g_default_job_timeout_ns;
    cascade_config.max_tasks_per_frame_ = 1U;
    cascade_config.control_budget_ns_ =
        cascade_coordinator_limits::g_default_control_budget_ns;
    if (prepared.code_ == status_code::ok) {
        prepared = owner.cascade_->vqec_vision_ai_appl_cscrd_configure(cascade_config);
    }

    owner.path_authorizer_ = std::make_unique<image_path_authorizer>();
    if (prepared.code_ == status_code::ok) {
        prepared = owner.path_authorizer_->vqec_vision_ai_fwctl_ipath_configure(
            {_arguments.enrollment_image_roots,
                _arguments.enrollment_max_image_bytes});
    }
    owner.image_source_ = std::make_unique<qcom_face_enrollment_image_source>(
        qcom_face_enrollment_image_source_config{_arguments.enrollment_jpeg_decoder,
            _arguments.enrollment_converter, _arguments.enrollment_scaler,
            _arguments.enrollment_transform, _arguments.enrollment_transform_engine,
            _arguments.enrollment_max_image_bytes,
            _arguments.enrollment_image_timeout_ms, true});
    owner.image_pipeline_ = std::make_unique<face_enrollment_image_pipeline>();
    face_enrollment_image_pipeline_config pipeline_config;
    pipeline_config.controller_ = owner.controller_.get();
    pipeline_config.path_authorizer_ = owner.path_authorizer_.get();
    pipeline_config.image_source_ = owner.image_source_.get();
    pipeline_config.detector_ = owner.detector_.get();
    pipeline_config.cascade_ = owner.cascade_.get();
    pipeline_config.geometry_ = detector_config.geometry_;
    pipeline_config.source_epoch_ = _cycle_id;
    pipeline_config.source_id_ = _source.source_id_;
    pipeline_config.camera_id_ = _source.camera_id_;
    pipeline_config.channel_id_ = _source.channel_id_;
    if (prepared.code_ == status_code::ok) {
        prepared = owner.image_pipeline_->vqec_vision_ai_appl_feipl_configure(
            pipeline_config);
    }
    if (prepared.code_ != status_code::ok) {
        (void)vqec_vision_ai_appl_svenr_stop();
        return prepared;
    }
    owner.port_ = owner.image_pipeline_.get();

#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
    owner.dbus_ = std::make_unique<face_enrollment_dbus_server>();
    face_enrollment_dbus_config dbus_config;
    dbus_config.trusted_peer_bus_name_ = _arguments.enrollment_peer_name;
    dbus_config.rpc_timeout_ms_ = _arguments.enrollment_rpc_timeout_ms;
    dbus_config.max_callbacks_per_poll_ = _arguments.enrollment_callbacks_per_poll;
    dbus_config.use_session_bus_ = _arguments.enrollment_dbus_session_bus;
    prepared = owner.dbus_->vqec_vision_ai_fwctl_fedbs_open(*owner.port_, dbus_config);
#else
    prepared = {status_code::unsupported,
        "face enrollment D-Bus adapter is not built"};
#endif
    if (prepared.code_ != status_code::ok) {
        (void)vqec_vision_ai_appl_svenr_stop();
        return prepared;
    }
    owner.is_configured_ = true;
    return {};
}

status service_enrollment_runtime::vqec_vision_ai_appl_svenr_poll(
    std::uint64_t _steady_now_ns) {
    auto& owner = *implementation_;
    if (!owner.is_configured_) {
        return {status_code::invalid_state, "enrollment runtime is not configured"};
    }
#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
    if (owner.dbus_ != nullptr) owner.dbus_->vqec_vision_ai_fwctl_fedbs_poll();
#endif
    if (owner.image_pipeline_ == nullptr ||
        !owner.image_pipeline_->vqec_vision_ai_appl_feipl_has_pending()) {
        return {status_code::pending, "enrollment runtime is idle"};
    }
    if (owner.image_pipeline_->vqec_vision_ai_appl_feipl_has_ready_image()) {
        const auto started = vqec_vision_ai_appl_svcsc_start_graph_sessions(
            std::array<cascade_graph_session*, 2>{owner.detector_graph_session_.get(),
                owner.embedding_graph_session_.get()});
        if (started.code_ != status_code::ok) {
            (void)owner.image_pipeline_->vqec_vision_ai_appl_feipl_fail_pending(
                started.code_);
            return started;
        }
    }
    return owner.image_pipeline_->vqec_vision_ai_appl_feipl_step(_steady_now_ns);
}

status service_enrollment_runtime::vqec_vision_ai_appl_svenr_stop() {
    auto& owner = *implementation_;
    return vqec_vision_ai_appl_svcsc_stop_graph_sessions(
        std::array<cascade_graph_session*, 2>{owner.detector_graph_session_.get(),
            owner.embedding_graph_session_.get()});
}

face_enrollment_port* service_enrollment_runtime::
vqec_vision_ai_appl_svenr_get_port() noexcept {
    return implementation_->port_;
}

}  // namespace vqec::vision::ai
