#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <fcntl.h>

#include "vqec_vision_app_content_store.hpp"
#include "vqec_vision_app_manager.hpp"
#include "vqec_vision_app_manifest.hpp"
#include "vqec_vision_fire_smoke_factory.hpp"
#include "vqec_vision_sqlite_app_inventory.hpp"

namespace {

using namespace vqec::vision::ai;

void vqec_vision_ai_unit_amtest_wait_operation(
    app_manager& _manager, const std::string& _operation_id,
    app_operation_record& _operation) {
    constexpr std::size_t g_max_attempts = 2000;
    for (std::size_t attempt = 0; attempt < g_max_attempts; ++attempt) {
        assert(_manager.vqec_vision_ai_ports_apmgr_get_operation(
                   _operation_id, _operation)
                   .code_ == status_code::ok);
        if (_operation.state_ == app_operation_state::committed ||
            _operation.state_ == app_operation_state::rolled_back ||
            _operation.state_ == app_operation_state::failed ||
            _operation.state_ == app_operation_state::cancelled ||
            _operation.state_ == app_operation_state::recovery_required) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "app operation did not reach a terminal state");
}

const char* g_manifest_fixture = VQEC_VISION_AI_APP_MANIFEST_FIXTURE;
const char* g_configuration_fixture = VQEC_VISION_AI_FIRE_SMOKE_CONFIG_FIXTURE;

struct test_database {
    std::string directory_;
    std::string path_;
    std::string content_path_;
    std::string model_path_;
    std::string labels_path_;
    std::string update_model_path_;
    std::string update_labels_path_;
    test_database() {
        char pattern[] = "/tmp/vqec_vision_app_manager_dir.XXXXXX";
        const char* directory = mkdtemp(pattern);
        assert(directory != nullptr);
        directory_ = directory;
        path_ = directory_ + "/inventory.db";
        content_path_ = directory_ + "/content";
        model_path_ = directory_ + "/model.bin";
        labels_path_ = directory_ + "/labels.txt";
        update_model_path_ = directory_ + "/model_update.bin";
        update_labels_path_ = directory_ + "/labels_update.txt";
        assert(::mkdir(content_path_.c_str(), S_IRWXU) == 0);
        {
            std::ofstream stream(model_path_, std::ios::binary);
            assert(stream);
            stream << "abc";
        }
        {
            std::ofstream stream(labels_path_, std::ios::binary);
            assert(stream);
            stream << "def";
        }
        {
            std::ofstream stream(update_model_path_, std::ios::binary);
            assert(stream);
            stream << "ghi";
        }
        {
            std::ofstream stream(update_labels_path_, std::ios::binary);
            assert(stream);
            stream << "jkl";
        }
    }
    ~test_database() {
        try {
            (void)std::filesystem::remove_all(directory_);
        } catch (...) {
        }
    }
};

struct test_candidate {
    app_package_candidate value_;
    std::vector<int> descriptors_;

    explicit test_candidate(const test_database& _database, bool _is_update = false);
    ~test_candidate() noexcept {
        for (const int descriptor : descriptors_) {
            if (descriptor >= 0) {
                (void)::close(descriptor);
            }
        }
    }
    test_candidate(const test_candidate&) = delete;
    test_candidate& operator=(const test_candidate&) = delete;
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
        const bool is_update = _candidate.signature_payload_ ==
            std::vector<std::uint8_t>{'u', 'p', 'd', 't'};
        if ((!is_update && _candidate.signature_payload_ !=
                 std::vector<std::uint8_t>{'t', 'e', 's', 't'}) ||
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
        if (is_update) {
            candidate.manifest_.app_version_ = "1.1.0";
            candidate.manifest_.release_sequence_ = 2;
            candidate.manifest_.rollback_predecessor_ = "1.0.0";
        }
        for (auto& component : candidate.manifest_.components_) {
            if (component.type_ == app_component_type::model) {
                component.artifact_sha256_ = is_update ?
                    "50ae61e841fac4e8f9e40baf2ad36ec868922ea48368c18f9535e47db56dd7fb" :
                    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
                component.artifact_bytes_ = 3U;
            } else if (component.type_ == app_component_type::labels) {
                component.artifact_sha256_ = is_update ?
                    "268f277c6d766d31334fda0f7a5533a185598d269e61c76a805870244828a5f1" :
                    "cb8379ac2098aa165029e3938a51da0bcecfc008fd6795f401178647f96c5b34";
                component.artifact_bytes_ = 3U;
            }
            if (is_update && component.type_ != app_component_type::configuration) {
                component.component_version_ = "1.1";
            }
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
        const bool revoke = _candidate.signature_payload_ ==
            std::vector<std::uint8_t>{'d', 'e', 'n', 'y'};
        if (!revoke && _candidate.signature_payload_ !=
                std::vector<std::uint8_t>{'t', 'e', 's', 't'}) {
            return {status_code::unauthorized, "test entitlement signature rejected"};
        }
        verified_app_entitlement candidate;
        candidate.grant_.schema_version_ = app_lifecycle_limits::g_schema_version;
        candidate.grant_.grant_id_ = revoke ? "test_revocation" : "test_grant";
        candidate.grant_.grant_revision_ = revoke ? 2 : 1;
        candidate.grant_.expected_entitlement_revision_ = revoke ? 2 : 1;
        candidate.grant_.issuer_id_ = "test_issuer";
        candidate.grant_.key_id_ = "test_key";
        candidate.grant_.customer_id_ = "test_customer";
        candidate.grant_.device_id_ = "test_device";
        candidate.grant_.target_id_ = "qcs6490_qlinux_1_8";
        candidate.grant_.app_id_ = "security.fire_smoke_detection";
        candidate.grant_.source_id_ = "camera_front";
        candidate.grant_.not_before_utc_ns_ = 1;
        candidate.grant_.expires_utc_ns_ = 9000000000000000000ULL;
        candidate.grant_.granted_ = !revoke;
        if (!revoke) {
            candidate.grant_.output_scopes_ = {"security.fire_smoke.event"};
        }
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

usecase_app_catalog vqec_vision_ai_unit_amtest_catalog() {
    usecase_app_catalog catalog;
    catalog.schema_version_ = app_lifecycle_limits::g_schema_version;
    catalog.catalog_id_ = "test.usecase_apps";
    catalog.revision_ = 1;
    catalog.applications_.push_back({"S01", "security.restricted_area_smoking",
        "Restricted Area Smoking", "1.0.0", true});
    catalog.applications_.push_back({"S04", "security.fire_smoke_detection",
        "Fire and Smoke Detection", "1.0.0", true});
    return catalog;
}

test_candidate::test_candidate(const test_database& _database, bool _is_update) {
    value_.manifest_payload_ = vqec_vision_ai_unit_amtest_read(
        g_manifest_fixture);
    value_.manifest_sha256_ = VQEC_VISION_AI_APP_MANIFEST_SHA256;
    value_.configuration_payload_ = vqec_vision_ai_unit_amtest_read(
        g_configuration_fixture);
    value_.configuration_sha256_ =
        "07e72c1c0bdb8762f3c2771c0d46f5207b8ae9af9a04ef4a7832d2104930e935";
    value_.signature_payload_ = _is_update ?
        std::vector<std::uint8_t>{'u', 'p', 'd', 't'} :
        std::vector<std::uint8_t>{'t', 'e', 's', 't'};
    const std::vector<std::string> paths = _is_update ?
        std::vector<std::string>{
            _database.update_model_path_, _database.update_labels_path_} :
        std::vector<std::string>{_database.model_path_, _database.labels_path_};
    for (const auto& path : paths) {
        const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        assert(descriptor >= 0);
        descriptors_.push_back(descriptor);
        value_.components_.push_back({descriptor});
    }
}

sqlite_app_inventory_config vqec_vision_ai_unit_amtest_inventory_config(
    const std::string& _path) {
    return {_path, 8U * 1024U * 1024U, 1000};
}

app_content_store_config vqec_vision_ai_unit_amtest_content_config(
    const std::string& _path) {
    return {_path, 8U * 1024U * 1024U, 4U * 1024U * 1024U, 32U};
}

void vqec_vision_ai_unit_amtest_test_full_lifecycle_and_restart() {
    test_database database;
    test_package_verifier verifier;
    test_entitlement_verifier entitlement_verifier;
    fire_smoke_factory fire_factory;
    app_configuration_registry registry;
    const auto catalog = vqec_vision_ai_unit_amtest_catalog();
    assert(registry.vqec_vision_ai_appl_apcrg_register(
               "security.fire_smoke_detection", "security.fire_smoke.configuration",
               "fire_smoke_alarm", fire_factory)
               .code_ == status_code::ok);

    runtime_control_snapshot snapshot;
    {
        sqlite_app_inventory inventory(
            vqec_vision_ai_unit_amtest_inventory_config(database.path_));
        app_content_store content_store(
            vqec_vision_ai_unit_amtest_content_config(database.content_path_));
        app_manager manager(vqec_vision_ai_unit_amtest_manager_config(), verifier,
            entitlement_verifier, registry, catalog, content_store, inventory);
        assert(manager.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);
        assert(snapshot.inventory_revision_ == 1);
        std::vector<app_catalog_status> applications;
        assert(manager.vqec_vision_ai_appl_appmn_list_applications(
                   "camera_front", applications)
                   .code_ == status_code::ok);
        assert(applications.size() == 2U && !applications[0].supported_ &&
            applications[0].state_ == app_install_state::not_installed &&
            applications[1].supported_ && !applications[1].installed_);

        test_candidate bad_signature(database);
        bad_signature.value_.signature_payload_.clear();
        assert(manager.vqec_vision_ai_appl_appmn_install(
                   bad_signature.value_, 1, snapshot)
                   .code_ == status_code::unauthorized);
        assert(snapshot.inventory_revision_ == 1);

        test_candidate candidate(database);
        assert(manager.vqec_vision_ai_appl_appmn_install(
                   candidate.value_, 1, snapshot)
                   .code_ == status_code::unauthorized);
        app_content_record unstaged;
        assert(content_store.vqec_vision_ai_ports_apcst_get(
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                   unstaged)
                   .code_ == status_code::source_lost);
        app_entitlement_candidate entitlement;
        entitlement.grant_payload_ = {'{', '}'};
        entitlement.grant_sha256_ =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        entitlement.signature_payload_ = {'t', 'e', 's', 't'};
        app_operation_record operation;
        const app_operation_request entitlement_request{
            "entitle_fire_smoke_001", entitlement.grant_sha256_,
            "security.fire_smoke_detection", app_operation_kind::entitlement};
        assert(manager.vqec_vision_ai_appl_appmn_submit_entitlement(
                   entitlement_request, entitlement, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::committed &&
            operation.result_code_ == status_code::ok);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_.empty() && snapshot.entitlement_revision_ == 2);
        const app_operation_request install_request{
            "install_fire_smoke_001",
            "1111111111111111111111111111111111111111111111111111111111111111",
            "security.fire_smoke_detection", app_operation_kind::install};
        assert(manager.vqec_vision_ai_appl_appmn_submit_package(install_request,
                   candidate.value_, 1, false, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::committed);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_.size() == 1);
        assert(snapshot.associations_[0].installed_);
        assert(!snapshot.associations_[0].desired_);

        app_desired_update desired{"security.fire_smoke_detection", "camera_front",
            snapshot.desired_revision_, true};
        const app_operation_request desired_request{
            "enable_fire_smoke_001",
            "2222222222222222222222222222222222222222222222222222222222222222",
            "security.fire_smoke_detection", app_operation_kind::desired};
        assert(manager.vqec_vision_ai_appl_appmn_submit_desired(
                   desired_request, desired, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::committed);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_[0].is_effective());

        auto invalid_configuration = candidate.value_.configuration_payload_;
        invalid_configuration.push_back('{');
        const app_operation_request invalid_configuration_request{
            "invalid_config_fire_smoke_001",
            candidate.value_.configuration_sha256_,
            "security.fire_smoke_detection", app_operation_kind::configure};
        assert(manager.vqec_vision_ai_appl_appmn_submit_configuration(
                   invalid_configuration_request, 1, invalid_configuration,
                   candidate.value_.configuration_sha256_, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::failed &&
            operation.result_code_ == status_code::protocol_error);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_[0].configuration_revision_ == 1);

        std::string updated_text(candidate.value_.configuration_payload_.begin(),
            candidate.value_.configuration_payload_.end());
        const std::string old_threshold = "\"fire_alarm_confidence\": 0.6";
        const auto threshold_offset = updated_text.find(old_threshold);
        assert(threshold_offset != std::string::npos);
        updated_text.replace(threshold_offset, old_threshold.size(),
            "\"fire_alarm_confidence\": 0.72");
        const std::vector<std::uint8_t> updated_configuration(
            updated_text.begin(), updated_text.end());
        constexpr char updated_sha256[] =
            "f3155b44db80397765c5437cdb7e4023ef5ca7ff26682afd74a92f42f1742387";
        const app_operation_request configuration_request{
            "config_fire_smoke_001", updated_sha256,
            "security.fire_smoke_detection", app_operation_kind::configure};
        assert(manager.vqec_vision_ai_appl_appmn_submit_configuration(
                   configuration_request, 1, updated_configuration,
                   updated_sha256, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::committed);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_[0].configuration_revision_ == 2);
        app_desired_update disable{"security.fire_smoke_detection", "camera_front",
            snapshot.desired_revision_, false};
        const app_operation_request disable_request{
            "disable_fire_smoke_001",
            "3333333333333333333333333333333333333333333333333333333333333333",
            "security.fire_smoke_detection", app_operation_kind::desired};
        assert(manager.vqec_vision_ai_appl_appmn_submit_desired(
                   disable_request, disable, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::committed);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        test_candidate update(database, true);
        app_operation_request update_request{
            "update_fire_smoke_001",
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            "security.fire_smoke_detection", app_operation_kind::update};
        assert(manager.vqec_vision_ai_appl_appmn_submit_package(update_request,
                   update.value_, snapshot.inventory_revision_, true, operation)
                   .code_ == status_code::ok);
        assert(operation.state_ == app_operation_state::queued &&
            operation.result_code_ == status_code::pending);
        const auto update_operation_id = operation.operation_id_;
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, update_operation_id, operation);
        assert(operation.state_ == app_operation_state::committed &&
            operation.result_code_ == status_code::ok);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_[0].app_version_ == "1.1.0");
        assert(snapshot.associations_[0].release_sequence_ == 2);
        assert(snapshot.associations_[0].configuration_revision_ == 3);
        app_operation_record duplicate;
        assert(manager.vqec_vision_ai_appl_appmn_submit_package(update_request,
                   update.value_, 1, true, duplicate).code_ == status_code::ok);
        assert(duplicate.operation_id_ == operation.operation_id_ &&
            duplicate.snapshot_revision_ == operation.snapshot_revision_);
        app_operation_request rollback_request{
            "rollback_fire_smoke_001",
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            "security.fire_smoke_detection", app_operation_kind::rollback};
        assert(manager.vqec_vision_ai_appl_appmn_submit_rollback(rollback_request,
                   snapshot.inventory_revision_, operation).code_ == status_code::ok);
        assert(operation.state_ == app_operation_state::queued &&
            operation.result_code_ == status_code::pending);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::rolled_back &&
            operation.result_code_ == status_code::ok);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_[0].app_version_ == "1.0.0");
        assert(snapshot.associations_[0].release_sequence_ == 1);
        assert(snapshot.associations_[0].configuration_revision_ == 4);
        assert(snapshot.associations_[0].configuration_sha256_ == updated_sha256);
        const app_operation_request stale_request{
            "update_fire_smoke_stale_001",
            "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
            "security.fire_smoke_detection", app_operation_kind::update};
        assert(manager.vqec_vision_ai_appl_appmn_submit_package(stale_request,
                   update.value_, 1, true, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::failed &&
            operation.result_code_ == status_code::invalid_state &&
            operation.snapshot_revision_ == 0);
        app_operation_record repeated_failure;
        assert(manager.vqec_vision_ai_appl_appmn_submit_package(stale_request,
                   update.value_, snapshot.inventory_revision_, true,
                   repeated_failure)
                   .code_ == status_code::ok);
        assert(repeated_failure.operation_id_ == operation.operation_id_ &&
            repeated_failure.state_ == app_operation_state::failed &&
            repeated_failure.result_code_ == status_code::invalid_state);
        assert(manager.vqec_vision_ai_appl_appmn_update_configuration(
                   "security.fire_smoke_detection", 1,
                   candidate.value_.configuration_payload_,
                   candidate.value_.configuration_sha256_, snapshot)
                   .code_ == status_code::invalid_state);
        assert(snapshot.associations_[0].configuration_revision_ == 4);

        app_entitlement_candidate revocation;
        revocation.grant_payload_ = {'{', '}'};
        revocation.grant_sha256_ =
            "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";
        revocation.signature_payload_ = {'d', 'e', 'n', 'y'};
        const app_operation_request revocation_request{
            "revoke_fire_smoke_001", revocation.grant_sha256_,
            "security.fire_smoke_detection", app_operation_kind::entitlement};
        assert(manager.vqec_vision_ai_appl_appmn_submit_entitlement(
                   revocation_request, revocation, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::committed);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(!snapshot.associations_[0].entitled_);
        applications.clear();
        assert(manager.vqec_vision_ai_appl_appmn_list_applications(
                   "camera_front", applications)
                   .code_ == status_code::ok);
        assert(applications.size() == 2U && applications[1].installed_ &&
            !applications[1].entitled_ &&
            applications[1].state_ == app_install_state::locked &&
            applications[1].reason_code_ == "entitlement_required");
    }
    {
        sqlite_app_inventory inventory(
            vqec_vision_ai_unit_amtest_inventory_config(database.path_));
        app_content_store content_store(
            vqec_vision_ai_unit_amtest_content_config(database.content_path_));
        app_manager manager(vqec_vision_ai_unit_amtest_manager_config(), verifier,
            entitlement_verifier, registry, catalog, content_store, inventory);
        assert(manager.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);
        assert(snapshot.associations_.size() == 1);
        assert(!snapshot.associations_[0].is_effective());
        assert(snapshot.associations_[0].configuration_revision_ == 4);
        assert(snapshot.associations_[0].configuration_sha256_ ==
            "f3155b44db80397765c5437cdb7e4023ef5ca7ff26682afd74a92f42f1742387");
        const app_operation_request uninstall_request{
            "uninstall_fire_smoke_001",
            "4444444444444444444444444444444444444444444444444444444444444444",
            "security.fire_smoke_detection", app_operation_kind::uninstall};
        app_operation_record operation;
        assert(manager.vqec_vision_ai_appl_appmn_submit_uninstall(
                   uninstall_request, snapshot.inventory_revision_, operation)
                   .code_ == status_code::ok);
        vqec_vision_ai_unit_amtest_wait_operation(
            manager, operation.operation_id_, operation);
        assert(operation.state_ == app_operation_state::committed &&
            operation.snapshot_revision_ != 0);
        assert(manager.vqec_vision_ai_appl_appmn_get_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_.empty());
    }
}

void vqec_vision_ai_unit_amtest_test_unpublished_app_is_fail_closed() {
    test_database database;
    test_package_verifier verifier;
    test_entitlement_verifier entitlement_verifier;
    fire_smoke_factory fire_factory;
    app_configuration_registry registry;
    assert(registry.vqec_vision_ai_appl_apcrg_register(
               "security.fire_smoke_detection", "security.fire_smoke.configuration",
               "fire_smoke_alarm", fire_factory)
               .code_ == status_code::ok);
    auto catalog = vqec_vision_ai_unit_amtest_catalog();
    catalog.applications_.erase(catalog.applications_.begin() + 1);
    sqlite_app_inventory inventory(
        vqec_vision_ai_unit_amtest_inventory_config(database.path_));
    app_content_store content_store(
        vqec_vision_ai_unit_amtest_content_config(database.content_path_));
    app_manager manager(vqec_vision_ai_unit_amtest_manager_config(), verifier,
        entitlement_verifier, registry, catalog, content_store, inventory);
    runtime_control_snapshot snapshot;
    assert(manager.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);

    app_entitlement_candidate entitlement;
    entitlement.grant_payload_ = {'{', '}'};
    entitlement.grant_sha256_ =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    entitlement.signature_payload_ = {'t', 'e', 's', 't'};
    assert(manager.vqec_vision_ai_appl_appmn_apply_entitlement(
               entitlement, snapshot)
               .code_ == status_code::unauthorized);
    test_candidate package(database);
    assert(manager.vqec_vision_ai_appl_appmn_install(
               package.value_, 1, snapshot)
               .code_ == status_code::unauthorized);
    app_content_record unstaged;
    assert(content_store.vqec_vision_ai_ports_apcst_get(
               "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
               unstaged)
               .code_ == status_code::source_lost);
}

void vqec_vision_ai_unit_amtest_test_target_and_digest_fail_closed() {
    test_database database;
    test_package_verifier verifier;
    test_entitlement_verifier entitlement_verifier;
    fire_smoke_factory fire_factory;
    app_configuration_registry registry;
    const auto catalog = vqec_vision_ai_unit_amtest_catalog();
    assert(registry.vqec_vision_ai_appl_apcrg_register(
               "security.fire_smoke_detection", "security.fire_smoke.configuration",
               "fire_smoke_alarm", fire_factory)
               .code_ == status_code::ok);
    sqlite_app_inventory inventory(
        vqec_vision_ai_unit_amtest_inventory_config(database.path_));
    app_content_store content_store(
        vqec_vision_ai_unit_amtest_content_config(database.content_path_));
    auto wrong_config = vqec_vision_ai_unit_amtest_manager_config();
    wrong_config.target_id_ = "another_target";
    app_manager wrong_target(wrong_config, verifier, entitlement_verifier,
        registry, catalog, content_store, inventory);
    runtime_control_snapshot snapshot;
    assert(wrong_target.vqec_vision_ai_appl_appmn_open(snapshot).code_ == status_code::ok);
    test_candidate candidate(database);
    assert(wrong_target.vqec_vision_ai_appl_appmn_install(
               candidate.value_, 1, snapshot)
               .code_ ==
        status_code::unsupported);

    candidate.value_.configuration_payload_[0] ^= 1U;
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
    vqec_vision_ai_unit_amtest_test_unpublished_app_is_fail_closed();
    return 0;
}
