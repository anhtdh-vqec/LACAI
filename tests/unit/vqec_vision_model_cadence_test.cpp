#include <cstdint>
#include <iostream>
#include <string>

#include "vqec_vision_model_cadence.hpp"

namespace {

vqec::vision::ai::model_catalog_entry
vqec_vision_ai_unit_mctst_make_model(const std::string& _model_id,
                                     std::uint32_t _fps) {
    vqec::vision::ai::model_catalog_entry model;
    model.model_id_ = _model_id;
    model.inference_fps_numerator_ = _fps;
    model.inference_fps_denominator_ = 1;
    return model;
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    source_deployment_config source;
    source.profile_.fps_numerator_ = 25;
    source.profile_.fps_denominator_ = 1;
    source.model_ids_ = {"detector_10fps", "detector_25fps"};
    model_catalog catalog;
    catalog.models_.push_back(
        vqec_vision_ai_unit_mctst_make_model("detector_25fps", 25));
    catalog.models_.push_back(
        vqec_vision_ai_unit_mctst_make_model("detector_10fps", 10));

    model_cadence_config config;
    check(vqec_vision_ai_sched_mdcad_compose_config(source, catalog, config).code_ ==
          status_code::ok);
    check(config.model_count_ == 2 && config.model_fps_numerators_[0] == 10 &&
          config.model_fps_numerators_[1] == 25);

    model_cadence_scheduler scheduler;
    check(scheduler.vqec_vision_ai_sched_mdcad_configure(config).code_ == status_code::ok);
    model_cadence_selection selection;
    check(scheduler.vqec_vision_ai_sched_mdcad_select(100, selection).code_ ==
          status_code::ok);
    check(vqec_vision_ai_sched_mdcad_is_model_due(selection, 0));
    check(vqec_vision_ai_sched_mdcad_is_model_due(selection, 1));
    check(!vqec_vision_ai_sched_mdcad_is_model_due(selection, 2));

    check(scheduler.vqec_vision_ai_sched_mdcad_select(101, selection).code_ ==
          status_code::ok);
    check(!vqec_vision_ai_sched_mdcad_is_model_due(selection, 0));
    check(vqec_vision_ai_sched_mdcad_is_model_due(selection, 1));
    check(scheduler.vqec_vision_ai_sched_mdcad_select(102, selection).code_ ==
          status_code::ok);
    check(!vqec_vision_ai_sched_mdcad_is_model_due(selection, 0));
    check(scheduler.vqec_vision_ai_sched_mdcad_select(103, selection).code_ ==
          status_code::ok);
    check(vqec_vision_ai_sched_mdcad_is_model_due(selection, 0));

    const auto preserved_sequence = selection.frame_sequence_;
    check(scheduler.vqec_vision_ai_sched_mdcad_select(103, selection).code_ ==
          status_code::invalid_argument);
    check(selection.frame_sequence_ == preserved_sequence);

    model_cadence_scheduler skipped_scheduler;
    check(skipped_scheduler.vqec_vision_ai_sched_mdcad_configure(config).code_ ==
          status_code::ok);
    check(skipped_scheduler.vqec_vision_ai_sched_mdcad_select(1, selection).code_ ==
          status_code::ok);
    check(skipped_scheduler.vqec_vision_ai_sched_mdcad_select(11, selection).code_ ==
          status_code::ok);
    check(selection.due_model_mask_ == 3 && selection.skipped_intervals_ == 12);

    auto excessive = config;
    excessive.model_fps_numerators_[0] = 26;
    check(scheduler.vqec_vision_ai_sched_mdcad_configure(excessive).code_ ==
          status_code::invalid_argument);
    check(scheduler.vqec_vision_ai_sched_mdcad_is_configured());
    check(scheduler.vqec_vision_ai_sched_mdcad_select(104, selection).code_ ==
          status_code::ok);

    std::cout << "model cadence failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
