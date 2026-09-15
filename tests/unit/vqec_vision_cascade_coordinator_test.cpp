// Device-free tests for the cascade coordinator: bounded per-frame task admission, exact
// acquire/complete/retire ordering, align-only and align+embedding pipelines, and per-task
// fault isolation.

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "vqec_vision_cascade_coordinator.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_test_cycle_id = 1U;
constexpr std::uint64_t g_test_job_timeout_ns = 1000000000U;

class fake_lease final : public cascade_frame_lease_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_cflse_acquire(
        const preview_frame_key&, raw_frame& _frame, std::uint64_t& _ticket) override {
        ++acquire_calls_;
        if (fail_acquire_) {
            return {status_code::resource_exhausted, "fixture store is full"};
        }
        _frame.owner_ = std::make_shared<int>(0);
        _frame.native_handle_ = 1;
        _frame.descriptor_.allocation_size_bytes_ = 16;
        _ticket = next_ticket_++;
        issued_.push_back(_ticket);
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_cflse_retire(
        const preview_frame_key&) override {
        ++retire_calls_;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_cflse_complete(std::uint64_t _ticket) override {
        for (auto iterator = issued_.begin(); iterator != issued_.end(); ++iterator) {
            if (*iterator == _ticket) {
                issued_.erase(iterator);
                ++complete_calls_;
                return {};
            }
        }
        return {status_code::invalid_state, "fixture ticket was never issued"};
    }

    unsigned acquire_calls_{0};
    unsigned retire_calls_{0};
    unsigned complete_calls_{0};
    std::uint64_t next_ticket_{100};
    std::vector<std::uint64_t> issued_;
    bool fail_acquire_{false};
};

class fake_aligner final : public image_alignment_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_imaln_probe_capabilities(
        alignment_capabilities& _capabilities) const override {
        _capabilities.supports_similarity_ = true;
        _capabilities.max_points_ = image_alignment_limits::g_max_points;
        _capabilities.max_destination_dimension_ = 112;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_validate_template(
        const alignment_template& _template,
        const alignment_capabilities& _capabilities) const override {
        return vqec_vision_ai_core_imaln_require_capability(_capabilities, _template);
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_align(
        const alignment_request& _request, const raw_frame&, const alignment_template&,
        alignment_result& _result, std::uint64_t& _ticket) override {
        if (fail_) {
            return {status_code::io_error, "fixture warp failed"};
        }
        if (vqec_vision_ai_core_imaln_validate_request(_request, template_).code_ !=
            status_code::ok) {
            return {status_code::invalid_argument, "fixture request invalid"};
        }
        _result.tensor_.spec_.dimensions_ = {1U, template_.destination_height_,
            template_.destination_width_, 3U};
        _result.tensor_.spec_.dtype_ = tensor_element_type::uint8;
        _result.tensor_.bytes_.assign(
            static_cast<std::size_t>(template_.destination_width_) *
                template_.destination_height_ * 3U,
            10U);
        _result.has_transform_ = true;
        _ticket = 1;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_poll_completion(
        std::uint64_t, bool& _complete) override {
        _complete = !completion_pending_;
        return {};
    }

    alignment_template template_;
    bool fail_{false};
    bool completion_pending_{false};
};

class fake_embedding_graph final : public inference_graph_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_infgr_validate_activation() const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_configure(
        const inference_plan&) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_load() override { return {}; }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_state() override { return {}; }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_bind_source(
        const source_binding&) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_start(
        const std::vector<tensor_spec>&, std::uint64_t) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t, std::uint64_t, std::uint64_t) override {
        ++arm_calls_;
        if (fail_arm_) {
            return {status_code::io_error, "fixture arm failed"};
        }
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_frame(
        const raw_frame&, std::uint64_t, submission_ticket&) override {
        return {status_code::unsupported, "fixture does not take raw frames"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_tensors(
        std::uint64_t, std::uint64_t, std::uint64_t, const std::vector<tensor_blob>&,
        std::uint64_t, submission_ticket& _ticket) override {
        ++submit_calls_;
        _ticket = {};
        _ticket.token_.job_id_ = 1;
        is_outstanding_ = true;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t, tensor_result& _result) override {
        if (!is_outstanding_) {
            return {status_code::pending, "fixture has no result"};
        }
        is_outstanding_ = false;
        tensor_blob blob;
        blob.spec_.name_ = "embedding";
        blob.spec_.dtype_ = tensor_element_type::float32;
        blob.spec_.dimensions_ = {1U, 4U};
        const float values[4] = {3.0F, 4.0F, 0.0F, 0.0F};
        blob.bytes_.resize(sizeof(values));
        std::memcpy(blob.bytes_.data(), values, sizeof(values));
        _result.tensors_.push_back(std::move(blob));
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_request_drain() override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_unload() override { return {}; }
    [[nodiscard]] inference_graph_state vqec_vision_ai_ports_infgr_get_state() const noexcept
        override {
        return inference_graph_state::running;
    }
    [[nodiscard]] unsigned vqec_vision_ai_ports_infgr_get_outstanding() const noexcept
        override {
        return is_outstanding_ ? 1U : 0U;
    }
    [[nodiscard]] submission_ticket vqec_vision_ai_ports_infgr_get_pending_ticket()
        const noexcept override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_get_input_specs(
        std::vector<tensor_spec>& _inputs) const override {
        _inputs.clear();
        _inputs.push_back({"input", {1U, 112U, 112U, 3U}, tensor_element_type::uint16,
            {true, 3.05180438e-05F, 32768}});
        return {};
    }

    unsigned submit_calls_{0};
    unsigned arm_calls_{0};
    bool is_outstanding_{false};
    bool fail_arm_{false};
};

class fake_embedding_decoder final : public embedding_decoder_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_embdec_validate(
        const model_outputs&) const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_embdec_decode(
        const tensor_result&, const preview_frame_key& _frame, std::uint64_t _track_id,
        embedding_result& _embedding) override {
        _embedding.frame_ = _frame;
        _embedding.track_id_ = _track_id;
        _embedding.model_id_ = "fixture";
        _embedding.model_version_ = "1";
        _embedding.values_ = {0.6F, 0.8F, 0.0F, 0.0F};
        _embedding.is_l2_normalized_ = true;
        return {};
    }
};

alignment_template make_template() {
    alignment_template value;
    value.schema_id_ = "face.5pt";
    value.schema_version_ = "1";
    value.destination_width_ = 112;
    value.destination_height_ = 112;
    value.reference_points_ = {{0.0F, 0.0F}, {112.0F, 0.0F}, {112.0F, 112.0F},
        {0.0F, 112.0F}};
    return value;
}

observation_batch make_batch(unsigned _count) {
    observation_batch batch;
    batch.frame_ = {0U, 0U, 1U, 1U, 1000U};
    for (unsigned index = 0; index < _count; ++index) {
        observation item;
        item.frame_ = batch.frame_;
        item.track_id_ = 100U + index;
        item.landmarks_.schema_id_ = "face.5pt";
        item.landmarks_.schema_version_ = "1";
        item.landmarks_.points_ = {{0.0F, 0.0F}, {112.0F, 0.0F}, {112.0F, 112.0F},
            {0.0F, 112.0F}};
        batch.observations_.push_back(std::move(item));
    }
    return batch;
}

cascade_coordinator_config make_config(
    fake_aligner& _aligner, fake_lease& _lease, std::size_t _max_tasks) {
    cascade_coordinator_config config;
    config.aligner_ = &_aligner;
    config.lease_ = &_lease;
    config.template_ = make_template();
    config.max_tasks_per_frame_ = _max_tasks;
    _aligner.template_ = config.template_;
    return config;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    std::vector<alignment_result> aligned;
    std::vector<embedding_result> embeddings;
    cascade_coordinator_report report;

    // Two of three faces are admitted; each acquires, completes, and admission is retired.
    {
        fake_aligner aligner;
        fake_lease lease;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 2)).code_ == status_code::ok);
        const auto batch = make_batch(3);
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  0, batch, aligned, embeddings, report).code_ == status_code::ok);
        check(report.accepted_ == 2 && report.skipped_ == 1 && report.failed_ == 0);
        check(aligned.size() == 2 && embeddings.empty() && lease.acquire_calls_ == 2 &&
            lease.complete_calls_ == 2 && lease.retire_calls_ == 1);
    }

    // Unconfigured fails closed.
    {
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  0, make_batch(1), aligned, embeddings, report).code_ ==
            status_code::invalid_state);
    }

    // Align failure is isolated, the ticket is completed and admission retired.
    {
        fake_aligner aligner;
        aligner.fail_ = true;
        fake_lease lease;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 2)).code_ == status_code::ok);
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  0, make_batch(2), aligned, embeddings, report).code_ == status_code::ok);
        check(report.accepted_ == 0 && report.failed_ == 2 && aligned.empty() &&
            lease.complete_calls_ == 2 && lease.retire_calls_ == 1);
    }

    // Acquire failure is counted and no ticket is completed; admission is still retired.
    {
        fake_aligner aligner;
        fake_lease lease;
        lease.fail_acquire_ = true;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 2)).code_ == status_code::ok);
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  0, make_batch(2), aligned, embeddings, report).code_ == status_code::ok);
        check(report.accepted_ == 0 && report.failed_ == 2 && lease.complete_calls_ == 0 &&
            lease.retire_calls_ == 1);
    }

    // A pending alignment completion fails the task but still releases the frame ticket.
    {
        fake_aligner aligner;
        aligner.completion_pending_ = true;
        fake_lease lease;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 2)).code_ == status_code::ok);
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  0, make_batch(1), aligned, embeddings, report).code_ == status_code::ok);
        check(report.accepted_ == 0 && report.failed_ == 1 && aligned.empty() &&
            lease.complete_calls_ == 1 && lease.retire_calls_ == 1);
    }

    // Align + embedding: the aligned face is quantized, submitted, polled and decoded.
    {
        fake_aligner aligner;
        fake_lease lease;
        fake_embedding_graph graph;
        fake_embedding_decoder decoder;
        auto config = make_config(aligner, lease, 2);
        config.embedding_graph_ = &graph;
        config.embedding_decoder_ = &decoder;
        config.normalize_offset_ = {127.5F, 127.5F, 127.5F};
        config.normalize_scale_ = {
            1.0F / 127.5F, 1.0F / 127.5F, 1.0F / 127.5F};
        config.cycle_id_ = g_test_cycle_id;
        config.job_timeout_ns_ = g_test_job_timeout_ns;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(config).code_ == status_code::ok);
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  0, make_batch(2), aligned, embeddings, report).code_ == status_code::ok);
        check(report.accepted_ == 2 && report.embedded_ == 2 && report.failed_ == 0 &&
            aligned.size() == 2 && embeddings.size() == 2 && graph.arm_calls_ == 1 &&
            graph.submit_calls_ == 2 &&
            embeddings[0].track_id_ == 100U && embeddings[0].is_l2_normalized_);
    }

    // A secondary graph without a decoder is rejected at configuration.
    {
        fake_aligner aligner;
        fake_lease lease;
        fake_embedding_graph graph;
        auto config = make_config(aligner, lease, 2);
        config.embedding_graph_ = &graph;
        config.cycle_id_ = g_test_cycle_id;
        config.job_timeout_ns_ = g_test_job_timeout_ns;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(config).code_ ==
            status_code::invalid_argument);
    }

    // A zero task budget is rejected at configuration.
    {
        fake_aligner aligner;
        fake_lease lease;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 0)).code_ == status_code::invalid_argument);
    }

    std::cout << "cascade coordinator failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
