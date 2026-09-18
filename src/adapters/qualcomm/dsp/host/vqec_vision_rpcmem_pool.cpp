#include "vqec_vision_rpcmem_pool.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<rpcmem.h>)
#include <rpcmem.h>
#define VQEC_VISION_AI_HAVE_RPCMEM 1
#endif
#endif

namespace {

constexpr std::size_t g_page_alignment = 4096U;

inline std::size_t vqec_vision_ai_qcom_rpcm_round_up(std::size_t _size) noexcept {
    return (_size + (g_page_alignment - 1U)) & ~(g_page_alignment - 1U);
}

int vqec_vision_ai_qcom_rpcm_memfd_create() noexcept {
#if defined(SYS_memfd_create)
    return static_cast<int>(syscall(SYS_memfd_create, "lacai_rpcmem", 0));
#else
    return -1;
#endif
}

}  // namespace

extern "C" {

void* vqec_vision_ai_qcom_rpcm_alloc(std::size_t _size, int* _fd) {
    if (_size == 0 || _fd == nullptr) {
        if (_fd != nullptr) {
            *_fd = -1;
        }
        return nullptr;
    }

#if defined(VQEC_VISION_AI_HAVE_RPCMEM)
    void* ptr = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, static_cast<int>(_size));
    if (ptr != nullptr) {
        const int fd = rpcmem_to_fd(ptr);
        if (fd >= 0) {
            *_fd = fd;
            return ptr;
        }
        rpcmem_free(ptr);
    }
#endif

    // Fallback: memfd + mmap for host tests and environments without rpcmem driver
    const int memfd = vqec_vision_ai_qcom_rpcm_memfd_create();
    if (memfd < 0) {
        *_fd = -1;
        return nullptr;
    }
    if (ftruncate(memfd, static_cast<off_t>(_size)) != 0) {
        close(memfd);
        *_fd = -1;
        return nullptr;
    }
    void* mapped = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
    if (mapped == MAP_FAILED) {
        close(memfd);
        *_fd = -1;
        return nullptr;
    }
    *_fd = memfd;
    return mapped;
}

void vqec_vision_ai_qcom_rpcm_free(void* _ptr, int _fd, std::size_t _size) {
    if (_ptr == nullptr) {
        return;
    }

#if defined(VQEC_VISION_AI_HAVE_RPCMEM)
    // Try freeing via rpcmem; if it was allocated via mmap fallback, munmap it
    // rpcmem_to_fd returns valid fd for rpcmem buffers
    if (rpcmem_to_fd(_ptr) >= 0) {
        rpcmem_free(_ptr);
        return;
    }
#endif

    munmap(_ptr, _size);
    if (_fd >= 0) {
        close(_fd);
    }
}

}  // extern "C"

namespace vqec::vision::ai {

rpcmem_pool::~rpcmem_pool() {
    vqec_vision_ai_qcom_rpcm_release();
}

rpcmem_pool::rpcmem_pool(rpcmem_pool&& _other) noexcept
    : slots_(std::move(_other.slots_)) {}

rpcmem_pool& rpcmem_pool::operator=(rpcmem_pool&& _other) noexcept {
    if (this != &_other) {
        vqec_vision_ai_qcom_rpcm_release();
        slots_ = std::move(_other.slots_);
    }
    return *this;
}

void rpcmem_pool::vqec_vision_ai_qcom_rpcm_release() noexcept {
    for (auto& slot : slots_) {
        if (slot.data_ != nullptr) {
            vqec_vision_ai_qcom_rpcm_free(slot.data_, slot.fd_, slot.size_);
            slot.data_ = nullptr;
            slot.fd_ = -1;
            slot.size_ = 0;
        }
    }
    slots_.clear();
}

status rpcmem_pool::vqec_vision_ai_qcom_rpcm_allocate(
    std::size_t _size, std::size_t _count) {
    return vqec_vision_ai_qcom_rpcm_allocate(std::vector<std::size_t>(_count, _size));
}

status rpcmem_pool::vqec_vision_ai_qcom_rpcm_allocate(
    const std::vector<std::size_t>& _sizes) {
    vqec_vision_ai_qcom_rpcm_release();
    slots_.reserve(_sizes.size());

    for (std::size_t i = 0; i < _sizes.size(); ++i) {
        const std::size_t aligned = vqec_vision_ai_qcom_rpcm_round_up(_sizes[i]);
        if (aligned == 0) {
            vqec_vision_ai_qcom_rpcm_release();
            return {status_code::invalid_argument,
                "Requested zero-sized rpcmem allocation at index " + std::to_string(i)};
        }
        int fd = -1;
        void* ptr = vqec_vision_ai_qcom_rpcm_alloc(aligned, &fd);
        if (ptr == nullptr || fd < 0) {
            vqec_vision_ai_qcom_rpcm_release();
            return {status_code::resource_exhausted,
                "Failed to allocate rpcmem slot " + std::to_string(i) + " size " + std::to_string(aligned)};
        }
        std::memset(ptr, 0, aligned);
        slots_.push_back(rpcmem_buffer{ptr, fd, aligned});
    }

    return {status_code::ok, ""};
}

const rpcmem_buffer& rpcmem_pool::vqec_vision_ai_qcom_rpcm_slot(
    std::size_t _index) const {
    static const rpcmem_buffer g_empty_buffer{};
    if (_index >= slots_.size()) {
        return g_empty_buffer;
    }
    return slots_[_index];
}

std::size_t rpcmem_pool::vqec_vision_ai_qcom_rpcm_count() const noexcept {
    return slots_.size();
}

}  // namespace vqec::vision::ai
