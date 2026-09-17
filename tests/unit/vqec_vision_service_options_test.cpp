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

        parsed_arguments zero;
        check(!vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--runtime-step-interval-us", "0"},
                  zero),
            "zero step interval rejected");
    }
    {
        parsed_arguments args;
        check(vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--inference-perf-profile", "low_latency", "--mode", "production",
                      "--platform", "qualcomm", "--enrollment-image-root", "a",
                      "--enrollment-image-root", "b"},
                  args),
            "production parse");
        check(args.production_mode && args.platform == "qualcomm", "production mode/platform");
        check(args.execution_policy.profile_ == inference_perf_profile::low_latency,
            "perf profile captured");
        check(args.enrollment_image_roots.size() == 2, "repeated image roots accumulate");

        parsed_arguments bad_profile;
        check(!vqec_vision_ai_unit_sotst_parse(
                  {"app", "--deployment", "d", "--model-catalog", "c",
                      "--inference-perf-profile", "turbo"},
                  bad_profile),
            "unknown perf profile rejected");
    }

    std::cout << "service options failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
