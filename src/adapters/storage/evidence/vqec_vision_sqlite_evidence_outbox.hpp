#ifndef VQEC_VISION_AI_ADAPTERS_STORAGE_SQLITE_EVIDENCE_OUTBOX_HPP
#define VQEC_VISION_AI_ADAPTERS_STORAGE_SQLITE_EVIDENCE_OUTBOX_HPP

#include <cstdint>
#include <mutex>
#include <string>

#include "vqec/vision/ai/ports/output/vqec_vision_evidence_outbox.hpp"

struct sqlite3;

namespace vqec::vision::ai {

struct sqlite_evidence_outbox_config {
    std::string database_path_;
    std::uint64_t maximum_database_bytes_{0};
    int busy_timeout_ms_{0};
};

class sqlite_evidence_outbox final : public evidence_outbox_port {
public:
    explicit sqlite_evidence_outbox(sqlite_evidence_outbox_config _config);
    ~sqlite_evidence_outbox() noexcept override;

    sqlite_evidence_outbox(const sqlite_evidence_outbox&) = delete;
    sqlite_evidence_outbox& operator=(const sqlite_evidence_outbox&) = delete;

    [[nodiscard]] status vqec_vision_ai_ports_evobx_open() override;
    [[nodiscard]] status vqec_vision_ai_ports_evobx_enqueue(
        const evidence_command& _command) override;
    [[nodiscard]] status vqec_vision_ai_ports_evobx_claim_due(
        std::uint64_t _now_monotonic_ns, evidence_outbox_record& _record) override;
    [[nodiscard]] status vqec_vision_ai_ports_evobx_retry(
        const std::string& _request_id, std::size_t _attempt_count,
        std::uint64_t _next_attempt_monotonic_ns, const std::string& _reason) override;
    [[nodiscard]] status vqec_vision_ai_ports_evobx_complete(
        const evidence_receipt& _receipt) override;
    [[nodiscard]] status vqec_vision_ai_ports_evobx_get_stats(
        evidence_outbox_stats& _stats) const override;

private:
    sqlite_evidence_outbox_config config_;
    sqlite3* database_{nullptr};
    evidence_outbox_stats stats_{};
    mutable std::mutex mutex_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_ADAPTERS_STORAGE_SQLITE_EVIDENCE_OUTBOX_HPP
