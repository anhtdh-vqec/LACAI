#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "vqec_vision_app_manifest.hpp"
#include "vqec_vision_sqlite_app_inventory.hpp"

namespace {

using namespace vqec::vision::ai;

struct test_database {
    std::string directory_;
    std::string path_;
    test_database() {
        char pattern[] = "/tmp/vqec_vision_app_inventory_dir.XXXXXX";
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

std::vector<std::uint8_t> vqec_vision_ai_unit_saitst_read_bytes(const char* _path) {
    std::ifstream stream(_path, std::ios::binary);
    assert(stream);
    std::vector<std::uint8_t> bytes;
    char byte = 0;
    while (stream.get(byte)) {
        bytes.push_back(static_cast<std::uint8_t>(byte));
    }
    assert(stream.eof());
    return bytes;
}

usecase_app_manifest vqec_vision_ai_unit_saitst_load_manifest() {
    std::ifstream stream(VQEC_VISION_AI_APP_MANIFEST_FIXTURE, std::ios::binary);
    assert(stream);
    usecase_app_manifest manifest;
    assert(vqec_vision_ai_lifec_apmft_load(stream, manifest).code_ == status_code::ok);
    return manifest;
}

sqlite_app_inventory_config vqec_vision_ai_unit_saitst_config(const std::string& _path) {
    return {_path, 8U * 1024U * 1024U, 1000};
}

app_install_request vqec_vision_ai_unit_saitst_install_request() {
    app_install_request request;
    request.manifest_ = vqec_vision_ai_unit_saitst_load_manifest();
    request.manifest_sha256_ =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    request.expected_inventory_revision_ = 1;
    request.configuration_revision_ = 1;
    request.configuration_sha256_ = request.manifest_.configuration_defaults_sha256_;
    request.configuration_payload_ = vqec_vision_ai_unit_saitst_read_bytes(
        VQEC_VISION_AI_FIRE_SMOKE_CONFIG_FIXTURE);
    request.supported_ = true;
    request.compatible_ = true;
    request.admitted_ = true;
    return request;
}

app_authority_update vqec_vision_ai_unit_saitst_authority() {
    app_authority_update update;
    update.app_id_ = "security.fire_smoke_detection";
    update.source_id_ = "camera_front";
    update.expected_entitlement_revision_ = 1;
    update.entitled_ = true;
    update.supported_ = true;
    update.compatible_ = true;
    update.admitted_ = true;
    update.entitlement_expires_utc_ns_ = 9000000000000000000ULL;
    update.output_scopes_ = {"security.fire_smoke.event"};
    update.reason_code_ = "ok";
    return update;
}

void vqec_vision_ai_unit_saitst_test_lifecycle_and_restart() {
    test_database database;
    runtime_control_snapshot snapshot;
    {
        sqlite_app_inventory inventory(vqec_vision_ai_unit_saitst_config(database.path_));
        assert(inventory.vqec_vision_ai_ports_apinv_open().code_ == status_code::ok);
        assert(inventory.vqec_vision_ai_ports_apinv_load_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.snapshot_revision_ == 1 && snapshot.associations_.empty());
        const auto install = vqec_vision_ai_unit_saitst_install_request();
        assert(inventory.vqec_vision_ai_ports_apinv_install(install, snapshot).code_ ==
            status_code::unauthorized);
        auto authority = vqec_vision_ai_unit_saitst_authority();
        assert(inventory.vqec_vision_ai_ports_apinv_update_authority(
            authority, snapshot).code_ == status_code::ok);
        assert(snapshot.entitlement_revision_ == 2 && snapshot.associations_.empty());
        assert(inventory.vqec_vision_ai_ports_apinv_install(install, snapshot).code_ ==
            status_code::ok);
        assert(snapshot.inventory_revision_ == 2 && snapshot.associations_.size() == 1);
        assert(snapshot.associations_[0].installed_ && snapshot.associations_[0].entitled_ &&
            !snapshot.associations_[0].desired_);
        runtime_control_snapshot preserved;
        preserved.snapshot_revision_ = 99;
        assert(inventory.vqec_vision_ai_ports_apinv_install(install, preserved).code_ !=
            status_code::ok);
        assert(preserved.snapshot_revision_ == 99);
        app_desired_update desired{authority.app_id_, authority.source_id_, 1, true};
        assert(inventory.vqec_vision_ai_ports_apinv_set_desired(
            desired, snapshot).code_ == status_code::ok);
        assert(snapshot.desired_revision_ == 2 && snapshot.associations_[0].is_effective());
    }
    {
        sqlite_app_inventory recovered(vqec_vision_ai_unit_saitst_config(database.path_));
        assert(recovered.vqec_vision_ai_ports_apinv_open().code_ == status_code::ok);
        assert(recovered.vqec_vision_ai_ports_apinv_load_snapshot(snapshot).code_ ==
            status_code::ok);
        assert(snapshot.associations_.size() == 1 && snapshot.associations_[0].is_effective());
        app_configuration_update configuration;
        configuration.app_id_ = "security.fire_smoke_detection";
        configuration.expected_configuration_revision_ = 1;
        configuration.configuration_revision_ = 2;
        configuration.configuration_sha256_ =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        configuration.configuration_payload_ = {'{', '"', 's', 'c', 'h', 'e', 'm', 'a',
            '_', 'v', 'e', 'r', 's', 'i', 'o', 'n', '"', ':', '1', '}'};
        assert(recovered.vqec_vision_ai_ports_apinv_update_configuration(
            configuration, snapshot).code_ == status_code::ok);
        assert(snapshot.inventory_revision_ == 3 &&
            snapshot.associations_[0].configuration_revision_ == 2);
        assert(recovered.vqec_vision_ai_ports_apinv_uninstall(
            "security.fire_smoke_detection", 3, snapshot).code_ ==
            status_code::invalid_state);
        app_desired_update disable{
            "security.fire_smoke_detection", "camera_front", 2, false};
        assert(recovered.vqec_vision_ai_ports_apinv_set_desired(
            disable, snapshot).code_ == status_code::ok);
        assert(recovered.vqec_vision_ai_ports_apinv_uninstall(
            "security.fire_smoke_detection", 3, snapshot).code_ == status_code::ok);
        assert(snapshot.inventory_revision_ == 4 && snapshot.associations_.empty());
    }
}

void vqec_vision_ai_unit_saitst_test_scope_and_revision_fail_closed() {
    test_database database;
    sqlite_app_inventory inventory(vqec_vision_ai_unit_saitst_config(database.path_));
    assert(inventory.vqec_vision_ai_ports_apinv_open().code_ == status_code::ok);
    struct stat database_status {};
    assert(stat(database.path_.c_str(), &database_status) == 0);
    assert((database_status.st_mode & (S_IRWXG | S_IRWXO)) == 0);
    auto install = vqec_vision_ai_unit_saitst_install_request();
    runtime_control_snapshot snapshot;
    auto authority = vqec_vision_ai_unit_saitst_authority();
    authority.output_scopes_ = {"security.identity.secret"};
    assert(inventory.vqec_vision_ai_ports_apinv_update_authority(
        authority, snapshot).code_ == status_code::ok);
    runtime_control_snapshot after;
    assert(inventory.vqec_vision_ai_ports_apinv_load_snapshot(after).code_ == status_code::ok);
    assert(after.entitlement_revision_ == 2 && after.associations_.empty());
    assert(inventory.vqec_vision_ai_ports_apinv_install(install, snapshot).code_ ==
        status_code::unauthorized);
    app_desired_update desired{authority.app_id_, authority.source_id_, 1, true};
    assert(inventory.vqec_vision_ai_ports_apinv_set_desired(
        desired, snapshot).code_ == status_code::invalid_state);
    assert(inventory.vqec_vision_ai_ports_apinv_load_snapshot(after).code_ == status_code::ok);
    assert(after.desired_revision_ == 1);

    authority = vqec_vision_ai_unit_saitst_authority();
    authority.expected_entitlement_revision_ = 2;
    assert(inventory.vqec_vision_ai_ports_apinv_update_authority(
        authority, after).code_ == status_code::ok);
    assert(inventory.vqec_vision_ai_ports_apinv_install(install, after).code_ ==
        status_code::ok);
    app_desired_update enable{authority.app_id_, authority.source_id_, 1, true};
    assert(inventory.vqec_vision_ai_ports_apinv_set_desired(
        enable, after).code_ == status_code::ok);
    assert(after.associations_[0].is_effective());
    authority.expected_entitlement_revision_ = 3;
    authority.entitled_ = false;
    authority.output_scopes_.clear();
    authority.entitlement_expires_utc_ns_ = 0;
    authority.reason_code_ = "revoked";
    assert(inventory.vqec_vision_ai_ports_apinv_update_authority(
        authority, after).code_ == status_code::ok);
    assert(after.associations_[0].desired_);
    assert(!after.associations_[0].entitled_);
    assert(!after.associations_[0].is_effective());
    assert(after.associations_[0].output_scopes_.empty());
}

void vqec_vision_ai_unit_saitst_test_storage_security_and_corruption() {
    sqlite_app_inventory insecure({"/tmp/vqec_vision_insecure_inventory.db",
        8U * 1024U * 1024U, 1000});
    assert(insecure.vqec_vision_ai_ports_apinv_open().code_ ==
        status_code::unauthorized);

    test_database database;
    {
        std::ofstream stream(database.path_, std::ios::binary);
        assert(stream);
        stream << "not-a-sqlite-database";
    }
    assert(chmod(database.path_.c_str(), S_IRUSR | S_IWUSR) == 0);
    sqlite_app_inventory corrupt(vqec_vision_ai_unit_saitst_config(database.path_));
    assert(corrupt.vqec_vision_ai_ports_apinv_open().code_ != status_code::ok);
}

}  // namespace

int main() {
    vqec_vision_ai_unit_saitst_test_lifecycle_and_restart();
    vqec_vision_ai_unit_saitst_test_scope_and_revision_fail_closed();
    vqec_vision_ai_unit_saitst_test_storage_security_and_corruption();
    return 0;
}
