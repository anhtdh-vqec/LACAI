#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include <fcntl.h>

#include "vqec_vision_app_content_store.hpp"

namespace {

using namespace vqec::vision::ai;

constexpr char g_abc_sha256[] =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
constexpr char g_wrong_sha256[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char g_def_sha256[] =
    "cb8379ac2098aa165029e3938a51da0bcecfc008fd6795f401178647f96c5b34";

struct test_directory {
    std::string path_;
    test_directory() {
        char pattern[] = "/tmp/vqec_vision_app_content_store.XXXXXX";
        const char* directory = ::mkdtemp(pattern);
        assert(directory != nullptr);
        path_ = directory;
    }
    ~test_directory() {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path_)) {
                (void)::unlink(entry.path().c_str());
            }
        } catch (...) {
        }
        (void)::rmdir(path_.c_str());
    }
};

app_content_store_config vqec_vision_ai_unit_acstst_config(
    const std::string& _path) {
    return {_path, 1024U, 512U, 4U};
}

void vqec_vision_ai_unit_acstst_test_publish_restart_and_remove() {
    test_directory directory;
    const auto abandoned = directory.path_ + "/.staging.abandoned";
    {
        std::ofstream stream(abandoned, std::ios::binary);
        assert(stream);
        stream << "partial";
    }
    app_content_record record;
    {
        app_content_store store(vqec_vision_ai_unit_acstst_config(directory.path_));
        assert(store.vqec_vision_ai_ports_apcst_open().code_ == status_code::ok);
        assert(::access(abandoned.c_str(), F_OK) != 0);
        std::istringstream source("abc");
        assert(store.vqec_vision_ai_ports_apcst_put(
                   source, g_abc_sha256, 3U, record)
                   .code_ == status_code::ok);
        assert(record.sha256_ == g_abc_sha256 && record.byte_size_ == 3U);
        struct stat file_status {};
        assert(::lstat(record.immutable_location_.c_str(), &file_status) == 0);
        assert(S_ISREG(file_status.st_mode));
        assert((file_status.st_mode & (S_IRWXG | S_IRWXO | S_IWUSR)) == 0);

        std::istringstream duplicate("ignored-existing-content");
        app_content_record duplicate_record;
        assert(store.vqec_vision_ai_ports_apcst_put(
                   duplicate, g_abc_sha256, 3U, duplicate_record)
                   .code_ == status_code::ok);
        assert(duplicate_record.immutable_location_ == record.immutable_location_);
    }
    {
        app_content_store recovered(vqec_vision_ai_unit_acstst_config(directory.path_));
        assert(recovered.vqec_vision_ai_ports_apcst_open().code_ == status_code::ok);
        app_content_record loaded;
        assert(recovered.vqec_vision_ai_ports_apcst_get(g_abc_sha256, loaded).code_ ==
            status_code::ok);
        assert(loaded.byte_size_ == 3U);
        assert(recovered.vqec_vision_ai_ports_apcst_remove(g_abc_sha256).code_ ==
            status_code::ok);
        assert(recovered.vqec_vision_ai_ports_apcst_get(g_abc_sha256, loaded).code_ ==
            status_code::source_lost);
    }
}

void vqec_vision_ai_unit_acstst_test_fail_closed() {
    test_directory directory;
    app_content_store store(vqec_vision_ai_unit_acstst_config(directory.path_));
    assert(store.vqec_vision_ai_ports_apcst_open().code_ == status_code::ok);
    app_content_record preserved{"preserved", 9U, "preserved"};
    std::istringstream wrong_digest("abc");
    assert(store.vqec_vision_ai_ports_apcst_put(
               wrong_digest, g_wrong_sha256, 3U, preserved)
               .code_ == status_code::protocol_error);
    assert(preserved.sha256_ == "preserved");
    assert(::access((directory.path_ + "/" + g_wrong_sha256).c_str(), F_OK) != 0);

    std::istringstream wrong_size("abc");
    assert(store.vqec_vision_ai_ports_apcst_put(
               wrong_size, g_abc_sha256, 4U, preserved)
               .code_ == status_code::protocol_error);
    assert(::access((directory.path_ + "/" + g_abc_sha256).c_str(), F_OK) != 0);
}

void vqec_vision_ai_unit_acstst_test_root_and_entry_security() {
    test_directory insecure_directory;
    assert(::chmod(insecure_directory.path_.c_str(),
               S_IRWXU | S_IRGRP | S_IXGRP) == 0);
    app_content_store insecure(
        vqec_vision_ai_unit_acstst_config(insecure_directory.path_));
    assert(insecure.vqec_vision_ai_ports_apcst_open().code_ ==
        status_code::unauthorized);

    test_directory unknown_directory;
    const auto unknown = unknown_directory.path_ + "/unexpected";
    {
        std::ofstream stream(unknown, std::ios::binary);
        assert(stream);
        stream << "unexpected";
    }
    app_content_store unknown_store(
        vqec_vision_ai_unit_acstst_config(unknown_directory.path_));
    assert(unknown_store.vqec_vision_ai_ports_apcst_open().code_ ==
        status_code::invalid_state);

    test_directory symlink_directory;
    app_content_store symlink_store(
        vqec_vision_ai_unit_acstst_config(symlink_directory.path_));
    assert(symlink_store.vqec_vision_ai_ports_apcst_open().code_ == status_code::ok);
    const auto symlink_path = symlink_directory.path_ + "/" + g_abc_sha256;
    assert(::symlink("/etc/passwd", symlink_path.c_str()) == 0);
    app_content_record record;
    assert(symlink_store.vqec_vision_ai_ports_apcst_get(g_abc_sha256, record).code_ ==
        status_code::io_error);
}

void vqec_vision_ai_unit_acstst_test_quota_is_fail_closed() {
    test_directory directory;
    auto config = vqec_vision_ai_unit_acstst_config(directory.path_);
    config.max_store_bytes_ = 3U;
    config.max_blob_bytes_ = 3U;
    config.max_blob_count_ = 1U;
    app_content_store store(config);
    assert(store.vqec_vision_ai_ports_apcst_open().code_ == status_code::ok);
    app_content_record record;
    std::istringstream first("abc");
    assert(store.vqec_vision_ai_ports_apcst_put(first, g_abc_sha256, 3U, record).code_ ==
        status_code::ok);
    std::istringstream second("def");
    assert(store.vqec_vision_ai_ports_apcst_put(second, g_def_sha256, 3U, record).code_ ==
        status_code::resource_exhausted);
}

void vqec_vision_ai_unit_acstst_test_descriptor_ingest() {
    test_directory directory;
    app_content_store store(vqec_vision_ai_unit_acstst_config(directory.path_));
    assert(store.vqec_vision_ai_ports_apcst_open().code_ == status_code::ok);

    char input_pattern[] = "/tmp/vqec_vision_app_component.XXXXXX";
    const int writable = ::mkstemp(input_pattern);
    assert(writable >= 0);
    assert(::write(writable, "def", 3U) == 3);
    assert(::fsync(writable) == 0);
    assert(::close(writable) == 0);

    const int readable = ::open(input_pattern, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    assert(readable >= 0);
    app_content_record record;
    assert(store.vqec_vision_ai_ports_apcst_put_descriptor(
               readable, g_def_sha256, 3U, record)
               .code_ == status_code::ok);
    assert(::lseek(readable, 0, SEEK_CUR) == 0);
    assert(::close(readable) == 0);

    const int read_write = ::open(input_pattern, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    assert(read_write >= 0);
    assert(store.vqec_vision_ai_ports_apcst_put_descriptor(
               read_write, g_def_sha256, 3U, record)
               .code_ == status_code::invalid_argument);
    assert(::close(read_write) == 0);
    assert(::unlink(input_pattern) == 0);
}

}  // namespace

int main() {
    vqec_vision_ai_unit_acstst_test_publish_restart_and_remove();
    vqec_vision_ai_unit_acstst_test_fail_closed();
    vqec_vision_ai_unit_acstst_test_root_and_entry_security();
    vqec_vision_ai_unit_acstst_test_quota_is_fail_closed();
    vqec_vision_ai_unit_acstst_test_descriptor_ingest();
    return 0;
}
