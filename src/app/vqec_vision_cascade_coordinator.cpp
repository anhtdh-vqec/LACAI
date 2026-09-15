#include "vqec_vision_cascade_coordinator.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_color.hpp"

namespace vqec::vision::ai {
namespace {

// Builds the quantized model input tensor from an aligned uint8 RGB patch using the exact
// spec taken from the loaded graph (name, dims, dtype, quantization).
status vqec_vision_ai_appl_cscrd_quantize(
    const alignment_result& _aligned, const tensor_spec& _target,
    const std::array<float, 3>& _offset, const std::array<float, 3>& _scale,
    float _quant_scale, std::int32_t _quant_zero_point, tensor_blob& _blob) {
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
    std::vector<std::uint16_t> quantized(expected, 0U);
    const auto converted = vqec_vision_ai_core_color_quantize_rgb8_to_uint16(
        _aligned.tensor_.bytes_.data(), width, height, width * 3U, _offset, _scale,
        _quant_scale, _quant_zero_point, quantized.data());
    if (converted.code_ != status_code::ok) {
        return converted;
    }
    tensor_blob blob;
    blob.spec_ = _target;
    blob.bytes_.resize(quantized.size() * sizeof(std::uint16_t));
    std::memcpy(blob.bytes_.data(), quantized.data(), blob.bytes_.size());
    _blob = std::move(blob);
    return {};
}

}  // namespace

status cascade_coordinator::vqec_vision_ai_appl_cscrd_configure(
    const cascade_coordinator_config& _config) {
    if (is_configured_) {
        return {status_code::invalid_state, "cascade coordinator is already configured"};
    }
    if (_config.aligner_ == nullptr || _config.lease_ == nullptr ||
        _config.max_tasks_per_frame_ == 0 ||
        _config.max_tasks_per_frame_ > image_alignment_limits::g_max_points) {
        return {status_code::invalid_argument, "invalid cascade coordinator configuration"};
    }
    const bool wants_embedding =
        _config.embedding_graph_ != nullptr || _config.embedding_decoder_ != nullptr;
    if (wants_embedding &&
        (_config.embedding_graph_ == nullptr || _config.embedding_decoder_ == nullptr ||
         !std::isfinite(_config.quant_scale_) || !(_config.quant_scale_ > 0.0F) ||
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
            input_spec.dimensions_[3] != 3U) {
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
    quant_scale_ = _config.quant_scale_;
    quant_zero_point_ = _config.quant_zero_point_;
    cycle_id_ = _config.cycle_id_;
    job_timeout_ns_ = _config.job_timeout_ns_;
    max_tasks_per_frame_ = _config.max_tasks_per_frame_;
    embedding_input_spec_ = input_spec;
    has_embedding_input_spec_ = wants_embedding;
    is_configured_ = true;
    return {};
}

bool cascade_coordinator::vqec_vision_ai_appl_cscrd_is_configured() const noexcept {
    return is_configured_;
}

status cascade_coordinator::vqec_vision_ai_appl_cscrd_process(
    std::uint64_t _steady_now_ns, const observation_batch& _tracked,
    std::vector<alignment_result>& _aligned, std::vector<embedding_result>& _embeddings,
    cascade_coordinator_report& _report) {
    _aligned.clear();
    _embeddings.clear();
    _report = {};
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
        (void)lease_->vqec_vision_ai_ports_cflse_retire(key);
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
            lease_->vqec_vision_ai_ports_cflse_acquire(key, frame, frame_ticket);
        if (acquired.code_ != status_code::ok) {
            ++_report.failed_;
            continue;
        }
        alignment_result result;
        std::uint64_t align_ticket = 0;
        const auto aligned = aligner_->vqec_vision_ai_ports_imaln_align(
            request, frame, template_, result, align_ticket);
        bool task_ok = aligned.code_ == status_code::ok;
        // The alignment transform is not complete until the backend reports device
        // completion; a pending transform is a task failure, not a successful crop.
        if (task_ok) {
            bool complete = false;
            if (aligner_->vqec_vision_ai_ports_imaln_poll_completion(align_ticket, complete)
                    .code_ != status_code::ok ||
                !complete) {
                task_ok = false;
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
                    cycle_id_, key.source_epoch_, job_timeout_ns_);
                if (armed.code_ == status_code::ok) {
                    armed_source_epoch_ = key.source_epoch_;
                } else {
                    task_ok = false;
                }
            }
            tensor_blob input;
            if (task_ok) {
                const auto quantized = vqec_vision_ai_appl_cscrd_quantize(
                    _aligned.back(), embedding_input_spec_, normalize_offset_, normalize_scale_,
                    quant_scale_, quant_zero_point_, input);
                if (quantized.code_ != status_code::ok) {
                    task_ok = false;
                }
            }
            if (task_ok) {
                submission_ticket secondary_ticket;
                const auto submitted =
                    embedding_graph_->vqec_vision_ai_ports_infgr_submit_tensors(
                        key.source_epoch_, key.frame_id_, key.source_pts_ns_, {input},
                        _steady_now_ns, secondary_ticket);
                if (submitted.code_ != status_code::ok) {
                    task_ok = false;
                } else {
                    tensor_result tensor;
                    embedding_result embedding;
                    const auto polled =
                        embedding_graph_->vqec_vision_ai_ports_infgr_poll_result(
                            _steady_now_ns, tensor);
                    if (polled.code_ == status_code::ok &&
                        embedding_decoder_
                                ->vqec_vision_ai_ports_embdec_decode(
                                    tensor, key, observation.track_id_, embedding)
                                .code_ == status_code::ok) {
                        _embeddings.push_back(std::move(embedding));
                        ++_report.embedded_;
                        pushed_embedding = true;
                    } else {
                        task_ok = false;
                    }
                }
            }
        }
        // Release the frame-retention ticket regardless of downstream success; it is a
        // different ticket from the alignment-completion ticket above.
        const bool complete_ok =
            lease_->vqec_vision_ai_ports_cflse_complete(frame_ticket).code_ == status_code::ok;
        if (task_ok && complete_ok) {
            ++_report.accepted_;
        } else {
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
    const auto retired = lease_->vqec_vision_ai_ports_cflse_retire(key);
    if (retired.code_ != status_code::ok && retired.code_ != status_code::invalid_state) {
        ++_report.failed_;
    }
    return {};
}

}  // namespace vqec::vision::ai
