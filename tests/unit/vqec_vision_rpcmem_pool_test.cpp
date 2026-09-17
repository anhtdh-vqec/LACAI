#include <cstdint>
#include <iostream>
#include <vector>

#include "vqec_vision_rpcmem_pool.hpp"

namespace {

int vqec_vision_ai_unit_rpmpt_test_basic_allocation() {
    vqec::vision::ai::rpcmem_pool pool;
    if (pool.vqec_vision_ai_qcom_rpcm_count() != 0) {
        std::cerr << "Initial count must be 0\n";
        return 1;
    }

    const auto status = pool.vqec_vision_ai_qcom_rpcm_allocate(1024, 2);
    if (status.code_ != vqec::vision::ai::status_code::ok) {
        std::cerr << "Allocation failed: " << status.message_ << "\n";
        return 1;
    }

    if (pool.vqec_vision_ai_qcom_rpcm_count() != 2) {
        std::cerr << "Expected 2 slots\n";
        return 1;
    }

    const auto& slot0 = pool.vqec_vision_ai_qcom_rpcm_slot(0);
    const auto& slot1 = pool.vqec_vision_ai_qcom_rpcm_slot(1);
    if (slot0.data_ == nullptr || slot0.fd_ < 0 || slot0.size_ < 1024) {
        std::cerr << "Slot 0 invalid\n";
        return 1;
    }
    if (slot1.data_ == nullptr || slot1.fd_ < 0 || slot1.size_ < 1024) {
        std::cerr << "Slot 1 invalid\n";
        return 1;
    }

    // Verify 4096 alignment
    if (reinterpret_cast<std::uintptr_t>(slot0.data_) % 4096 != 0 ||
        reinterpret_cast<std::uintptr_t>(slot1.data_) % 4096 != 0) {
        std::cerr << "Slots not page-aligned\n";
        return 1;
    }

    pool.vqec_vision_ai_qcom_rpcm_release();
    if (pool.vqec_vision_ai_qcom_rpcm_count() != 0) {
        std::cerr << "Count must be 0 after release\n";
        return 1;
    }
    return 0;
}

int vqec_vision_ai_unit_rpmpt_test_heterogeneous_allocation() {
    vqec::vision::ai::rpcmem_pool pool;
    std::vector<std::size_t> sizes = {640 * 640 * 3 * 2, 4096, 8192};
    const auto status = pool.vqec_vision_ai_qcom_rpcm_allocate(sizes);
    if (status.code_ != vqec::vision::ai::status_code::ok) {
        std::cerr << "Heterogeneous allocation failed: " << status.message_ << "\n";
        return 1;
    }

    if (pool.vqec_vision_ai_qcom_rpcm_count() != 3) {
        std::cerr << "Expected 3 slots\n";
        return 1;
    }

    for (std::size_t i = 0; i < 3; ++i) {
        const auto& s = pool.vqec_vision_ai_qcom_rpcm_slot(i);
        if (s.data_ == nullptr || s.fd_ < 0 || s.size_ < sizes[i]) {
            std::cerr << "Slot " << i << " invalid\n";
            return 1;
        }
    }
    return 0;
}

}  // namespace

int main() {
    if (vqec_vision_ai_unit_rpmpt_test_basic_allocation() != 0) {
        return 1;
    }
    if (vqec_vision_ai_unit_rpmpt_test_heterogeneous_allocation() != 0) {
        return 1;
    }
    std::cout << "All rpcmem_pool tests passed.\n";
    return 0;
}
