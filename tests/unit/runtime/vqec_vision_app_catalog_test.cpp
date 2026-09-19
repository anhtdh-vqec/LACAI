#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

#include "vqec_vision_app_catalog.hpp"

namespace {

std::string vqec_vision_ai_unit_acatst_read_fixture() {
    std::ifstream stream(VQEC_VISION_AI_APP_CATALOG_FIXTURE, std::ios::binary);
    assert(stream);
    std::ostringstream document;
    document << stream.rdbuf();
    return document.str();
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    const auto document = vqec_vision_ai_unit_acatst_read_fixture();
    std::istringstream valid_stream(document);
    usecase_app_catalog catalog;
    assert(vqec_vision_ai_lifec_apcat_load(valid_stream, catalog).code_ ==
        status_code::ok);
    assert(catalog.schema_version_ == app_lifecycle_limits::g_schema_version);
    assert(catalog.applications_.size() == 18U);
    assert(catalog.applications_[3].app_id_ == "security.fire_smoke_detection");

    auto unknown_document = document;
    const auto revision = unknown_document.find("\"revision\": 1");
    assert(revision != std::string::npos);
    unknown_document.replace(revision, std::string("\"revision\": 1").size(),
        "\"revision\": 1, \"unknown\": true");
    std::istringstream unknown_stream(unknown_document);
    catalog.catalog_id_ = "preserved";
    assert(vqec_vision_ai_lifec_apcat_load(unknown_stream, catalog).code_ ==
        status_code::invalid_argument);
    assert(catalog.catalog_id_ == "preserved");

    auto duplicate_document = document;
    const auto duplicate = duplicate_document.find("security.suspicious_weapon");
    assert(duplicate != std::string::npos);
    duplicate_document.replace(duplicate, std::string("security.suspicious_weapon").size(),
        "security.restricted_area_smoking");
    std::istringstream duplicate_stream(duplicate_document);
    assert(vqec_vision_ai_lifec_apcat_load(duplicate_stream, catalog).code_ ==
        status_code::invalid_argument);

    std::string oversized(app_lifecycle_limits::g_max_document_bytes + 1U, 'x');
    std::istringstream oversized_stream(oversized);
    assert(vqec_vision_ai_lifec_apcat_load(oversized_stream, catalog).code_ ==
        status_code::resource_exhausted);
    return 0;
}
