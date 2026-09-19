#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "vqec_vision_app_configuration_registry.hpp"
#include "vqec_vision_app_manifest.hpp"
#include "vqec_vision_fire_smoke_factory.hpp"

namespace {

using namespace vqec::vision::ai;

std::vector<std::uint8_t> vqec_vision_ai_unit_acrtst_read(
    const std::string& _path) {
    std::ifstream stream(_path, std::ios::binary);
    assert(stream);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

usecase_app_manifest vqec_vision_ai_unit_acrtst_load_manifest() {
    std::ifstream stream(VQEC_VISION_AI_APP_MANIFEST_FIXTURE, std::ios::binary);
    assert(stream);
    usecase_app_manifest manifest;
    assert(vqec_vision_ai_lifec_apmft_load(stream, manifest).code_ == status_code::ok);
    return manifest;
}

void vqec_vision_ai_unit_acrtst_test_registration_and_validation() {
    constexpr char app_id[] = "security.fire_smoke_detection";
    constexpr char schema_id[] = "security.fire_smoke.configuration";
    constexpr char processor_contract[] = "fire_smoke_alarm";

    fire_smoke_factory factory;
    app_configuration_registry registry;
    assert(registry.vqec_vision_ai_appl_apcrg_register(
               app_id, schema_id, processor_contract, factory)
               .code_ == status_code::ok);
    assert(registry.vqec_vision_ai_appl_apcrg_register(
               app_id, schema_id, processor_contract, factory)
               .code_ == status_code::invalid_argument);

    const auto manifest = vqec_vision_ai_unit_acrtst_load_manifest();
    assert(registry.vqec_vision_ai_appl_apcrg_supports_manifest(manifest));
    const auto payload = vqec_vision_ai_unit_acrtst_read(
        VQEC_VISION_AI_FIRE_SMOKE_CONFIG_FIXTURE);
    assert(registry.vqec_vision_ai_appl_apcrg_validate(
               app_id, schema_id, 1, payload)
               .code_ == status_code::ok);

    auto invalid_payload = payload;
    invalid_payload.push_back('{');
    assert(registry.vqec_vision_ai_appl_apcrg_validate(
               app_id, schema_id, 1, invalid_payload)
               .code_ == status_code::invalid_argument);
    assert(registry.vqec_vision_ai_appl_apcrg_validate(
               "security.unknown", schema_id, 1, payload)
               .code_ == status_code::unsupported);

    auto unsupported = manifest;
    unsupported.features_[0].processor_contract_ = "unknown_processor";
    assert(!registry.vqec_vision_ai_appl_apcrg_supports_manifest(unsupported));
}

}  // namespace

int main() {
    vqec_vision_ai_unit_acrtst_test_registration_and_validation();
    return 0;
}
