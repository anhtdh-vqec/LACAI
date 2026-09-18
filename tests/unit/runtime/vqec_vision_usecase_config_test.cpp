#include <sstream>
#include <stdexcept>
#include <string>

#include "vqec_vision_usecase_config.hpp"

namespace vqec::vision::ai {
namespace {

constexpr char g_valid_document[] = R"json({
  "schema_version":1,"control_revision":2,"entitlement_revision":3,
  "deployment_revision":4,
  "catalog":{"schema_version":1,"revision":5,"catalog_id":"usecases",
    "model_catalog_ref":"models","usecases":[{"usecase_id":"person_detection",
    "usecase_version":"1.0","root_model_ids":["person"],"feature_ids":[]}]},
  "associations":[{"source_id":"camera","usecase_id":"person_detection",
    "desired":true,"installed":true,"entitled":true,"supported":true,
    "compatible":true,"admitted":true}]
})json";

void vqec_vision_ai_unit_ucftst_check_valid_document() {
    std::istringstream stream(g_valid_document);
    usecase_control_snapshot snapshot;
    const auto loaded = vqec_vision_ai_ftmgr_ucfg_load_snapshot(stream, snapshot);
    if (loaded.code_ != status_code::ok || snapshot.control_revision_ != 2 ||
        snapshot.catalog_.usecases_.size() != 1 || snapshot.requests_.size() != 1 ||
        !snapshot.requests_[0].desired_enabled_) {
        throw std::runtime_error("valid usecase snapshot rejected");
    }
}

void vqec_vision_ai_unit_ucftst_check_transactional_rejection() {
    const std::string duplicate =
        "{\"schema_version\":1,\"schema_version\":1}";
    std::istringstream stream(duplicate);
    usecase_control_snapshot snapshot;
    snapshot.control_revision_ = 99;
    const auto loaded = vqec_vision_ai_ftmgr_ucfg_load_snapshot(stream, snapshot);
    if (loaded.code_ != status_code::invalid_argument || snapshot.control_revision_ != 99) {
        throw std::runtime_error("invalid usecase snapshot changed output");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_ucftst_check_valid_document();
        vqec::vision::ai::vqec_vision_ai_unit_ucftst_check_transactional_rejection();
    } catch (const std::exception&) {
        return 1;
    }
    return 0;
}
