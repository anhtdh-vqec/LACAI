#include <iostream>
#include <string>
#include <vector>

#include "vqec_vision_service_options.hpp"

namespace {

// Builds a mutable argv from string literals; program name is prepended by the caller use.
bool vqec_vision_ai_unit_sotst_parse(
    const std::vector<std::string>& _tokens,
    vqec::vision::ai::parsed_arguments& _args) {
    std::vector<std::string> storage = _tokens;
    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (auto& token : storage) {
        argv.push_back(token.data());
    }
    return vqec::vision::ai::vqec_vision_ai_appl_svopt_parse(
        static_cast<int>(argv.size()), argv.data(), _args);
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition, const char* _what) {
        if (!_condition) {
            ++failures;
            std::cerr << "failed: " << _what << '\n';
        }
    };

    check(parsed_arguments{}.runtime_step_interval_ns ==
              service_options_limits::g_default_runtime_step_interval_ns,
        "default step interval");
    check(parsed_arguments{}.source_recovery_backoff_ms ==
              service_options_limits::g_default_source_recovery_backoff_ms,
        "default source recovery backoff");

    {
        parsed_arguments args;
        check(vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d.json", "--model-catalog", "c.json"}, args),
            "minimal valid parse");
        check(args.deployment_path == "d.json" && args.catalog_path == "c.json",
            "deployment/catalog paths captured");
        check(args.platform == "none" && !args.production_mode, "harness defaults");
    }
    {
        parsed_arguments args;
        check(vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--app-manager-dbus", "--app-manager-service-name", "com.vqec.Manager",
                      "--app-manager-client-name", "com.vqec.Runtime",
                      "--app-manager-object-path", "/com/vqec/Manager",
                      "--app-manager-rpc-timeout-ms", "1000",
                      "--app-manager-poll-interval-ms", "250"}, args),
            "app manager poll interval parse");
        check(args.app_manager_poll_interval_ms == 250,
            "app manager poll interval captured");
    }
    {
        parsed_arguments args;
        check(!vqec_vision_ai_unit_sotst_parse({"app", "--nonsense", "x"}, args),
            "unknown option rejected");
        check(!vqec_vision_ai_unit_sotst_parse({"app", "--deployment", "d.json"}, args),
            "missing catalog rejected");
        check(!vqec_vision_ai_unit_sotst_parse({"app", "--deployment"}, args),
            "incomplete option rejected");
    }
    {
        parsed_arguments args;
        check(vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--runtime-step-interval-us", "15000"},
                  args),
            "step interval parse");
        check(args.runtime_step_interval_ns == 15000000ULL, "step interval converted to ns");

        parsed_arguments recovery;
        check(vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--source-recovery-backoff-ms", "250"},
                  recovery),
            "source recovery backoff parse");
        check(recovery.source_recovery_backoff_ms == 250,
            "source recovery backoff captured");

        parsed_arguments zero;
        check(!vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--runtime-step-interval-us", "0"},
                  zero),
            "zero step interval rejected");

        parsed_arguments zero_recovery;
        check(!vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--source-recovery-backoff-ms", "0"},
                  zero_recovery),
            "zero source recovery backoff rejected");
    }
    {
        parsed_arguments args;
        check(vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--inference-perf-profile", "low_latency", "--mode", "production",
                      "--platform", "qualcomm", "--enrollment-image-root", "a",
                      "--enrollment-image-root", "b", "--allow-qaic-copy-input"},
                  args),
            "production parse");
        check(args.production_mode && args.platform == "qualcomm", "production mode/platform");
        check(args.execution_policy.profile_ == inference_perf_profile::low_latency,
            "perf profile captured");
        check(args.enrollment_image_roots.size() == 2, "repeated image roots accumulate");
        check(args.allow_qaic_copy_input, "explicit QAIC copy input captured");

        parsed_arguments bad_profile;
        check(!vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--inference-perf-profile", "turbo"},
                  bad_profile),
            "unknown perf profile rejected");
    }
    {
        parsed_arguments args;
        check(vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--model-root", "/opt/lacai/models",
                      "--dsp-v1-skel-dir", "/opt/lacai/dsp/v1",
                      "--dsp-legacy-skel-dir", "/opt/lacai/dsp/legacy",
                      "--dsp-legacy-clock-corner", "7",
                      "--dsp-legacy-latency-us", "100", "--dsp-enable-unsigned-pd",
                      "--hardware-profile", "/opt/lacai/config/hardware_profile.json",
                      "--metadata-profile", "/opt/lacai/config/metadata_runtime_profile.json",
                      "--evidence-socket", "/run/lacai/evidence.sock",
                      "--evidence-outbox", "/opt/lacai/data/evidence/outbox.db",
                      "--evidence-peer-uid", "0", "--evidence-io-timeout-ms", "500",
                      "--evidence-outbox-busy-timeout-ms", "1000",
                      "--evidence-outbox-max-bytes", "67108864",
                      "--evidence-initial-retry-ms", "100",
                      "--evidence-maximum-retry-ms", "10000",
                      "--evidence-idle-poll-ms", "50",
                      "--evidence-stop-drain-ms", "2000",
                      "--evidence-maximum-attempts", "12",
                      "--max-artifact-bytes", "134217728"},
                  args),
            "model root and max artifact bytes parse");
        check(args.model_root == "/opt/lacai/models", "model_root captured");
        check(args.dsp_v1_skel_dir == "/opt/lacai/dsp/v1", "DSP v1 path captured");
        check(args.dsp_legacy_skel_dir == "/opt/lacai/dsp/legacy",
            "legacy DSP path captured");
        check(args.dsp_legacy_clock_corner == 7 && args.dsp_legacy_latency_us == 100,
            "legacy DSP clock policy captured");
        check(args.dsp_enable_unsigned_pd, "unsigned DSP policy captured");
        check(args.hardware_profile_path == "/opt/lacai/config/hardware_profile.json",
            "hardware profile path captured");
        check(args.metadata_profile_path ==
                "/opt/lacai/config/metadata_runtime_profile.json",
            "metadata profile path captured");
        check(args.evidence_socket_path == "/run/lacai/evidence.sock" &&
                args.evidence_outbox_path == "/opt/lacai/data/evidence/outbox.db" &&
                args.evidence_peer_uid_set && args.evidence_peer_uid == 0U,
            "evidence endpoints captured");
        check(args.evidence_io_timeout_ms == 500 &&
                args.evidence_outbox_busy_timeout_ms == 1000 &&
                args.evidence_outbox_max_bytes == 67108864ULL &&
                args.evidence_initial_retry_ms == 100U &&
                args.evidence_maximum_retry_ms == 10000U &&
                args.evidence_idle_poll_ms == 50U &&
                args.evidence_stop_drain_ms == 2000U &&
                args.evidence_maximum_attempts == 12U,
            "evidence retry and storage policy captured");
        check(args.max_artifact_bytes == 134217728ULL, "max_artifact_bytes captured");
    }

    std::cout << "service options failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
