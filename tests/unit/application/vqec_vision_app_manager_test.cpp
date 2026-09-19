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

const char* g_manifest_fixture = VQEC_VISION_AI_APP_MANIFEST_FIXTURE;
const char* g_configuration_fixture = VQEC_VISION_AI_FIRE_SMOKE_CONFIG_FIXTURE;

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

class test_entitlement_verifier final : public app_entitlement_verifier_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_entvr_verify(
        const app_entitlement_candidate& _candidate,
        verified_app_entitlement& _entitlement) const override {
        if (_candidate.signature_payload_ != std::vector<std::uint8_t>{'t', 'e', 's', 't'}) {
            return {status_code::unauthorized, "test entitlement signature rejected"};
        }
        verified_app_entitlement candidate;
        candidate.grant_.schema_version_ = app_lifecycle_limits::g_schema_version;
        candidate.grant_.grant_id_ = "test_grant";
        candidate.grant_.grant_revision_ = 1;
        candidate.grant_.expected_entitlement_revision_ = 1;
        candidate.grant_.issuer_id_ = "test_issuer";
        candidate.grant_.key_id_ = "test_key";
        candidate.grant_.customer_id_ = "test_customer";
        candidate.grant_.device_id_ = "test_device";
        candidate.grant_.target_id_ = "qcs6490_qlinux_1_8";
        candidate.grant_.app_id_ = "security.fire_smoke_detection";
        candidate.grant_.source_id_ = "camera_front";
        candidate.grant_.not_before_utc_ns_ = 1;
        candidate.grant_.expires_utc_ns_ = 9000000000000000000ULL;
        candidate.grant_.granted_ = true;
        candidate.grant_.output_scopes_ = {"security.fire_smoke.event"};
        candidate.grant_sha256_ = _candidate.grant_sha256_;
        candidate.verification_receipt_id_ = "test_entitlement_receipt";
        _entitlement = std::move(candidate);
        return {};
    }
};

app_manager_config vqec_vision_ai_unit_amtest_manager_config() {
    app_manager_config config;
    config.target_id_ = "qcs6490_qlinux_1_8";
    config.device_id_ = "test_device";
    config.capacity_.max_resident_bytes_ = 512U * 1024U * 1024U;
    config.capacity_.max_tensor_bytes_ = 128U * 1024U * 1024U;
    config.capacity_.max_active_incidents_ = 32;
    config.capacity_.max_events_per_second_ = 64.0;
    return config;
}

app_package_candidate vqec_vision_ai_unit_amtest_candidate() {
    app_package_candidate candidate;
    candidate.manifest_payload_ = vqec_vision_ai_unit_amtest_read(
        g_manifest_fixture);
    candidate.manifest_sha256_ = VQEC_VISION_AI_APP_MANIFEST_SHA256;
    candidate.configuration_payload_ = vqec_vision_ai_unit_amtest_read(
        g_configuration_fixture);
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
    test_entitlement_verifier entitlement_verifier;
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
        app_manager manager(vqec_vision_ai_unit_amtest_manager_config(), verifier,
            entitlement_verifier, registry, inventory);
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

        app_entitlement_candidate entitlement;
        entitlement.grant_payload_ = {'{', '}'};
        entitlement.grant_sha256_ =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        entitlement.signature_payload_ = {'t', 'e', 's', 't'};
        assert(manager.vqec_vision_ai_appl_appmn_apply_entitlement(
                   entitlement, snapshot)
                   .code_ == status_code::ok);
        app_desired_update desired{"security.fire_smoke_detection", "camera_front",
            snapshot.desired_revision_, true};
        assert(manager.vqec_vision_ai_appl_appmn_set_desired(desired, snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_[0].is_effective());

        auto invalid_configuration = candidate.configuration_payload_;
        invalid_configuration.push_back('{');
        assert(manager.vqec_vision_ai_appl_appmn_update_configuration(
                   "security.fire_smoke_detection", 1, invalid_configuration,
                   candidate.configuration_sha256_, snapshot)
                   .code_ == status_code::protocol_error);
        assert(snapshot.associations_[0].configuration_revision_ == 1);

        std::string updated_text(candidate.configuration_payload_.begin(),
            candidate.configuration_payload_.end());
        const std::string old_threshold = "\"fire_alarm_confidence\": 0.6";
        const auto threshold_offset = updated_text.find(old_threshold);
        assert(threshold_offset != std::string::npos);
        updated_text.replace(threshold_offset, old_threshold.size(),
            "\"fire_alarm_confidence\": 0.72");
        const std::vector<std::uint8_t> updated_configuration(
            updated_text.begin(), updated_text.end());
        constexpr char updated_sha256[] =
            "f3155b44db80397765c5437cdb7e4023ef5ca7ff26682afd74a92f42f1742387";
        assert(manager.vqec_vision_ai_appl_appmn_update_configuration(
                   "security.fire_smoke_detection", 1, updated_configuration,
                   updated_sha256, snapshot)
                   .code_ == status_code::ok);
        assert(snapshot.associations_[0].configuration_revision_ == 2);
        assert(snapshot.associations_[0].configuration_sha256_ == updated_sha256);
        assert(manager.vqec_vision_ai_appl_appmn_update_configuration(
                   "security.fire_smoke_detection", 1, candidate.configuration_payload_,
                   candidate.configuration_sha256_, snapshot)
                   .code_ == status_code::invalid_state);
        assert(snapshot.associations_[0].configuration_revision_ == 2);
    }
    {
        sqlite_app_inventory inventory(
            vqec_vision_ai_unit_amtest_inventory_config(database.path_));
        app_manager manager(vqec_vision_ai_unit_amtest_manager_config(), verifier,
            entitlement_verifier, registry, inventory);
        assert(manager.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);
        assert(snapshot.associations_.size() == 1);
        assert(snapshot.associations_[0].is_effective());
        assert(snapshot.associations_[0].configuration_revision_ == 2);
        assert(snapshot.associations_[0].configuration_sha256_ ==
            "f3155b44db80397765c5437cdb7e4023ef5ca7ff26682afd74a92f42f1742387");
    }
}

void vqec_vision_ai_unit_amtest_test_target_and_digest_fail_closed() {
    test_database database;
    test_package_verifier verifier;
    test_entitlement_verifier entitlement_verifier;
    fire_smoke_factory fire_factory;
    app_configuration_registry registry;
    assert(registry.vqec_vision_ai_appl_apcrg_register(
               "security.fire_smoke_detection", "security.fire_smoke.configuration",
               "fire_smoke_alarm", fire_factory)
               .code_ == status_code::ok);
    sqlite_app_inventory inventory(
        vqec_vision_ai_unit_amtest_inventory_config(database.path_));
    auto wrong_config = vqec_vision_ai_unit_amtest_manager_config();
    wrong_config.target_id_ = "another_target";
    app_manager wrong_target(wrong_config, verifier, entitlement_verifier,
        registry, inventory);
    runtime_control_snapshot snapshot;
    assert(wrong_target.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);
    auto candidate = vqec_vision_ai_unit_amtest_candidate();
    assert(wrong_target.vqec_vision_ai_appl_appmn_install(candidate, 1, snapshot).code_ ==
        status_code::unsupported);

    candidate.configuration_payload_[0] ^= 1U;
    assert(snapshot.associations_.empty());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3) {
        g_manifest_fixture = argv[1];
        g_configuration_fixture = argv[2];
    } else if (argc != 1) {
        return 2;
    }
    vqec_vision_ai_unit_amtest_test_full_lifecycle_and_restart();
    vqec_vision_ai_unit_amtest_test_target_and_digest_fail_closed();
    return 0;
}
