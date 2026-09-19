#include <cassert>
#include <sstream>
#include <string>
#include <vector>

#include "vqec_vision_runtime_control_snapshot.hpp"

namespace {

using namespace vqec::vision::ai;

constexpr char g_snapshot[] = R"({
  "schema_version": 1,
  "snapshot_revision": 4,
  "inventory_revision": 2,
  "entitlement_revision": 3,
  "desired_revision": 4,
  "associations": [{
    "app_id": "security.fire_smoke_detection",
    "source_id": "camera_front",
    "installed": true,
    "entitled": true,
    "desired": true,
    "supported": true,
    "compatible": true,
    "admitted": true,
    "configuration_revision": 1,
    "configuration_sha256": "07e72c1c0bdb8762f3c2771c0d46f5207b8ae9af9a04ef4a7832d2104930e935",
    "configuration_schema_id": "security.fire_smoke.configuration",
    "configuration_payload": {"schema_version": 1},
    "output_scopes": ["security.fire_smoke.event"],
    "reason_code": "verified",
    "entitlement_expires_utc_ns": 9000000000000000000
  }]
})";

void vqec_vision_ai_unit_rcstst_test_round_trip() {
    std::istringstream input(g_snapshot);
    runtime_control_snapshot snapshot;
    assert(vqec_vision_ai_lifec_rcsnp_load(input, snapshot).code_ == status_code::ok);
    assert(snapshot.associations_.size() == 1);
    assert(snapshot.associations_[0].is_effective());
    assert(snapshot.associations_[0].configuration_payload_ ==
        std::vector<std::uint8_t>({'{', '"', 's', 'c', 'h', 'e', 'm', 'a', '_',
            'v', 'e', 'r', 's', 'i', 'o', 'n', '"', ':', '1', '}'}));

    std::ostringstream output;
    assert(vqec_vision_ai_lifec_rcsnp_write(snapshot, output).code_ == status_code::ok);
    std::istringstream round_trip(output.str());
    runtime_control_snapshot recovered;
    assert(vqec_vision_ai_lifec_rcsnp_load(round_trip, recovered).code_ == status_code::ok);
    assert(recovered.snapshot_revision_ == snapshot.snapshot_revision_);
    assert(recovered.associations_[0].configuration_sha256_ ==
        snapshot.associations_[0].configuration_sha256_);
}

void vqec_vision_ai_unit_rcstst_test_strict_and_preserving() {
    std::string duplicate = g_snapshot;
    const auto marker = duplicate.find("\"schema_version\": 1");
    assert(marker != std::string::npos);
    duplicate.replace(marker, std::string("\"schema_version\": 1").size(),
        "\"schema_version\": 1, \"schema_version\": 1");
    std::istringstream duplicate_stream(duplicate);
    runtime_control_snapshot preserved;
    preserved.snapshot_revision_ = 99;
    assert(vqec_vision_ai_lifec_rcsnp_load(duplicate_stream, preserved).code_ ==
        status_code::invalid_argument);
    assert(preserved.snapshot_revision_ == 99);

    std::string unknown = g_snapshot;
    const auto associations = unknown.find("\"associations\"");
    assert(associations != std::string::npos);
    unknown.insert(associations, "\"unknown\": true, ");
    std::istringstream unknown_stream(unknown);
    assert(vqec_vision_ai_lifec_rcsnp_load(unknown_stream, preserved).code_ ==
        status_code::invalid_argument);
}

}  // namespace

int main() {
    vqec_vision_ai_unit_rcstst_test_round_trip();
    vqec_vision_ai_unit_rcstst_test_strict_and_preserving();
    return 0;
}
