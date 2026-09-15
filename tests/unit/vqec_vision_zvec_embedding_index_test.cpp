#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "vqec_vision_zvec_embedding_index.hpp"

// Independent synthetic vectors verify backend distance conversion and mutation visibility.
// The runner supplies an unused collection path; never use a real gallery for this test.
int main(int _argc, char** _argv) {
    using namespace vqec::vision::ai;
    if (_argc != 2 || std::filesystem::exists(_argv[1])) {
        return 2;
    }
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    {
        zvec_embedding_index index(_argv[1]);
        embedding_index_config config;
        config.model_id_ = "synthetic_embedding";
        config.model_version_ = "1";
        config.dimensions_ = 2;
        config.capacity_ = 3;
        config.max_results_ = 3;
        config.initial_revision_ = 1;
        const auto configured = index.vqec_vision_ai_ports_emidx_configure(config);
        check(configured.code_ == status_code::ok);
        if (configured.code_ == status_code::ok) {
            check(index.vqec_vision_ai_ports_emidx_upsert(
                {1, "synthetic_a", {1.0F, 0.0F}}, 1, 2).code_ == status_code::ok);
            check(index.vqec_vision_ai_ports_emidx_upsert(
                {2, "synthetic_b", {0.0F, 1.0F}}, 2, 3).code_ == status_code::ok);
            check(index.vqec_vision_ai_ports_emidx_upsert(
                {3, "synthetic_c", {-1.0F, 0.0F}}, 3, 4).code_ == status_code::ok);
            embedding_result query;
            query.model_id_ = config.model_id_;
            query.model_version_ = config.model_version_;
            query.values_ = {1.0F, 0.0F};
            query.is_l2_normalized_ = true;
            embedding_search_result result;
            result.matches_.reserve(config.max_results_);
            check(index.vqec_vision_ai_ports_emidx_search(
                query, 4, 3, 0.5F, result).code_ == status_code::ok);
            check(result.matches_.size() == 1);
            if (result.matches_.size() == 1) {
                check(result.matches_[0].record_id_ == 1);
                check(result.matches_[0].subject_ref_ == "synthetic_a");
                check(result.matches_[0].similarity_ > 0.99F);
            }
            check(index.vqec_vision_ai_ports_emidx_search(
                query, 3, 3, 0.5F, result).code_ == status_code::invalid_state);
            check(index.vqec_vision_ai_ports_emidx_remove(1, 4, 5).code_ == status_code::ok);
            check(index.vqec_vision_ai_ports_emidx_search(
                query, 5, 3, 0.5F, result).code_ == status_code::ok);
            check(result.matches_.empty());
        }
    }
    // This directory was absent before this test and contains only synthetic records.
    std::filesystem::remove_all(_argv[1]);
    std::cout << "Zvec integration failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
