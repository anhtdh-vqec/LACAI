#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "vqec_vision_app_manager.hpp"
#include "vqec_vision_app_manifest.hpp"
#include "vqec_vision_fire_smoke_factory.hpp"
#include "vqec_vision_sqlite_app_inventory.hpp"

namespace {

using namespace vqec::vision::ai;

struct test_database {
    std::string directory_;
    std::string path_;
    test_database() {
        char pattern[] = "/tmp/vqec_vision_app_manager_dir.XXXXXX";
        const char* directory = mkdtemp(pattern);
        assert(directory != nullptr);
        directory_ = directory;
        path_ = directory_ + "/inventory.db";
    }
    ~test_database() {
        (void)std::remove(path_.c_str());
        (void)std::remove((path_ + "-wal").c_str());
        (void)std::remove((path_ + "-shm").c_str());
        (void)rmdir(directory_.c_str());
    }
};

std::vector<std::uint8_t> vqec_vision_ai_unit_amtest_read(const char* _path) {
    std::ifstream stream(_path, std::ios::binary);
    assert(stream);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

class test_package_verifier final : public app_package_verifier_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_apver_verify(
        const app_package_candidate& _candidate,
        verified_app_package& _package) const override {
        if (_candidate.signature_payload_ != std::vector<std::uint8_t>{'t', 'e', 's', 't'} ||
            _candidate.manifest_sha256_ != VQEC_VISION_AI_APP_MANIFEST_SHA256) {
            return {status_code::unauthorized, "test package signature rejected"};
        }
        const std::string text(_candidate.manifest_payload_.begin(),
            _candidate.manifest_payload_.end());
        std::istringstream stream(text);
        verified_app_package candidate;
        const auto loaded = vqec_vision_ai_lifec_apmft_load(stream, candidate.manifest_);
        if (loaded.code_ != status_code::ok) {
            return loaded;
        }
        candidate.manifest_sha256_ = _candidate.manifest_sha256_;
        candidate.configuration_payload_ = _candidate.configuration_payload_;
        candidate.configuration_sha256_ = _candidate.configuration_sha256_;
        candidate.verification_receipt_id_ = "test_trust_receipt";
        _package = std::move(candidate);
        return {};
    }
};

app_package_candidate vqec_vision_ai_unit_amtest_candidate() {
    app_package_candidate candidate;
    candidate.manifest_payload_ = vqec_vision_ai_unit_amtest_read(
        VQEC_VISION_AI_APP_MANIFEST_FIXTURE);
    candidate.manifest_sha256_ = VQEC_VISION_AI_APP_MANIFEST_SHA256;
    candidate.configuration_payload_ = vqec_vision_ai_unit_amtest_read(
        VQEC_VISION_AI_FIRE_SMOKE_CONFIG_FIXTURE);
    candidate.configuration_sha256_ =
        "07e72c1c0bdb8762f3c2771c0d46f5207b8ae9af9a04ef4a7832d2104930e935";
    candidate.signature_payload_ = {'t', 'e', 's', 't'};
    return candidate;
}

sqlite_app_inventory_config vqec_vision_ai_unit_amtest_inventory_config(
    const std::string& _path) {
    return {_path, 8U * 1024U * 1024U, 1000};
}

void vqec_vision_ai_unit_amtest_test_full_lifecycle_and_restart() {
    test_database database;
    test_package_verifier verifier;
    fire_smoke_factory fire_factory;
    app_configuration_registry registry;
    assert(registry.vqec_vision_ai_appl_apcrg_register(
               "security.fire_smoke_detection", "security.fire_smoke.configuration",
               "fire_smoke_alarm", fire_factory)
               .code_ == status_code::ok);

    runtime_control_snapshot snapshot;
    {
        sqlite_app_inventory inventory(
            vqec_vision_ai_unit_amtest_inventory_config(database.path_));
        app_manager manager({"qcs6490_qlinux_1_8"}, verifier, registry, inventory);
        assert(manager.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);
        assert(snapshot.inventory_revision_ == 1);

        auto bad_signature = vqec_vision_ai_unit_amtest_candidate();
        bad_signature.signature_payload_.clear();
        assert(manager.vqec_vision_ai_appl_appmn_install(
                   bad_signature, 1, snapshot)
                   .code_ == status_code::unauthorized);
        assert(snapshot.inventory_revision_ == 1);

        auto candidate = vqec_vision_ai_unit_amtest_candidate();
        assert(manager.vqec_vision_ai_appl_appmn_install(candidate, 1, snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_.size() == 1);
        assert(snapshot.associations_[0].installed_);
        assert(!snapshot.associations_[0].desired_);

        app_authority_update authority;
        authority.app_id_ = "security.fire_smoke_detection";
        authority.source_id_ = "camera_front";
        authority.expected_entitlement_revision_ = snapshot.entitlement_revision_;
        authority.entitled_ = true;
        authority.supported_ = true;
        authority.compatible_ = true;
        authority.admitted_ = true;
        authority.entitlement_expires_utc_ns_ = 9000000000000000000ULL;
        authority.output_scopes_ = {"security.fire_smoke.event"};
        authority.reason_code_ = "verified";
        assert(manager.vqec_vision_ai_appl_appmn_apply_verified_authority(
                   authority, snapshot)
                   .code_ == status_code::ok);
        app_desired_update desired{authority.app_id_, authority.source_id_,
            snapshot.desired_revision_, true};
        assert(manager.vqec_vision_ai_appl_appmn_set_desired(desired, snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_[0].is_effective());

        auto invalid_configuration = candidate.configuration_payload_;
        invalid_configuration.push_back('{');
        assert(manager.vqec_vision_ai_appl_appmn_update_configuration(
                   authority.app_id_, 1, invalid_configuration,
                   candidate.configuration_sha256_, snapshot)
                   .code_ == status_code::protocol_error);
        assert(snapshot.associations_[0].configuration_revision_ == 1);
    }
    {
        sqlite_app_inventory inventory(
            vqec_vision_ai_unit_amtest_inventory_config(database.path_));
        app_manager manager({"qcs6490_qlinux_1_8"}, verifier, registry, inventory);
        assert(manager.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);
        assert(snapshot.associations_.size() == 1);
        assert(snapshot.associations_[0].is_effective());
    }
}

void vqec_vision_ai_unit_amtest_test_target_and_digest_fail_closed() {
    test_database database;
    test_package_verifier verifier;
    fire_smoke_factory fire_factory;
    app_configuration_registry registry;
    assert(registry.vqec_vision_ai_appl_apcrg_register(
               "security.fire_smoke_detection", "security.fire_smoke.configuration",
               "fire_smoke_alarm", fire_factory)
               .code_ == status_code::ok);
    sqlite_app_inventory inventory(
        vqec_vision_ai_unit_amtest_inventory_config(database.path_));
    app_manager wrong_target({"another_target"}, verifier, registry, inventory);
    runtime_control_snapshot snapshot;
    assert(wrong_target.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);
    auto candidate = vqec_vision_ai_unit_amtest_candidate();
    assert(wrong_target.vqec_vision_ai_appl_appmn_install(candidate, 1, snapshot).code_ ==
        status_code::unsupported);

    candidate.configuration_payload_[0] ^= 1U;
    assert(snapshot.associations_.empty());
}

}  // namespace

int main() {
    vqec_vision_ai_unit_amtest_test_full_lifecycle_and_restart();
    vqec_vision_ai_unit_amtest_test_target_and_digest_fail_closed();
    return 0;
}
