#include "vqec_vision_cascade_coordinator.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_color.hpp"

namespace vqec::vision::ai {
namespace {

class direct_frame_lease final : public cascade_frame_lease_port {
public:
    direct_frame_lease(const raw_frame& _frame, const preview_frame_key& _key) noexcept
        : frame_(_frame), key_(_key) {}
    status vqec_vision_ai_ports_cflse_acquire(
        const preview_frame_key& _key, raw_frame& _frame,
        std::uint64_t& _ticket) override {
        if (is_outstanding_ || _key.camera_id_ != key_.camera_id_ ||
            _key.channel_id_ != key_.channel_id_ || _key.source_epoch_ != key_.source_epoch_ ||
            _key.frame_id_ != key_.frame_id_ || _key.source_pts_ns_ != key_.source_pts_ns_) {
            return {status_code::invalid_state, "direct frame lease identity is unavailable"};
        }
        is_outstanding_ = true;
        _ticket = next_ticket_++;
        active_ticket_ = _ticket;
        _frame = frame_;
        return {};
    }
    status vqec_vision_ai_ports_cflse_complete(std::uint64_t _ticket) override {
        if (!is_outstanding_ || _ticket != active_ticket_) {
            return {status_code::invalid_state, "direct frame lease ticket is invalid"};
        }
        is_outstanding_ = false;
        active_ticket_ = 0;
        return {};
    }
    status vqec_vision_ai_ports_cflse_retire(const preview_frame_key& _key) override {
        if (is_outstanding_ || _key.source_epoch_ != key_.source_epoch_ ||
            _key.frame_id_ != key_.frame_id_) {
            return {status_code::invalid_state, "direct frame lease cannot retire"};
        }
        return {};
    }

private:
    raw_frame frame_;
    preview_frame_key key_;
    std::uint64_t next_ticket_{1};
    std::uint64_t active_ticket_{0};
    bool is_outstanding_{false};
};

// Builds the quantized model input tensor from an aligned uint8 RGB patch using the exact
// spec taken from the loaded graph (name, dims, dtype, quantization).
status vqec_vision_ai_appl_cscrd_quantize(
    const alignment_result& _aligned, const tensor_spec& _target,
    const std::array<float, 3>& _offset, const std::array<float, 3>& _scale,
    std::vector<std::uint16_t>& _workspace, tensor_blob& _blob) {
    const auto& spec = _aligned.tensor_.spec_;
    if (spec.dtype_ != tensor_element_type::uint8 || spec.dimensions_.size() != 4U ||
        spec.dimensions_[0] != 1U || spec.dimensions_[3] != 3U) {
        return {status_code::unsupported, "aligned tensor is not a uint8 RGB patch"};
    }
    const std::uint32_t width = spec.dimensions_[2];
    const std::uint32_t height = spec.dimensions_[1];
    const std::size_t expected = static_cast<std::size_t>(width) * height * 3U;
    if (_aligned.tensor_.bytes_.size() != expected) {
        return {status_code::unsupported, "aligned tensor byte count is inconsistent"};
    }
    if (_target.dtype_ != tensor_element_type::uint16 ||
        _target.dimensions_.size() != 4U || _target.dimensions_[0] != 1U ||
        _target.dimensions_[1] != height || _target.dimensions_[2] != width ||
        _target.dimensions_[3] != 3U) {
        return {status_code::invalid_argument,
            "aligned patch does not match the model input spec"};
    }
    if (_workspace.size() != expected || _blob.spec_.name_ != _target.name_ ||
        _blob.bytes_.size() != expected * sizeof(std::uint16_t)) {
        return {status_code::invalid_state,
            "embedding input workspace does not match the loaded graph"};
    }
    const auto converted = vqec_vision_ai_core_color_quantize_rgb8_to_uint16(
        _aligned.tensor_.bytes_.data(), width, height, width * 3U, _offset, _scale,
        _target.quantization_.scale_, _target.quantization_.zero_point_, _workspace.data());
    if (converted.code_ != status_code::ok) {
        return converted;
    }
    std::memcpy(_blob.bytes_.data(), _workspace.data(), _blob.bytes_.size());
    return {};
}

}  // namespace

status cascade_coordinator::vqec_vision_ai_appl_cscrd_configure(
    const cascade_coordinator_config& _config) {
    if (is_configured_) {
        return {status_code::invalid_state, "cascade coordinator is already configured"};
    }
    if (_config.aligner_ == nullptr ||
        _config.max_tasks_per_frame_ == 0 ||
        _config.max_tasks_per_frame_ > image_alignment_limits::g_max_points) {
        return {status_code::invalid_argument, "invalid cascade coordinator configuration"};
    }
    const bool wants_embedding =
        _config.embedding_graph_ != nullptr || _config.embedding_decoder_ != nullptr;
    if (wants_embedding &&
        (_config.embedding_graph_ == nullptr || _config.embedding_decoder_ == nullptr ||
         _config.cycle_id_ == 0 || _config.job_timeout_ns_ == 0 ||
         _config.job_timeout_ns_ == std::numeric_limits<std::uint64_t>::max())) {
        return {status_code::invalid_argument, "invalid secondary embedding configuration"};
    }
    alignment_capabilities capabilities;
    const auto probed = _config.aligner_->vqec_vision_ai_ports_imaln_probe_capabilities(
        capabilities);
    if (probed.code_ != status_code::ok) {
        return probed;
    }
    const auto checked = _config.aligner_->vqec_vision_ai_ports_imaln_validate_template(
        _config.template_, capabilities);
    if (checked.code_ != status_code::ok) {
        return checked;
    }
    tensor_spec input_spec;
    if (wants_embedding) {
        // This coordinator drives a synchronous secondary backend only; an asynchronous
        // backend must be validated with a state machine that holds pending tasks across
        // steps, which is not implemented.
        const auto backend_capabilities =
            _config.embedding_graph_->vqec_vision_ai_ports_infgr_get_capabilities();
        if (backend_capabilities.supports_async_ ||
            backend_capabilities.max_inflight_jobs_ != 1U) {
            return {status_code::unsupported,
                "cascade coordinator requires a synchronous single-inflight embedding graph"};
        }
        std::vector<tensor_spec> inputs;
        const auto specs =
            _config.embedding_graph_->vqec_vision_ai_ports_infgr_get_input_specs(inputs);
        if (specs.code_ != status_code::ok || inputs.size() != 1U) {
            return {status_code::unsupported,
                "embedding graph must expose exactly one input tensor spec"};
        }
        input_spec = inputs[0];
        if (input_spec.dtype_ != tensor_element_type::uint16 ||
            input_spec.dimensions_.size() != 4U || input_spec.dimensions_[0] != 1U ||
            input_spec.dimensions_[1] != _config.template_.destination_height_ ||
            input_spec.dimensions_[2] != _config.template_.destination_width_ ||
            input_spec.dimensions_[3] != 3U ||
            !input_spec.quantization_.is_quantized_ ||
            !std::isfinite(input_spec.quantization_.scale_) ||
            !(input_spec.quantization_.scale_ > 0.0F)) {
            return {status_code::invalid_argument,
                "embedding graph input spec does not match the alignment template"};
        }
    }
    aligner_ = _config.aligner_;
    lease_ = _config.lease_;
    embedding_graph_ = _config.embedding_graph_;
    embedding_decoder_ = _config.embedding_decoder_;
    template_ = _config.template_;
    capabilities_ = capabilities;
    normalize_offset_ = _config.normalize_offset_;
    normalize_scale_ = _config.normalize_scale_;
    cycle_id_ = _config.cycle_id_;
    job_timeout_ns_ = _config.job_timeout_ns_;
    max_tasks_per_frame_ = _config.max_tasks_per_frame_;
    embedding_input_spec_ = input_spec;
    if (wants_embedding) {
        const auto elements = static_cast<std::size_t>(input_spec.dimensions_[1]) *
            input_spec.dimensions_[2] * input_spec.dimensions_[3];
        try {
            embedding_quantized_workspace_.assign(elements, 0U);
            embedding_inputs_.assign(1U, tensor_blob{});
            embedding_inputs_[0].spec_ = input_spec;
            embedding_inputs_[0].bytes_.assign(
                elements * sizeof(std::uint16_t), 0U);
        } catch (const std::bad_alloc&) {
            embedding_quantized_workspace_.clear();
            embedding_inputs_.clear();
            return {status_code::resource_exhausted,
                "cannot allocate embedding input workspace"};
        }
    }
    has_embedding_input_spec_ = wants_embedding;
    is_configured_ = true;
    return {};
}

bool cascade_coordinator::vqec_vision_ai_appl_cscrd_is_configured() const noexcept {
    return is_configured_;
}

const status& cascade_coordinator::
vqec_vision_ai_appl_cscrd_get_last_task_error() const noexcept {
    return last_task_error_;
}

status cascade_coordinator::vqec_vision_ai_appl_cscrd_process(
    std::uint64_t _steady_now_ns, const observation_batch& _tracked,
    std::vector<alignment_result>& _aligned, std::vector<embedding_result>& _embeddings,
    cascade_coordinator_report& _report) {
    if (lease_ == nullptr) {
        return {status_code::invalid_state, "cascade coordinator has no live frame lease"};
    }
    return vqec_vision_ai_appl_cscrd_process_with_lease(
        _steady_now_ns, *lease_, _tracked, _aligned, _embeddings, _report);
}

status cascade_coordinator::vqec_vision_ai_appl_cscrd_process_frame(
    std::uint64_t _steady_now_ns, const raw_frame& _frame,
    const observation_batch& _tracked, std::vector<alignment_result>& _aligned,
    std::vector<embedding_result>& _embeddings, cascade_coordinator_report& _report) {
    if (!is_configured_) {
        return {status_code::invalid_state, "cascade coordinator is not configured"};
    }
    if (!_frame.owner_ || _frame.descriptor_.session_epoch_ != _tracked.frame_.source_epoch_ ||
        _frame.descriptor_.buffer_id_ != _tracked.frame_.frame_id_ ||
        _frame.descriptor_.pts_ns_ != _tracked.frame_.source_pts_ns_ ||
        _frame.descriptor_.width_ != _tracked.geometry_.width_ ||
        _frame.descriptor_.height_ != _tracked.geometry_.height_) {
        return {status_code::invalid_argument, "direct cascade frame identity is invalid"};
    }
    direct_frame_lease lease(_frame, _tracked.frame_);
    return vqec_vision_ai_appl_cscrd_process_with_lease(
        _steady_now_ns, lease, _tracked, _aligned, _embeddings, _report);
}

status cascade_coordinator::vqec_vision_ai_ports_ficas_run(
    const raw_frame& _frame, const observation_batch& _detections,
    std::uint64_t _steady_now_ns, std::vector<embedding_result>& _embeddings,
    std::size_t& _failed_tasks) {
    std::vector<alignment_result> aligned;
    cascade_coordinator_report report;
    const auto result = vqec_vision_ai_appl_cscrd_process_frame(
        _steady_now_ns, _frame, _detections, aligned, _embeddings, report);
    if (result.code_ != status_code::ok) {
        return result;
    }
    _failed_tasks = report.failed_;
    return report.failed_ == 0 ? status{} : last_task_error_;
}

status cascade_coordinator::vqec_vision_ai_appl_cscrd_process_with_lease(
    std::uint64_t _steady_now_ns, cascade_frame_lease_port& _lease,
    const observation_batch& _tracked, std::vector<alignment_result>& _aligned,
    std::vector<embedding_result>& _embeddings, cascade_coordinator_report& _report) {
    _aligned.clear();
    _embeddings.clear();
    _report = {};
    last_task_error_ = {};
    if (!is_configured_) {
        return {status_code::invalid_state, "cascade coordinator is not configured"};
    }
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::invalid_argument, "cascade coordinator requires steady time"};
    }
    const preview_frame_key& key = _tracked.frame_;
    if (key.source_epoch_ == 0 || key.frame_id_ == 0) {
        return {status_code::invalid_argument, "cascade batch has no source frame identity"};
    }
    const bool embed = embedding_graph_ != nullptr;
    if (embed && armed_source_epoch_ != 0 && armed_source_epoch_ != key.source_epoch_) {
        (void)_lease.vqec_vision_ai_ports_cflse_retire(key);
        return {status_code::invalid_state,
            "secondary graph must restart before the source epoch changes"};
    }
    std::size_t accepted = 0;
    for (const auto& observation : _tracked.observations_) {
        if (accepted >= max_tasks_per_frame_ ||
            observation.landmarks_.points_.empty() ||
            observation.landmarks_.schema_id_.empty()) {
            ++_report.skipped_;
            continue;
        }
        alignment_request request;
        request.frame_ = key;
        request.landmarks_ = observation.landmarks_;
        raw_frame frame;
        std::uint64_t frame_ticket = 0;
        const auto acquired =
            _lease.vqec_vision_ai_ports_cflse_acquire(key, frame, frame_ticket);
        if (acquired.code_ != status_code::ok) {
            if (last_task_error_.code_ == status_code::ok) {
                last_task_error_ = acquired;
            }
            ++_report.failed_;
            continue;
        }
        alignment_result result;
        std::uint64_t align_ticket = 0;
        const auto aligned = aligner_->vqec_vision_ai_ports_imaln_align(
            request, frame, template_, result, align_ticket);
        bool task_ok = aligned.code_ == status_code::ok;
        if (!task_ok && last_task_error_.code_ == status_code::ok) {
            last_task_error_ = aligned;
        }
        // The alignment transform is not complete until the backend reports device
        // completion; a pending transform is a task failure, not a successful crop.
        if (task_ok) {
            bool complete = false;
            const auto completion =
                aligner_->vqec_vision_ai_ports_imaln_poll_completion(align_ticket, complete);
            if (completion.code_ != status_code::ok || !complete) {
                task_ok = false;
                if (last_task_error_.code_ == status_code::ok) {
                    last_task_error_ = completion.code_ != status_code::ok ? completion :
                        status{status_code::protocol_error,
                            "alignment completion was not signalled"};
                }
            }
        }
        bool pushed_aligned = false;
        if (task_ok) {
            _aligned.push_back(std::move(result));
            ++accepted;
            pushed_aligned = true;
        }
        bool pushed_embedding = false;
        if (task_ok && embed) {
            if (armed_source_epoch_ == 0) {
                const auto armed = embedding_graph_->vqec_vision_ai_ports_infgr_arm(
                    cycle_id_, key.source_epoch_, job_timeout_ns_,
                    submission_sequence_policy::repeated_tasks_per_source_frame);
                if (armed.code_ == status_code::ok) {
                    armed_source_epoch_ = key.source_epoch_;
                } else {
                    task_ok = false;
                    if (last_task_error_.code_ == status_code::ok) {
                        last_task_error_ = armed;
                    }
                }
            }
            if (task_ok) {
                const auto quantized = vqec_vision_ai_appl_cscrd_quantize(
                    _aligned.back(), embedding_input_spec_, normalize_offset_, normalize_scale_,
                    embedding_quantized_workspace_, embedding_inputs_[0]);
                if (quantized.code_ != status_code::ok) {
                    task_ok = false;
                    if (last_task_error_.code_ == status_code::ok) {
                        last_task_error_ = quantized;
                    }
                }
            }
            if (task_ok) {
                submission_ticket secondary_ticket;
                const auto submitted =
                    embedding_graph_->vqec_vision_ai_ports_infgr_submit_tensors(
                        key.source_epoch_, key.frame_id_, key.source_pts_ns_, embedding_inputs_,
                        _steady_now_ns, secondary_ticket);
                if (submitted.code_ != status_code::ok) {
                    task_ok = false;
                    if (last_task_error_.code_ == status_code::ok) {
                        last_task_error_ = submitted;
                    }
                } else {
                    tensor_result tensor;
                    embedding_result embedding;
                    const auto polled =
                        embedding_graph_->vqec_vision_ai_ports_infgr_poll_result(
                            _steady_now_ns, tensor);
                    const auto decoded = polled.code_ == status_code::ok ?
                        embedding_decoder_->vqec_vision_ai_ports_embdec_decode(
                            tensor, key, observation.track_id_, embedding) : polled;
                    if (decoded.code_ == status_code::ok) {
                        _embeddings.push_back(std::move(embedding));
                        ++_report.embedded_;
                        pushed_embedding = true;
                    } else {
                        task_ok = false;
                        if (last_task_error_.code_ == status_code::ok) {
                            last_task_error_ = decoded;
                        }
                    }
                }
            }
        }
        // Release the frame-retention ticket regardless of downstream success; it is a
        // different ticket from the alignment-completion ticket above.
        const bool complete_ok =
            _lease.vqec_vision_ai_ports_cflse_complete(frame_ticket).code_ == status_code::ok;
        if (task_ok && complete_ok) {
            ++_report.accepted_;
        } else {
            if (!complete_ok && last_task_error_.code_ == status_code::ok) {
                last_task_error_ = {status_code::invalid_state,
                    "cascade frame lease completion failed"};
            }
            ++_report.failed_;
            if (pushed_aligned) {
                _aligned.pop_back();
                --accepted;
            }
            if (pushed_embedding) {
                _embeddings.pop_back();
                --_report.embedded_;
            }
        }
    }
    const auto retired = _lease.vqec_vision_ai_ports_cflse_retire(key);
    if (retired.code_ != status_code::ok && retired.code_ != status_code::invalid_state) {
        if (last_task_error_.code_ == status_code::ok) {
            last_task_error_ = retired;
        }
        ++_report.failed_;
    }
    return {};
}

}  // namespace vqec::vision::ai
