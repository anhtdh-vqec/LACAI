#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <utility>

#include "vqec_vision_dsp_buffer_cache.hpp"

namespace {

using namespace vqec::vision::ai;
constexpr std::size_t g_test_bytes = 4096;

struct test_file {
    int fd_{::memfd_create("lacai_mapping_lease_test", MFD_CLOEXEC)};
    explicit test_file(std::uint8_t _value) {
        if (fd_ >= 0 && (::ftruncate(fd_, static_cast<off_t>(g_test_bytes)) != 0 ||
            ::pwrite(fd_, &_value, sizeof(_value), 0) != sizeof(_value))) {
            (void)::close(fd_);
            fd_ = -1;
        }
    }
    ~test_file() {
        if (fd_ >= 0) {
            (void)::close(fd_);
        }
    }
};

std::size_t vqec_vision_ai_unit_dbctst_count_fds() {
    DIR* directory = ::opendir("/proc/self/fd");
    if (directory == nullptr) {
        return 0;
    }
    std::size_t count = 0;
    while (const auto* entry = ::readdir(directory)) {
        if (entry->d_name[0] != '.') {
            ++count;
        }
    }
    (void)::closedir(directory);
    return count;
}

bool vqec_vision_ai_unit_dbctst_check_leases() {
    test_file first(17), second(29);
    if (first.fd_ < 0 || second.fd_ < 0) {
        return false;
    }
    dsp_buffer_cache cache({1, false});
    status state;
    auto lease = cache.vqec_vision_ai_qcom_dspbc_map(first.fd_, g_test_bytes, state);
    if (state.code_ != status_code::ok || lease.data_ == nullptr || lease.data_[0] != 17) {
        return false;
    }
    auto hit = cache.vqec_vision_ai_qcom_dspbc_map(first.fd_, g_test_bytes, state);
    if (state.code_ != status_code::ok || hit.data_ != lease.data_) {
        return false;
    }
    auto denied = cache.vqec_vision_ai_qcom_dspbc_map(second.fd_, g_test_bytes, state);
    if (state.code_ != status_code::resource_exhausted || denied.data_ != nullptr) {
        return false;
    }
    cache.vqec_vision_ai_qcom_dspbc_clear();
    if (cache.vqec_vision_ai_qcom_dspbc_entry_count() != 1 || lease.data_[0] != 17) {
        return false;
    }
    denied = cache.vqec_vision_ai_qcom_dspbc_map(first.fd_, g_test_bytes, state);
    if (state.code_ != status_code::pending || denied.data_ != nullptr) {
        return false;
    }
    hit = {};
    lease = {};
    auto replacement = cache.vqec_vision_ai_qcom_dspbc_map(second.fd_, g_test_bytes, state);
    return state.code_ == status_code::ok && replacement.data_ != nullptr &&
        replacement.data_[0] == 29 && cache.vqec_vision_ai_qcom_dspbc_entry_count() == 1;
}

bool vqec_vision_ai_unit_dbctst_check_move_and_fd_reuse() {
    const auto initial = vqec_vision_ai_unit_dbctst_count_fds();
    {
        test_file first(37), second(41);
        dsp_buffer_cache cache({2, false}), other({1, false});
        status state;
        auto retained = cache.vqec_vision_ai_qcom_dspbc_map(first.fd_, g_test_bytes, state);
        auto former = other.vqec_vision_ai_qcom_dspbc_map(second.fd_, g_test_bytes, state);
        if (!retained.owner_ || !former.owner_) {
            return false;
        }
        other = std::move(cache);
        if (retained.data_[0] != 37 || former.data_[0] != 41 ||
            cache.vqec_vision_ai_qcom_dspbc_entry_count() != 0) {
            return false;
        }
        // The cached duplicated FD keeps the old allocation alive. Reusing the
        // caller's numeric FD must resolve the new inode, not the old mapping.
        if (::dup2(second.fd_, first.fd_) < 0) {
            return false;
        }
        auto reused = other.vqec_vision_ai_qcom_dspbc_map(first.fd_, g_test_bytes, state);
        if (state.code_ != status_code::ok || reused.data_ == nullptr ||
            reused.data_[0] != 41 || retained.data_[0] != 37) {
            return false;
        }
    }
    if (vqec_vision_ai_unit_dbctst_count_fds() != initial) {
        return false;
    }
    // Move-replacement used to destroy implementation without unmapping or
    // closing its entries. Repeated replacement catches that resource leak.
    for (std::size_t cycle = 0; cycle < 200; ++cycle) {
        test_file file(51);
        dsp_buffer_cache target({1, false}), source({1, false});
        status state;
        auto mapping = target.vqec_vision_ai_qcom_dspbc_map(file.fd_, g_test_bytes, state);
        if (!mapping.owner_) {
            return false;
        }
        mapping = {};
        target = std::move(source);
    }
    return vqec_vision_ai_unit_dbctst_count_fds() == initial;
}

bool vqec_vision_ai_unit_dbctst_check_concurrent_retirement() {
    test_file first(61), second(67);
    dsp_buffer_cache cache({1, false});
    status state;
    auto pinned = cache.vqec_vision_ai_qcom_dspbc_map(first.fd_, g_test_bytes, state);
    if (!pinned.owner_) {
        return false;
    }
    std::atomic<bool> finished{false};
    std::atomic<bool> valid{true};
    std::thread reader([&]() {
        while (!finished.load()) {
            if (pinned.data_[0] != 61) {
                valid.store(false);
            }
        }
    });
    bool bounded = true;
    for (std::size_t cycle = 0; cycle < 200; ++cycle) {
        cache.vqec_vision_ai_qcom_dspbc_clear();
        const auto rejected = cache.vqec_vision_ai_qcom_dspbc_map(second.fd_, g_test_bytes, state);
        bounded = bounded && state.code_ == status_code::resource_exhausted &&
            rejected.data_ == nullptr && cache.vqec_vision_ai_qcom_dspbc_entry_count() == 1;
    }
    finished.store(true);
    reader.join();
    return valid.load() && bounded;
}

bool vqec_vision_ai_unit_dbctst_check_invalid_descriptors() {
    test_file file(71);
    dsp_buffer_cache cache({1, false}), invalid({0, false});
    status state;
    auto mapping = cache.vqec_vision_ai_qcom_dspbc_map(file.fd_, g_test_bytes + 1, state);
    if (state.code_ != status_code::invalid_argument || mapping.owner_) {
        return false;
    }
    mapping = invalid.vqec_vision_ai_qcom_dspbc_map(file.fd_, g_test_bytes, state);
    return state.code_ == status_code::invalid_argument && !mapping.owner_;
}

bool vqec_vision_ai_unit_dbctst_reject_registered_memfd() {
    test_file file(79);
    dsp_buffer_cache cache({1, true});
    status state;
    const auto mapping = cache.vqec_vision_ai_qcom_dspbc_map(
        file.fd_, g_test_bytes, state);
    return state.code_ == status_code::unsupported && !mapping.owner_ &&
        cache.vqec_vision_ai_qcom_dspbc_entry_count() == 0;
}

}  // namespace

int main() {
    if (!vqec_vision_ai_unit_dbctst_check_leases() ||
        !vqec_vision_ai_unit_dbctst_check_move_and_fd_reuse() ||
        !vqec_vision_ai_unit_dbctst_check_concurrent_retirement() ||
        !vqec_vision_ai_unit_dbctst_check_invalid_descriptors() ||
        !vqec_vision_ai_unit_dbctst_reject_registered_memfd()) {
        std::cerr << "mapping lease regression failed\n";
        return 1;
    }
    std::cout << "mapping lease regressions passed\n";
    return 0;
}
