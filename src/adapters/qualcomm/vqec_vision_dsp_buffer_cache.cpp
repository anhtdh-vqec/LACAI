#include "vqec_vision_dsp_buffer_cache.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

#if defined(__has_include)
#if __has_include(<remote.h>)
#include <remote.h>
#define VQEC_VISION_AI_HAVE_CDSP 1
#endif
#endif

namespace vqec::vision::ai {

struct dsp_buffer_cache_entry {
    dev_t dev_{0};
    ino_t ino_{0};
    int dup_fd_{-1};
    void* addr_{nullptr};
    std::size_t size_{0};
    std::uint64_t last_used_{0};
    bool fastrpc_mapped_{false};
};

struct dsp_buffer_cache::implementation {
    dsp_buffer_cache_config config_;
    std::mutex mutex_;
    std::vector<dsp_buffer_cache_entry> entries_;
    std::uint64_t clock_{0};

    static void vqec_vision_ai_qcom_dspbc_release_entry(
        dsp_buffer_cache_entry& _entry) noexcept {
#if defined(VQEC_VISION_AI_HAVE_CDSP)
        if (_entry.fastrpc_mapped_ && _entry.dup_fd_ >= 0 &&
            _entry.addr_ != nullptr && _entry.addr_ != MAP_FAILED) {
            (void)::fastrpc_munmap(CDSP_DOMAIN_ID, _entry.dup_fd_, _entry.addr_, _entry.size_);
        }
#endif
        if (_entry.addr_ != nullptr && _entry.addr_ != MAP_FAILED) {
            ::munmap(_entry.addr_, _entry.size_);
            _entry.addr_ = nullptr;
        }
        if (_entry.dup_fd_ >= 0) {
            ::close(_entry.dup_fd_);
            _entry.dup_fd_ = -1;
        }
        _entry.fastrpc_mapped_ = false;
    }
};

dsp_buffer_cache::dsp_buffer_cache(dsp_buffer_cache_config _config)
    : implementation_(std::make_unique<implementation>()) {
    implementation_->config_ = std::move(_config);
    if (implementation_->config_.max_entries_ < 2) {
        implementation_->config_.max_entries_ = 2;
    }
}

dsp_buffer_cache::~dsp_buffer_cache() noexcept {
    vqec_vision_ai_qcom_dspbc_clear();
}

dsp_buffer_cache::dsp_buffer_cache(dsp_buffer_cache&&) noexcept = default;
dsp_buffer_cache& dsp_buffer_cache::operator=(dsp_buffer_cache&&) noexcept = default;

const std::uint8_t* dsp_buffer_cache::vqec_vision_ai_qcom_dspbc_map(
    int _fd, std::size_t _size, status& _status) {
    if (_fd < 0 || _size == 0) {
        _status = {status_code::invalid_argument,
            "dsp_buffer_cache requires a valid file descriptor and nonzero size"};
        return nullptr;
    }
    struct stat st{};
    if (::fstat(_fd, &st) != 0) {
        _status = {status_code::io_error,
            "dsp_buffer_cache fstat on dma-buf fd failed"};
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(implementation_->mutex_);
    for (auto& entry : implementation_->entries_) {
        if (entry.dev_ == st.st_dev && entry.ino_ == st.st_ino) {
            if (entry.size_ < _size) {
                _status = {status_code::protocol_error,
                    "dsp_buffer_cache buffer re-seen with larger size"};
                return nullptr;
            }
            entry.last_used_ = ++implementation_->clock_;
            _status = {};
            return static_cast<const std::uint8_t*>(entry.addr_);
        }
    }

    while (implementation_->entries_.size() >= implementation_->config_.max_entries_) {
        auto victim = implementation_->entries_.begin();
        for (auto it = implementation_->entries_.begin();
             it != implementation_->entries_.end(); ++it) {
            if (it->last_used_ < victim->last_used_) {
                victim = it;
            }
        }
        implementation::vqec_vision_ai_qcom_dspbc_release_entry(*victim);
        implementation_->entries_.erase(victim);
    }

    const int dup_fd = ::fcntl(_fd, F_DUPFD_CLOEXEC, 0);
    if (dup_fd < 0) {
        _status = {status_code::io_error,
            "dsp_buffer_cache duplicate dma-buf fd failed"};
        return nullptr;
    }

    void* addr = ::mmap(nullptr, _size, PROT_READ, MAP_SHARED, dup_fd, 0);
    if (addr == MAP_FAILED) {
        ::close(dup_fd);
        _status = {status_code::resource_exhausted,
            "dsp_buffer_cache mmap on dma-buf fd failed"};
        return nullptr;
    }

    bool fastrpc_mapped = false;
#if defined(VQEC_VISION_AI_HAVE_CDSP)
    const int rc = ::fastrpc_mmap(CDSP_DOMAIN_ID, dup_fd, addr, 0, _size, FASTRPC_MAP_FD);
    if (rc == 0) {
        fastrpc_mapped = true;
    }
#endif

    dsp_buffer_cache_entry created;
    created.dev_ = st.st_dev;
    created.ino_ = st.st_ino;
    created.dup_fd_ = dup_fd;
    created.addr_ = addr;
    created.size_ = _size;
    created.last_used_ = ++implementation_->clock_;
    created.fastrpc_mapped_ = fastrpc_mapped;

    implementation_->entries_.push_back(created);
    _status = {};
    return static_cast<const std::uint8_t*>(addr);
}

void dsp_buffer_cache::vqec_vision_ai_qcom_dspbc_clear() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(implementation_->mutex_);
    for (auto& entry : implementation_->entries_) {
        implementation::vqec_vision_ai_qcom_dspbc_release_entry(entry);
    }
    implementation_->entries_.clear();
}

std::size_t dsp_buffer_cache::vqec_vision_ai_qcom_dspbc_entry_count() const noexcept {
    if (implementation_ == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(implementation_->mutex_);
    return implementation_->entries_.size();
}

}  // namespace vqec::vision::ai
