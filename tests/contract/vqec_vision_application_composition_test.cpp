#include "vqec_vision_application_composition.hpp"

#include <cassert>

using namespace vqec::vision::ai;

int main() {
    application_composition invalid(0, 1, 1);
    assert(invalid.vqec_vision_ai_cntr_acomp_validate().code_ ==
           status_code::invalid_argument);

    application_composition composition(7, 11, 1);
    assert(composition.vqec_vision_ai_cntr_acomp_validate().code_ == status_code::ok);
    assert(composition.vqec_vision_ai_cntr_acomp_activate().code_ == status_code::ok);
    assert(composition.vqec_vision_ai_cntr_acomp_step(10).code_ ==
           status_code::unsupported);
    assert(composition.vqec_vision_ai_cntr_acomp_step(9).code_ ==
           status_code::invalid_argument);
    assert(composition.vqec_vision_ai_cntr_acomp_request_stop(10).code_ ==
           status_code::pending);
    const auto snapshot = composition.vqec_vision_ai_cntr_acomp_get_snapshot();
    assert(snapshot.deployment_revision_ == 7);
    assert(snapshot.catalog_revision_ == 11);
    assert(snapshot.is_recovery_required_);
    return 0;
}
