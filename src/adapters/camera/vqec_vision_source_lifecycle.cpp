#include "vqec_vision_source_lifecycle.hpp"

#include <utility>

#include "vqec_vision_camera_protocol.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_camer_srclc_is_identifier(const std::string& _value) {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, camera_protocol::g_max_identifier_bytes);
}

} // namespace

source_lifecycle::source_lifecycle(std::shared_ptr<camera_rpc> _rpc,
                                   camera_lifecycle_config _config)
    : config_(std::move(_config)), control_(std::move(_rpc)) {}

status source_lifecycle::vqec_vision_ai_camer_srclc_start(int _timeout_ms) {
    if (_timeout_ms < 1 || _timeout_ms > 60000) {
        return {status_code::invalid_argument, "Camera start timeout must be in 1..60000 ms"};
    }
    if (state_ == camera_source_state::draining || state_ == camera_source_state::releasing ||
        state_ == camera_source_state::stopped) {
        return {status_code::invalid_state, "this Camera acquisition cycle is stopping or stopped"};
    }
    if (state_ == camera_source_state::running) {
        if (!source_.vqec_vision_ai_camer_frsrc_is_healthy()) {
            state_ = camera_source_state::draining;
            return {status_code::source_lost, "Camera media session faulted; drain and stop"};
        }
        return {};
    }
    if (state_ == camera_source_state::idle) {
        if (config_.media_.socket_path_.empty() ||
            !vqec_vision_ai_camer_srclc_is_identifier(config_.stop_request_id_) ||
            !vqec_vision_ai_camer_srclc_is_identifier(config_.acquire_.request_id_) ||
            !vqec_vision_ai_camer_srclc_is_identifier(config_.acquire_.consumer_id_) ||
            config_.stop_request_id_ == config_.acquire_.request_id_ || config_.max_fps_ == 0 ||
            config_.media_.producer_uid_ == UINT32_MAX ||
            config_.media_.limits_.nv12_format_value_ == 0 ||
            config_.media_.limits_.max_width_ == 0 || config_.media_.limits_.max_height_ == 0 ||
            config_.media_.limits_.max_allocation_bytes_ == 0) {
            return {status_code::invalid_argument, "invalid Camera lifecycle configuration"};
        }
        state_ = camera_source_state::starting;
    }
    if (state_ == camera_source_state::starting) {
        const auto acquired =
            control_.vqec_vision_ai_camer_cctrl_start(config_.acquire_, _timeout_ms);
        if (acquired.code_ != status_code::ok) {
            if (control_.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::acquired) {
                state_ = camera_source_state::draining;
            }
            return acquired;
        }
        const auto& profile = control_.vqec_vision_ai_camer_cctrl_get_profile();
        if (profile.width_ > config_.media_.limits_.max_width_ ||
            profile.height_ > config_.media_.limits_.max_height_ ||
            profile.fps_ > config_.max_fps_) {
            state_ = camera_source_state::draining;
            return {status_code::unsupported, "FW effective profile exceeds AI admission limits"};
        }
        state_ = camera_source_state::connecting;
    }
    const auto connected = source_.vqec_vision_ai_camer_frsrc_connect(config_.media_);
    if (connected.code_ == status_code::ok) {
        state_ = camera_source_state::running;
    } else if (connected.code_ != status_code::timeout &&
               connected.code_ != status_code::source_lost) {
        state_ = camera_source_state::draining;
    }
    return connected;
}

status
source_lifecycle::vqec_vision_ai_camer_srclc_receive(std::shared_ptr<const received_frame>& _frame,
                                                     int _timeout_ms) {
    if (state_ != camera_source_state::running) {
        return {status_code::invalid_state, "Camera is not accepting frames"};
    }
    const auto received = source_.vqec_vision_ai_camer_frsrc_receive(_frame, _timeout_ms);
    if (received.code_ != status_code::ok) {
        if (!source_.vqec_vision_ai_camer_frsrc_is_healthy()) {
            state_ = camera_source_state::draining;
        }
        return received;
    }
    const auto& descriptor = _frame->vqec_vision_ai_camer_frsrc_get_descriptor();
    const auto& profile = control_.vqec_vision_ai_camer_cctrl_get_profile();
    if (descriptor.width_ != profile.width_ || descriptor.height_ != profile.height_) {
        _frame.reset(); // Not delivered/submitted: final owner may ACK immediately.
        state_ = camera_source_state::draining;
        return {status_code::unsupported, "Camera geometry changed; drain and reacquire profile"};
    }
    return {};
}

status source_lifecycle::vqec_vision_ai_camer_srclc_stop(int _timeout_ms) {
    if (_timeout_ms < 1 || _timeout_ms > 60000) {
        return {status_code::invalid_argument, "Camera stop timeout must be in 1..60000 ms"};
    }
    if (state_ == camera_source_state::stopped || state_ == camera_source_state::idle) {
        state_ = camera_source_state::stopped;
        return {};
    }
    state_ = camera_source_state::draining;
    if (source_.vqec_vision_ai_camer_frsrc_get_outstanding() != 0) {
        return {status_code::pending, "Camera readers still own frames; no StopStream issued"};
    }
    source_.vqec_vision_ai_camer_frsrc_disconnect();
    if (control_.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::start_pending) {
        const auto reconciled =
            control_.vqec_vision_ai_camer_cctrl_start(config_.acquire_, _timeout_ms);
        if (control_.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::acquired) {
            return {status_code::pending, "Camera handle recovered; next stop step releases it"};
        }
        return reconciled;
    }
    state_ = camera_source_state::releasing;
    const auto released =
        control_.vqec_vision_ai_camer_cctrl_stop(config_.stop_request_id_, _timeout_ms);
    if (released.code_ == status_code::ok) {
        state_ = camera_source_state::stopped;
    }
    return released;
}

camera_source_state source_lifecycle::vqec_vision_ai_camer_srclc_get_state() const noexcept {
    return state_;
}

unsigned source_lifecycle::vqec_vision_ai_camer_srclc_get_outstanding() const noexcept {
    return source_.vqec_vision_ai_camer_frsrc_get_outstanding();
}

const camera_stream_profile&
source_lifecycle::vqec_vision_ai_camer_srclc_get_profile() const noexcept {
    return control_.vqec_vision_ai_camer_cctrl_get_profile();
}

status source_lifecycle::vqec_vision_ai_ports_rawsr_start(int _timeout_ms) {
    return vqec_vision_ai_camer_srclc_start(_timeout_ms);
}

status source_lifecycle::vqec_vision_ai_ports_rawsr_receive(
    raw_frame& _frame, int _timeout_ms) {
    if (_frame.owner_ || _frame.native_handle_ != -1) {
        return {status_code::invalid_argument, "RAW frame destination is occupied"};
    }
    std::shared_ptr<const received_frame> received;
    const auto received_status = vqec_vision_ai_camer_srclc_receive(received, _timeout_ms);
    if (received_status.code_ != status_code::ok) {
        return received_status;
    }
    raw_frame candidate;
    candidate.descriptor_ = received->vqec_vision_ai_camer_frsrc_get_descriptor();
    candidate.native_handle_ = received->vqec_vision_ai_camer_frsrc_get_fd();
    candidate.owner_ = std::move(received);
    _frame = std::move(candidate);
    return {};
}

status source_lifecycle::vqec_vision_ai_ports_rawsr_stop(int _timeout_ms) {
    return vqec_vision_ai_camer_srclc_stop(_timeout_ms);
}

raw_source_state source_lifecycle::vqec_vision_ai_ports_rawsr_get_state() const noexcept {
    switch (state_) {
        case camera_source_state::idle:
            return raw_source_state::idle;
        case camera_source_state::starting:
        case camera_source_state::connecting:
            return raw_source_state::starting;
        case camera_source_state::running:
            return raw_source_state::running;
        case camera_source_state::draining:
        case camera_source_state::releasing:
            return raw_source_state::draining;
        case camera_source_state::stopped:
            return raw_source_state::stopped;
    }
    return raw_source_state::draining;
}

raw_source_profile source_lifecycle::vqec_vision_ai_ports_rawsr_get_profile()
    const noexcept {
    const auto& profile = vqec_vision_ai_camer_srclc_get_profile();
    return {profile.width_, profile.height_, profile.fps_, profile.is_valid_ ? 1U : 0U};
}

unsigned source_lifecycle::vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept {
    return vqec_vision_ai_camer_srclc_get_outstanding();
}

} // namespace vqec::vision::ai
