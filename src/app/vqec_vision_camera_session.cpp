#include "vqec_vision_camera_session.hpp"

#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {

status vqec_vision_ai_appl_camsn_bind_model_outputs(
    const model_outputs& _manifest, const model_output_selection& _selection,
    camera_session_config& _config) {
    if (_selection.model_id_.empty() || _selection.model_version_.empty() ||
        _selection.decoder_contract_.empty() || _selection.artifact_sha256_.size() != 64 ||
        _selection.artifact_sha256_.find_first_not_of("0123456789abcdef") != std::string::npos ||
        _manifest.model_id_ != _selection.model_id_ ||
        _manifest.model_version_ != _selection.model_version_ ||
        _manifest.artifact_sha256_ != _selection.artifact_sha256_ ||
        _manifest.decoder_contract_ != _selection.decoder_contract_) {
        return {status_code::invalid_argument, "manifest differs from selected model/decoder"};
    }
    std::uint64_t required_bytes = 0;
    const auto valid = vqec_vision_ai_core_tnctr_validate_outputs(
        _manifest.outputs_, _manifest.max_output_bytes_, required_bytes);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    auto outputs = _manifest.outputs_;  // Complete allocating work before modifying the config.
    _config.outputs_.swap(outputs);
    _config.max_output_bytes_ = _manifest.max_output_bytes_;
    return {};
}

camera_session::camera_session(source_lifecycle& _source, plugin_graph& _graph,
    std::shared_ptr<graph_retention> _retention, camera_session_config _config)
    : source_(_source), graph_(_graph), retention_(std::move(_retention)),
      config_(std::move(_config)),
      pump_(source_, graph_, retention_, config_.cycle_id_, config_.job_timeout_ns_) {}

status camera_session::vqec_vision_ai_appl_camsn_check_time(std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == UINT64_MAX || _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "session requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    return {};
}

status camera_session::vqec_vision_ai_appl_camsn_request_stop(std::uint64_t _steady_now_ns) {
    const auto time = vqec_vision_ai_appl_camsn_check_time(_steady_now_ns);
    if (time.code_ != status_code::ok) {
        return time;
    }
    if (state_ == camera_session_state::stopped || state_ == camera_session_state::draining_graph ||
        state_ == camera_session_state::releasing_camera) {
        return {};
    }
    pump_.vqec_vision_ai_appl_cgpmp_begin_stop();
    if (state_ == camera_session_state::idle) {
        state_ = camera_session_state::stopped;  // Never acquired ownership of external state.
        return {};
    }
    stop_ns_ = _steady_now_ns;
    state_ = camera_session_state::draining_graph;
    return {};
}

status camera_session::vqec_vision_ai_appl_camsn_stop_graph(std::uint64_t _steady_now_ns) {
    // EOS may be necessary to release downstream-held input; do not wait for zero jobs first.
    if (graph_.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::playing) {
        return graph_.vqec_vision_ai_qcom_plgr_request_drain();
    }
    if (graph_.vqec_vision_ai_qcom_plgr_get_outstanding() != 0) {
        tensor_result discarded;
        const auto result = graph_.vqec_vision_ai_qcom_plgr_poll_result(_steady_now_ns, discarded);
        if (graph_.vqec_vision_ai_qcom_plgr_get_outstanding() != 0) {
            return result.code_ == status_code::ok ?
                status{status_code::pending, "waiting for graph readers"} : result;
        }
    }
    const auto state = graph_.vqec_vision_ai_qcom_plgr_get_state();
    if (state == plugin_graph_state::empty || state == plugin_graph_state::configured) {
        state_ = camera_session_state::releasing_camera;
        return {status_code::pending, "graph released; next step reconciles camera lease"};
    }
    if (state == plugin_graph_state::starting || state == plugin_graph_state::draining ||
        state == plugin_graph_state::unloading) {
        return graph_.vqec_vision_ai_qcom_plgr_poll_state();
    }
    return graph_.vqec_vision_ai_qcom_plgr_unload_model();
}

status camera_session::vqec_vision_ai_appl_camsn_step(
    std::uint64_t _steady_now_ns, tensor_result& _result, camera_pump_report& _report) {
    _report = {};
    const auto time = vqec_vision_ai_appl_camsn_check_time(_steady_now_ns);
    if (time.code_ != status_code::ok) {
        return time;
    }
    if (state_ == camera_session_state::stopped) {
        return {};
    }
    if (state_ == camera_session_state::idle) {
        if (!retention_ || config_.cycle_id_ == 0 || config_.rpc_timeout_ms_ < 1 ||
            config_.rpc_timeout_ms_ > 60000 || config_.job_timeout_ns_ == 0 ||
            config_.job_timeout_ns_ == UINT64_MAX || config_.startup_timeout_ns_ == 0 ||
            config_.startup_timeout_ns_ == UINT64_MAX || config_.stop_timeout_ns_ == 0 ||
            config_.stop_timeout_ns_ == UINT64_MAX || config_.outputs_.empty() ||
            config_.outputs_.size() > 16 || config_.max_output_bytes_ == 0 ||
            config_.max_output_bytes_ > 64ULL * 1024 * 1024) {
            return {status_code::invalid_argument, "invalid session configuration"};
        }
        if (source_.vqec_vision_ai_camer_srclc_get_state() != camera_source_state::idle ||
            graph_.vqec_vision_ai_qcom_plgr_get_state() != plugin_graph_state::empty) {
            return {status_code::invalid_state, "session requires idle camera and empty graph"};
        }
        const auto valid =
            vqec_vision_ai_core_srcbd_validate_binding(config_.binding_, config_.plan_);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        std::uint64_t required_bytes = 0;
        const auto outputs = vqec_vision_ai_core_tnctr_validate_outputs(
            config_.outputs_, config_.max_output_bytes_, required_bytes);
        if (outputs.code_ != status_code::ok) {
            return outputs;
        }
        start_ns_ = _steady_now_ns;
        state_ = camera_session_state::acquiring;
        return {status_code::pending, "validated; next step acquires camera"};
    }
    const bool is_stopping = state_ == camera_session_state::draining_graph ||
        state_ == camera_session_state::releasing_camera;
    if (!is_stopping && state_ != camera_session_state::running &&
        _steady_now_ns - start_ns_ >= config_.startup_timeout_ns_) {
        const status expired{status_code::timeout, "session startup deadline elapsed"};
        if (last_error_.code_ == status_code::ok) {
            last_error_ = expired;
        }
        const auto stopped = vqec_vision_ai_appl_camsn_request_stop(_steady_now_ns);
        (void)stopped;
        return expired;
    }
    status progress;
    switch (state_) {
        case camera_session_state::acquiring:
            progress = source_.vqec_vision_ai_camer_srclc_start(config_.rpc_timeout_ms_);
            if (progress.code_ == status_code::ok) {
                state_ = camera_session_state::configuring;
            } else if ((progress.code_ == status_code::timeout ||
                        progress.code_ == status_code::source_lost) &&
                       source_.vqec_vision_ai_camer_srclc_get_state() !=
                           camera_source_state::draining) {
                if (last_error_.code_ == status_code::ok) {
                    last_error_ = progress;
                }
                return {status_code::pending, "camera acquisition/connect still pending"};
            }
            break;
        case camera_session_state::configuring: {
            const auto& profile = source_.vqec_vision_ai_camer_srclc_get_profile();
            if (profile.width_ != config_.plan_.source_width_ ||
                profile.height_ != config_.plan_.source_height_ ||
                static_cast<std::uint64_t>(profile.fps_) * config_.plan_.fps_denominator_ !=
                    config_.plan_.fps_numerator_) {
                progress = {status_code::unsupported, "FW effective profile differs from plan"};
                break;
            }
            progress = graph_.vqec_vision_ai_qcom_plgr_configure_graph(config_.plan_);
            if (progress.code_ == status_code::ok) {
                state_ = camera_session_state::loading;
            }
            break;
        }
        case camera_session_state::loading:
            progress =
                graph_.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::configured ?
                graph_.vqec_vision_ai_qcom_plgr_load_model() :
                graph_.vqec_vision_ai_qcom_plgr_poll_state();
            if (graph_.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::ready) {
                state_ = camera_session_state::binding;
            }
            break;
        case camera_session_state::binding:
            progress = graph_.vqec_vision_ai_qcom_plgr_bind_source(config_.binding_);
            if (progress.code_ == status_code::ok) {
                state_ = camera_session_state::starting;
            }
            break;
        case camera_session_state::starting:
            progress = graph_.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::ready ?
                graph_.vqec_vision_ai_qcom_plgr_start_stream(
                    config_.outputs_, config_.max_output_bytes_) :
                graph_.vqec_vision_ai_qcom_plgr_poll_state();
            if (graph_.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::playing) {
                state_ = camera_session_state::running;
            }
            break;
        case camera_session_state::running:
            progress = pump_.vqec_vision_ai_appl_cgpmp_pump_step(_steady_now_ns, _result, _report);
            break;
        case camera_session_state::draining_graph:
            progress = vqec_vision_ai_appl_camsn_stop_graph(_steady_now_ns);
            break;
        case camera_session_state::releasing_camera:
            progress = source_.vqec_vision_ai_camer_srclc_stop(config_.rpc_timeout_ms_);
            if (source_.vqec_vision_ai_camer_srclc_get_state() == camera_source_state::stopped) {
                state_ = camera_session_state::stopped;
                is_recovery_required_ = false;
                return {};
            }
            break;
        default:
            return {status_code::invalid_state, "unexpected session state"};
    }
    if (progress.code_ != status_code::ok && progress.code_ != status_code::pending) {
        if (last_error_.code_ == status_code::ok) {
            last_error_ = progress;
        }
        if (!is_stopping) {
            const auto stopped = vqec_vision_ai_appl_camsn_request_stop(_steady_now_ns);
            (void)stopped;
        }
    }
    if (is_stopping && _steady_now_ns - stop_ns_ >= config_.stop_timeout_ns_) {
        is_recovery_required_ = true;
        const status expired{status_code::timeout,
                             "stop deadline elapsed; retain resources and request recovery"};
        if (last_error_.code_ == status_code::ok) {
            last_error_ = expired;
        }
        return expired;
    }
    if (progress.code_ == status_code::ok && state_ != camera_session_state::running &&
        state_ != camera_session_state::stopped) {
        return {status_code::pending, "session lifecycle transition still in progress"};
    }
    return progress;
}

camera_session_state camera_session::vqec_vision_ai_appl_camsn_get_state() const noexcept {
    return state_;
}

bool camera_session::vqec_vision_ai_appl_camsn_is_recovery_required() const noexcept {
    return is_recovery_required_;
}

const status& camera_session::vqec_vision_ai_appl_camsn_get_last_error() const noexcept {
    return last_error_;
}

camera_session_snapshot camera_session::vqec_vision_ai_appl_camsn_get_snapshot() const noexcept {
    return {state_, source_.vqec_vision_ai_camer_srclc_get_state(),
            graph_.vqec_vision_ai_qcom_plgr_get_state(),
            graph_.vqec_vision_ai_qcom_plgr_get_outstanding(),
            source_.vqec_vision_ai_camer_srclc_get_outstanding(),
            is_recovery_required_, last_error_.code_};
}

}  // namespace vqec::vision::ai
