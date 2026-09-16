#include <cassert>
#include <memory>

#include "vqec_vision_face_enrollment_image_pipeline.hpp"

using namespace vqec::vision::ai;

namespace {
class test_controller final : public face_enrollment_port {
public:
    status vqec_vision_ai_ports_fenrl_begin(
        const face_enrollment_begin_request& _request,
        face_enrollment_status& _status) override {
        request_ = _request;
        status_ = {_request.request_id_, _request.subject_ref_,
            face_enrollment_state::collecting, 0, _request.expected_samples_,
            _request.expected_gallery_revision_, status_code::ok};
        _status = status_;
        return {};
    }
    status vqec_vision_ai_ports_fenrl_begin_image(
        const face_enrollment_begin_request& _request,
        face_enrollment_status& _status) override {
        return vqec_vision_ai_ports_fenrl_begin(_request, _status);
    }
    status vqec_vision_ai_ports_fenrl_cancel(
        const std::string&, face_enrollment_status& _status) override {
        status_.state_ = face_enrollment_state::cancelled;
        _status = status_;
        return {};
    }
    status vqec_vision_ai_ports_fenrl_remove_subject(
        const std::string&, std::uint64_t, std::uint64_t&) override { return {}; }
    status vqec_vision_ai_ports_fenrl_get_status(
        const std::string&, face_enrollment_status& _status) const override {
        _status = status_;
        return {};
    }
    status vqec_vision_ai_ports_fenrl_fail(
        const std::string&, status_code _error, face_enrollment_status& _status) override {
        status_.state_ = face_enrollment_state::failed;
        status_.last_error_ = _error;
        _status = status_;
        return {};
    }
    status vqec_vision_ai_ports_fenrl_accept_embedding(
        const embedding_result&, face_enrollment_status&) override { return {}; }
    status vqec_vision_ai_ports_fenrl_accept_batch(
        const std::string& _source_id, const std::vector<embedding_result>& _embeddings,
        std::size_t _eligible_face_count, face_enrollment_status& _status) override {
        if (_source_id != request_.source_id_ || _eligible_face_count != 1 ||
            _embeddings.size() != 1) return {status_code::invalid_argument, "bad batch"};
        status_.accepted_samples_ = 1;
        status_.state_ = face_enrollment_state::completed;
        _status = status_;
        return {};
    }
    face_enrollment_begin_request request_;
    face_enrollment_status status_;
};

class test_authorizer final : public image_path_authorizer_port {
public:
    status vqec_vision_ai_ports_ipath_authorize(
        const std::string& _requested_path, std::string& _authorized_path) override {
        if (deny_) return {status_code::unauthorized, "denied"};
        _authorized_path = _requested_path;
        return {};
    }
    bool deny_{false};
};

class test_source final : public face_enrollment_image_source_port {
public:
    status vqec_vision_ai_ports_feimg_load(
        const face_enrollment_image_request& _request,
        face_enrollment_image& _image) override {
        _image.frame_.descriptor_.buffer_id_ = _request.buffer_id_;
        _image.frame_.descriptor_.session_epoch_ = _request.session_epoch_;
        _image.frame_.descriptor_.pts_ns_ = _request.source_pts_ns_;
        _image.frame_.descriptor_.width_ = _request.width_;
        _image.frame_.descriptor_.height_ = _request.height_;
        _image.frame_.owner_ = std::make_shared<int>(1);
        return {};
    }
};

class test_detector final : public face_image_detector_port {
public:
    status vqec_vision_ai_ports_fidet_run(
        const raw_frame& _frame, std::uint64_t,
        observation_batch& _detections) override {
        _detections.frame_ = {2, 3, _frame.descriptor_.session_epoch_,
            _frame.descriptor_.buffer_id_, _frame.descriptor_.pts_ns_};
        _detections.geometry_ = {_frame.descriptor_.width_, _frame.descriptor_.height_};
        observation face;
        face.frame_ = _detections.frame_;
        face.class_id_ = "face";
        face.confidence_ = 1.0F;
        face.box_ = {1, 1, 10, 10, 0xffffffffU, "face"};
        face.landmarks_.schema_id_ = "face.5pt";
        face.landmarks_.schema_version_ = "1";
        face.landmarks_.points_ = {{2, 2}};
        _detections.observations_ = {std::move(face)};
        return {};
    }
};

class test_cascade final : public face_image_cascade_port {
public:
    status vqec_vision_ai_ports_ficas_run(
        const raw_frame&, const observation_batch& _detections, std::uint64_t,
        std::vector<embedding_result>& _embeddings,
        std::size_t& _failed_tasks) override {
        embedding_result embedding;
        embedding.frame_ = _detections.frame_;
        embedding.model_id_ = "face.embedding";
        embedding.model_version_ = "1";
        embedding.values_ = {1.0F, 0.0F};
        embedding.is_l2_normalized_ = true;
        _embeddings = {std::move(embedding)};
        _failed_tasks = 0;
        return {};
    }
};
}

int main() {
    test_controller controller;
    test_authorizer authorizer;
    test_source source;
    test_detector detector;
    test_cascade cascade;
    face_enrollment_image_pipeline pipeline;
    const face_enrollment_image_pipeline_config config{
        &controller, &authorizer, &source, &detector, &cascade, {64, 48}, 9};
    assert(pipeline.vqec_vision_ai_appl_feipl_configure(config).code_ == status_code::ok);
    face_enrollment_begin_request request{
        "request-1", "subject-1", "/authorized/face.jpg", "file", 2, 3, 0, 1, 1};
    face_enrollment_status status;
    assert(pipeline.vqec_vision_ai_ports_fenrl_begin(request, status).code_ == status_code::ok);
    assert(status.state_ == face_enrollment_state::collecting &&
        pipeline.vqec_vision_ai_appl_feipl_has_pending());
    assert(pipeline.vqec_vision_ai_appl_feipl_step(100).code_ == status_code::ok);
    assert(!pipeline.vqec_vision_ai_appl_feipl_has_pending());
    assert(pipeline.vqec_vision_ai_ports_fenrl_get_status(request.request_id_, status).code_ ==
        status_code::ok);
    assert(status.state_ == face_enrollment_state::completed && status.accepted_samples_ == 1);

    request.request_id_ = "request-2";
    request.expected_gallery_revision_ = 2;
    authorizer.deny_ = true;
    assert(pipeline.vqec_vision_ai_ports_fenrl_begin(request, status).code_ == status_code::ok);
    assert(pipeline.vqec_vision_ai_appl_feipl_step(101).code_ == status_code::unauthorized);
    assert(status.state_ == face_enrollment_state::collecting);
    assert(pipeline.vqec_vision_ai_ports_fenrl_get_status(request.request_id_, status).code_ ==
        status_code::ok);
    assert(status.state_ == face_enrollment_state::failed &&
        status.last_error_ == status_code::unauthorized);
    return 0;
}
