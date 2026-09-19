#include "vqec_vision_service_feature_registry.hpp"

#include <new>

namespace vqec::vision::ai {
namespace {

constexpr char g_fire_smoke_processor_contract[] = "fire_smoke_alarm";

}  // namespace

status service_feature_registry::vqec_vision_ai_appl_sfreg_register_compiled(
    const feature_catalog& _catalog, feature_processor_registry& _registry,
    feature_catalog& _platform_catalog) {
    feature_catalog platform_catalog;
    platform_catalog.schema_version_ = _catalog.schema_version_;
    platform_catalog.revision_ = _catalog.revision_;
    platform_catalog.catalog_id_ = _catalog.catalog_id_;
    platform_catalog.model_catalog_ref_ = _catalog.model_catalog_ref_;
    try {
        platform_catalog.features_.reserve(_catalog.features_.size());
        bool fire_smoke_registered = false;
        for (const auto& feature : _catalog.features_) {
            if (feature.processor_contract_ == g_fire_smoke_processor_contract) {
                if (!fire_smoke_registered) {
                    const auto registered =
                        _registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
                            g_fire_smoke_processor_contract, fire_smoke_factory_);
                    if (registered.code_ != status_code::ok) {
                        return registered;
                    }
                    fire_smoke_registered = true;
                }
                continue;
            }
            platform_catalog.features_.push_back(feature);
        }
        _platform_catalog = std::move(platform_catalog);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "service feature registry allocation failed"};
    }
}

}  // namespace vqec::vision::ai
