#include <cmath>
#include <iostream>

#include "vqec_vision_exact_embedding_index.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    exact_embedding_index index;
    embedding_index_config config;
    config.model_id_ = "face_embedding";
    config.model_version_ = "1.0";
    config.dimensions_ = 2;
    config.capacity_ = 2;
    config.max_results_ = 2;
    config.initial_revision_ = 10;
    check(index.vqec_vision_ai_ports_emidx_configure(config).code_ == status_code::ok);

    const float diagonal = 1.0F / std::sqrt(2.0F);
    check(index.vqec_vision_ai_ports_emidx_upsert(
              {1, "subject_a", {1.0F, 0.0F}}, 10, 11).code_ == status_code::ok);
    check(index.vqec_vision_ai_ports_emidx_upsert(
              {2, "subject_b", {diagonal, diagonal}}, 11, 12).code_ == status_code::ok);
    check(index.vqec_vision_ai_ports_emidx_upsert(
              {3, "subject_c", {0.0F, 1.0F}}, 11, 12).code_ ==
        status_code::invalid_state);

    const preview_frame_key frame{0, 0, 1, 20, 30};
    embedding_result query{frame, 7, "face_embedding", "1.0",
        {1.0F, 0.0F}, true};
    embedding_search_result result;
    result.matches_.reserve(config.max_results_);
    check(index.vqec_vision_ai_ports_emidx_search(
              query, 12, 2, 0.0F, result).code_ == status_code::ok);
    check(result.gallery_revision_ == 12 && result.matches_.size() == 2 &&
        result.matches_[0].record_id_ == 1 && result.matches_[1].record_id_ == 2);
    const auto preserved = result;
    check(index.vqec_vision_ai_ports_emidx_search(
              query, 11, 2, 0.0F, result).code_ == status_code::invalid_state);
    check(result.gallery_revision_ == preserved.gallery_revision_);
    query.model_version_ = "2.0";
    check(index.vqec_vision_ai_ports_emidx_search(
              query, 12, 2, 0.0F, result).code_ == status_code::invalid_argument);
    query.model_version_ = "1.0";

    check(index.vqec_vision_ai_ports_emidx_remove(1, 12, 13).code_ == status_code::ok);
    check(index.vqec_vision_ai_ports_emidx_search(
              query, 13, 2, 0.0F, result).code_ == status_code::ok);
    check(result.matches_.size() == 1 && result.matches_[0].record_id_ == 2);

    std::cout << "exact embedding index failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
