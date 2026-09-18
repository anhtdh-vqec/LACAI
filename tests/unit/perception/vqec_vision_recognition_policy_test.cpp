#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_recognition.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    recognition_policy_config config{0.70F, 0.10F, 4};
    check(vqec_vision_ai_embed_rcpol_validate_config(config).code_ == status_code::ok);
    const embedding_search_result search{
        {0, 0, 1, 2, 3}, 9, 12,
        {{1, "person_a", 0.90F}, {2, "person_a", 0.85F}, {3, "person_b", 0.75F}}};
    recognition_match_result result;
    check(vqec_vision_ai_embed_rcpol_evaluate(search, 12, config, result).code_ ==
        status_code::ok);
    check(result.decision_ == recognition_decision::known &&
        result.subject_ref_ == "person_a" && result.best_similarity_ == 0.90F);

    recognition_policy_config ambiguous{0.70F, 0.10F, 4};
    const embedding_search_result close_search{
        {0, 0, 1, 2, 3}, 9, 12,
        {{1, "person_a", 0.90F}, {2, "person_b", 0.85F}}};
    check(vqec_vision_ai_embed_rcpol_evaluate(close_search, 12, ambiguous, result).code_ ==
        status_code::ok);
    check(result.decision_ == recognition_decision::ambiguous && result.subject_ref_.empty());

    const embedding_search_result unknown_search{{0, 0, 1, 2, 3}, 9, 12, {}};
    check(vqec_vision_ai_embed_rcpol_evaluate(unknown_search, 12, config, result).code_ ==
        status_code::ok);
    check(result.decision_ == recognition_decision::unknown && result.subject_ref_.empty());

    recognition_policy_config invalid{1.1F, 0.1F, 4};
    check(vqec_vision_ai_embed_rcpol_validate_config(invalid).code_ ==
        status_code::invalid_argument);
    std::cout << "recognition policy failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
