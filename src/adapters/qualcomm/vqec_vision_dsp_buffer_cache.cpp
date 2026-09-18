#include "vqec_vision_dsp_buffer_cache.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <limits>
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
    bool initializing_{true};
    bool retired_{false};

    ~dsp_buffer_cache_entry() noexcept {
#if defined(VQEC_VISION_AI_HAVE_CDSP)
        if (fastrpc_mapped_) {
            (void)::fastrpc_munmap(CDSP_DOMAIN_ID, dup_fd_, addr_, size_);
        }
#endif
        if (addr_ != nullptr && addr_ != MAP_FAILED) {
            (void)::munmap(addr_, size_);
        }
        if (dup_fd_ >= 0) {
            (void)::close(dup_fd_);
        }
    }
};

struct dsp_buffer_cache::implementation {
    dsp_buffer_cache_config config_;
    std::mutex mutex_;
    std::vector<std::shared_ptr<dsp_buffer_cache_entry>> entries_;
    std::uint64_t clock_{0};
};

dsp_buffer_cache::dsp_buffer_cache(dsp_buffer_cache_config _config)
    : implementation_(std::make_unique<implementation>()) {
    implementation_->config_ = std::move(_config);
    implementation_->entries_.reserve(implementation_->config_.max_entries_);
}

dsp_buffer_cache::~dsp_buffer_cache() noexcept = default;
dsp_buffer_cache::dsp_buffer_cache(dsp_buffer_cache&&) noexcept = default;
dsp_buffer_cache& dsp_buffer_cache::operator=(dsp_buffer_cache&&) noexcept = default;

dsp_buffer_mapping dsp_buffer_cache::vqec_vision_ai_qcom_dspbc_map(
    int _fd, std::size_t _size, status& _status) {
    if (implementation_ == nullptr || implementation_->config_.max_entries_ == 0 ||
        _fd < 0 || _size == 0 ||
        _size > static_cast<std::size_t>(std::numeric_limits<off_t>::max())) {
        _status = {status_code::invalid_argument, "invalid mapping/cache descriptor"};
        return {};
    }
    struct stat descriptor{};
    if (::fstat(_fd, &descriptor) != 0) {
        _status = {status_code::io_error, "cannot resolve mapping allocation identity"};
        return {};
    }
    if (descriptor.st_size > 0 &&
        _size > static_cast<std::uint64_t>(descriptor.st_size)) {
        _status = {status_code::invalid_argument, "mapping exceeds allocation size"};
        return {};
    }
    std::shared_ptr<dsp_buffer_cache_entry> created;
    std::shared_ptr<dsp_buffer_cache_entry> victim;
    {
        std::lock_guard<std::mutex> lock(implementation_->mutex_);
        auto& entries = implementation_->entries_;
        auto selected = entries.end();
        for (const auto& entry : entries) {
            if (entry->dev_ != descriptor.st_dev || entry->ino_ != descriptor.st_ino) {
                continue;
            }
            if (entry->size_ < _size) {
                _status = {status_code::protocol_error, "allocation mapping size changed"};
                return {};
            }
            if (entry->initializing_ || entry->retired_) {
                if (entry.use_count() > 1 || entry->initializing_) {
                    _status = {status_code::pending, "allocation mapping is initializing/retired"};
                    return {};
                }
                selected = std::find(entries.begin(), entries.end(), entry);
                break;
            }
            entry->last_used_ = ++implementation_->clock_;
            _status = {};
            return {static_cast<const std::uint8_t*>(entry->addr_), entry->size_, entry};
        }
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (selected != entries.end() && (*selected)->retired_) {
                break;
            }
            if (it->use_count() != 1) {
                continue;
            }
            if ((*it)->retired_) {
                selected = it;
                break;
            }
            if (entries.size() >= implementation_->config_.max_entries_ &&
                (selected == entries.end() || (*it)->last_used_ < (*selected)->last_used_)) {
                selected = it;
            }
        }
        if (selected != entries.end()) {
            victim = std::move(*selected);
            entries.erase(selected);
        }
        if (entries.size() >= implementation_->config_.max_entries_) {
            _status = {status_code::resource_exhausted, "all mapping cache slots are leased"};
            return {};
        }
        created = std::make_shared<dsp_buffer_cache_entry>();
        created->dev_ = descriptor.st_dev;
        created->ino_ = descriptor.st_ino;
        created->size_ = _size;
        created->last_used_ = ++implementation_->clock_;
        entries.push_back(created); // Reserve capacity before any mapping SDK call.
    }
    victim.reset(); // No unmapping SDK call under the bookkeeping lock.
    created->dup_fd_ = ::fcntl(_fd, F_DUPFD_CLOEXEC, 0);
    struct stat retained{};
    status mapped;
    if (created->dup_fd_ < 0 || ::fstat(created->dup_fd_, &retained) != 0 ||
        retained.st_dev != descriptor.st_dev || retained.st_ino != descriptor.st_ino) {
        mapped = {status_code::io_error, "cannot retain original allocation identity"};
    } else {
        created->addr_ = ::mmap(nullptr, _size, PROT_READ, MAP_SHARED, created->dup_fd_, 0);
        if (created->addr_ == MAP_FAILED) {
            mapped = {status_code::resource_exhausted, "cannot map retained allocation"};
        } else if (implementation_->config_.enable_fastrpc_) {
#if defined(VQEC_VISION_AI_HAVE_CDSP)
            const int result = ::fastrpc_mmap(CDSP_DOMAIN_ID, created->dup_fd_,
                created->addr_, 0, _size, FASTRPC_MAP_FD);
            created->fastrpc_mapped_ = result == 0;
            if (result != 0) {
                mapped = {status_code::io_error, "FastRPC allocation registration failed"};
            }
#else
            mapped = {status_code::unsupported, "FastRPC mapping API is unavailable"};
#endif
        }
    }
    {
        std::lock_guard<std::mutex> lock(implementation_->mutex_);
        created->initializing_ = false;
        if (created->retired_ && mapped.code_ == status_code::ok) {
            mapped = {status_code::invalid_state, "mapping retired during initialization"};
        }
        if (mapped.code_ != status_code::ok) {
            auto& entries = implementation_->entries_;
            entries.erase(std::remove(entries.begin(), entries.end(), created), entries.end());
        }
    }
    _status = mapped;
    if (mapped.code_ != status_code::ok) {
        return {};
    }
    return {static_cast<const std::uint8_t*>(created->addr_), created->size_, created};
}

void dsp_buffer_cache::vqec_vision_ai_qcom_dspbc_clear() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    for (;;) {
        std::shared_ptr<dsp_buffer_cache_entry> removed;
        {
            std::lock_guard<std::mutex> lock(implementation_->mutex_);
            auto& entries = implementation_->entries_;
            for (const auto& entry : entries) {
                entry->retired_ = true;
            }
            const auto selected = std::find_if(entries.begin(), entries.end(),
                [](const auto& _entry) { return _entry.use_count() == 1; });
            if (selected == entries.end()) {
                return;
            }
            removed = std::move(*selected);
            entries.erase(selected);
        }
    }
}

std::size_t dsp_buffer_cache::vqec_vision_ai_qcom_dspbc_entry_count() const noexcept {
    if (implementation_ == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(implementation_->mutex_);
    return implementation_->entries_.size();
}

}  // namespace vqec::vision::ai
