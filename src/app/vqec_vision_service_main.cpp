// LACAI service composition root. This is the required externally named executable
// `vqec_ai_vision_applications`. It loads validated deployment/model/feature metadata,
// constructs the neutral runtime bundle with the device-free reference backend and a
// built-in development fixture package set, then runs the serialized executor loop.
//
// The fixture decoder/tracker/feature produce no real detections and prove nothing about
// model accuracy, Qualcomm support or hardware completion. A usecase integrator replaces
// them by registering real package factories before bundle construction, or by swapping
// the reference platform owners for the Camera/Qualcomm adapters. See
// docs/architecture/runtime_executor.md.

#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "vqec_vision_deployment_config.hpp"
#include "vqec_vision_feature_activation_manager.hpp"
#include "vqec_vision_feature_catalog.hpp"
#include "vqec_vision_feature_fanout.hpp"
#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_model_catalog.hpp"
#include "vqec_vision_reference_graph.hpp"
#include "vqec_vision_reference_sink.hpp"
#include "vqec_vision_reference_source.hpp"
#include "vqec_vision_runtime_composition_factory.hpp"

using namespace vqec::vision::ai;

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;

void vqec_vision_ai_appl_svcmn_on_signal(int) {
    g_stop_requested = 1;
}

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;
constexpr char g_reference_tracker_contract[] = "reference.tracker.v1";
constexpr std::uint64_t g_step_interval_ns = 1000000;

// --- development fixture package set (not a model or a qualified usecase) ---------------

class fixture_decoder final : public model_decoder_port {
public:
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override {
        return _outputs.outputs_.empty() ?
            status{status_code::unsupported, "fixture decoder requires an output"} : status{};
    }
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override {
        (void)_result;
        observation_batch candidate;
        candidate.frame_ = _expected_frame;
        candidate.geometry_ = {width_, height_};
        observation item;
        item.frame_ = _expected_frame;
        item.class_id_ = "person";
        item.box_ = {0.0F, 0.0F, 10.0F, 10.0F, 0xffffffffU, "person"};
        item.confidence_ = 0.5F;
        item.quality_ = observation_quality::low;
        candidate.observations_.push_back(std::move(item));
        _observations = std::move(candidate);
        return {};
    }

    // The development harness assumes one uniform source geometry per deployment.
    std::uint32_t width_{640};
    std::uint32_t height_{480};
};

class fixture_tracker final : public tracker_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_trker_validate_activation() const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_update_tracks(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        observation_batch candidate = _detections;
        for (auto& item : candidate.observations_) {
            item.track_id_ = ++assigned_;
        }
        _tracked = std::move(candidate);
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        assigned_ = 0;
        return {};
    }

    std::uint64_t assigned_{0};
};

class fixture_tracker_factory final : public tracker_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        (void)_model_id;
        return !_source_id.empty() ? status{} :
            status{status_code::unsupported, "fixture tracker requires a source"};
    }
    [[nodiscard]] status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) override {
        (void)_source_id;
        (void)_model_id;
        _tracker = std::make_unique<fixture_tracker>();
        return {};
    }
};

class fixture_feature final : public feature_processor_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        config_ = _config;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        next_event_id_ = 0;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        feature_event_batch candidate;
        candidate.frame_ = _tracked.frame_;
        candidate.geometry_ = _tracked.geometry_;
        for (const auto& item : _tracked.observations_) {
            if (item.track_id_ == 0) {
                continue;
            }
            feature_event event;
            event.frame_ = _tracked.frame_;
            event.source_id_ = config_.source_id_;
            event.feature_id_ = config_.feature_id_;
            event.event_id_ = "fixture_" + std::to_string(++next_event_id_);
            event.event_schema_id_ = "fixture.event";
            event.event_schema_version_ = "1";
            event.kind_ = feature_event_kind::snapshot;
            event.occurred_at_ns_ = _tracked.frame_.source_pts_ns_;
            event.config_revision_ = config_.config_revision_;
            event.track_ids_.push_back(item.track_id_);
            feature_event_field field;
            field.schema_id_ = "fixture.attribute";
            field.schema_version_ = "1";
            field.value_ = "present";
            field.confidence_ = 0.5F;
            field.quality_ = observation_quality::low;
            event.fields_.push_back(std::move(field));
            candidate.events_.push_back(std::move(event));
        }
        _events = std::move(candidate);
        return {};
    }

    mutable feature_processor_config config_;
    std::uint64_t next_event_id_{0};
};

class fixture_feature_factory final : public feature_processor_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        _processor = std::make_unique<fixture_feature>();
        return {};
    }
};

// --- loading helpers --------------------------------------------------------------------

bool vqec_vision_ai_appl_svcmn_load_model_catalog(
    const std::string& _path, model_catalog& _catalog) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open model catalog: %s\n", _path.c_str());
        return false;
    }
    std::uint64_t resident_bytes = 0;
    const auto loaded = vqec_vision_ai_mreg_mdcat_load_catalog(stream, _catalog, resident_bytes);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "model catalog rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svcmn_load_deployment(
    const std::string& _path, deployment_config& _deployment) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open deployment config: %s\n", _path.c_str());
        return false;
    }
    std::uint64_t resident_bytes = 0;
    const auto loaded = vqec_vision_ai_life_dpcfg_load(stream, _deployment, resident_bytes);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "deployment config rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svcmn_load_feature_catalog(
    const std::string& _path, feature_catalog& _features) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open feature catalog: %s\n", _path.c_str());
        return false;
    }
    const auto loaded = vqec_vision_ai_ftmgr_ftcat_load_catalog(stream, _features);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "feature catalog rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

std::string vqec_vision_ai_appl_svcmn_dev_model_path(const std::string& _model_id) {
    return "/opt/vqec/models/" + _model_id + ".bin";
}

// Builds the parsed output metadata the runtime validates against the catalog identity.
// Device-free harness only; a real deployment reads the model package output manifest.
model_outputs vqec_vision_ai_appl_svcmn_synthetic_outputs(const model_catalog_entry& _model) {
    model_outputs outputs;
    outputs.model_id_ = _model.model_id_;
    outputs.model_version_ = _model.model_version_;
    outputs.artifact_sha256_ = _model.artifact_sha256_;
    outputs.decoder_contract_ = _model.decoder_contract_;
    outputs.max_output_bytes_ = 16;
    outputs.outputs_.push_back({"boxes", {1, 4}});
    return outputs;
}

struct parsed_arguments {
    std::string deployment_path;
    std::string catalog_path;
    std::string feature_catalog_path;
    std::uint64_t max_steps{0};
    std::uint32_t require_sources{0};
};

bool vqec_vision_ai_appl_svcmn_parse(int _argc, char** _argv, parsed_arguments& _args) {
    for (int index = 1; index < _argc; ++index) {
        const std::string option = _argv[index];
        const bool has_value = index + 1 < _argc;
        if (option == "--deployment" && has_value) {
            _args.deployment_path = _argv[++index];
        } else if (option == "--model-catalog" && has_value) {
            _args.catalog_path = _argv[++index];
        } else if (option == "--feature-catalog" && has_value) {
            _args.feature_catalog_path = _argv[++index];
        } else if (option == "--steps" && has_value) {
            _args.max_steps = std::strtoull(_argv[++index], nullptr, 10);
        } else if (option == "--require-sources" && has_value) {
            _args.require_sources = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 10));
        } else {
            std::fprintf(stderr, "unknown or incomplete argument: %s\n", option.c_str());
            return false;
        }
    }
    return !_args.deployment_path.empty() && !_args.catalog_path.empty();
}

std::uint16_t vqec_vision_ai_appl_svcmn_model_slot(
    const source_deployment_config& _source, const std::string& _model_id) {
    for (std::uint16_t slot = 0; slot < _source.model_ids_.size(); ++slot) {
        if (_source.model_ids_[slot] == _model_id) {
            return slot;
        }
    }
    return g_invalid_model_slot;
}

const model_catalog_entry* vqec_vision_ai_appl_svcmn_find_model(
    const model_catalog& _catalog, const std::string& _model_id) {
    for (const auto& model : _catalog.models_) {
        if (model.model_id_ == _model_id) {
            return &model;
        }
    }
    return nullptr;
}

}  // namespace

int main(int _argc, char** _argv) {
    parsed_arguments args;
    if (!vqec_vision_ai_appl_svcmn_parse(_argc, _argv, args)) {
        std::fprintf(stderr,
            "usage: vqec_ai_vision_applications --deployment <json> --model-catalog <json> "
            "[--feature-catalog <json>] [--steps <n>] [--require-sources <n>]\n");
        return 2;
    }
    std::signal(SIGINT, vqec_vision_ai_appl_svcmn_on_signal);
    std::signal(SIGTERM, vqec_vision_ai_appl_svcmn_on_signal);

    model_catalog catalog;
    deployment_config deployment;
    feature_catalog features;
    if (!vqec_vision_ai_appl_svcmn_load_model_catalog(args.catalog_path, catalog) ||
        !vqec_vision_ai_appl_svcmn_load_deployment(args.deployment_path, deployment)) {
        return 1;
    }
    if (!args.feature_catalog_path.empty() &&
        !vqec_vision_ai_appl_svcmn_load_feature_catalog(args.feature_catalog_path, features)) {
        return 1;
    }

    if (deployment.sources_.size() > deployment_limits::g_max_sources) {
        std::fprintf(stderr, "deployment source count exceeds runtime support\n");
        return 1;
    }

    // Fixture packages: registered for every catalog contract so the harness can run.
    fixture_decoder decoder;
    decoder.width_ = deployment.sources_.front().profile_.width_;
    decoder.height_ = deployment.sources_.front().profile_.height_;
    model_decoder_registry decoders;
    for (const auto& model : catalog.models_) {
        if (decoders.vqec_vision_ai_detec_mdreg_register_decoder(
                model.decoder_contract_, decoder).code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register fixture decoder: %s\n",
                model.decoder_contract_.c_str());
            return 1;
        }
    }
    fixture_tracker_factory tracker_factory;
    tracker_registry trackers;
    if (trackers.vqec_vision_ai_track_trreg_register_factory(
            g_reference_tracker_contract, tracker_factory).code_ != status_code::ok) {
        std::fprintf(stderr, "cannot register fixture tracker\n");
        return 1;
    }
    fixture_feature_factory feature_factory;
    feature_processor_registry feature_registry;
    for (const auto& feature : features.features_) {
        if (feature_registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
                feature.processor_contract_, feature_factory).code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register fixture feature: %s\n",
                feature.processor_contract_.c_str());
            return 1;
        }
    }

    // Output boundary for the harness: a permissive-but-explicit policy plus a
    // development sink. The gate still denies any event whose attributes are unlisted.
    output_gate output_policy_gate;
    reference_event_sink event_sink;

    // Platform owners: reference backend for every deployment source/model slot.
    std::vector<std::unique_ptr<reference_raw_source>> sources;
    std::vector<std::unique_ptr<reference_inference_graph>> graphs;
    sources.reserve(deployment.sources_.size());
    graphs.reserve(deployment.sources_.size() * deployment_limits::g_max_models_per_source);
    for (const auto& source : deployment.sources_) {
        sources.push_back(std::make_unique<reference_raw_source>(
            reference_source_config{source.profile_.width_, source.profile_.height_,
                source.profile_.fps_numerator_, source.profile_.fps_denominator_}));
    }

    runtime_composition_activation activation;
    activation.source_count_ = static_cast<std::uint16_t>(deployment.sources_.size());
    activation.startup_timeout_ns_ = 30000000000ULL;
    activation.stop_timeout_ns_ = 10000000000ULL;
    activation.rpc_timeout_ms_ = 1000;
    for (std::uint16_t source_slot = 0; source_slot < activation.source_count_; ++source_slot) {
        const auto& source = deployment.sources_[source_slot];
        auto& source_activation = activation.sources_[source_slot];
        source_activation.source_id_ = source.source_id_;
        source_activation.source_ = sources[source_slot].get();
        source_activation.model_count_ = static_cast<std::uint16_t>(source.model_ids_.size());
        for (std::uint16_t model_slot = 0; model_slot < source_activation.model_count_;
             ++model_slot) {
            const auto* model = vqec_vision_ai_appl_svcmn_find_model(
                catalog, source.model_ids_[model_slot]);
            if (model == nullptr) {
                std::fprintf(stderr, "deployment references unknown model: %s\n",
                    source.model_ids_[model_slot].c_str());
                return 1;
            }
            graphs.push_back(std::make_unique<reference_inference_graph>());
            auto& model_activation = source_activation.models_[model_slot];
            model_activation.model_id_ = model->model_id_;
            model_activation.graph_ = graphs.back().get();
            model_activation.paths_.model_id_ = model->model_id_;
            model_activation.paths_.target_id_ = model->target_id_;
            model_activation.paths_.artifact_ref_ = model->artifact_ref_;
            model_activation.paths_.model_path_ =
                vqec_vision_ai_appl_svcmn_dev_model_path(model->model_id_);
            model_activation.paths_.backend_path_ = "/usr/lib/libQnnHtp.so";
            model_activation.paths_.system_path_ = "/usr/lib/libQnnSystem.so";
            model_activation.resolved_output_manifest_ref_ = model->output_manifest_ref_;
            model_activation.outputs_ = vqec_vision_ai_appl_svcmn_synthetic_outputs(*model);
            model_activation.tracker_contract_ = g_reference_tracker_contract;
            model_activation.binding_.width_ = source.profile_.width_;
            model_activation.binding_.height_ = source.profile_.height_;
            model_activation.binding_.fps_numerator_ = source.profile_.fps_numerator_;
            model_activation.binding_.fps_denominator_ = source.profile_.fps_denominator_;
            model_activation.binding_.memory_kind_ = source_memory_kind::dmabuf;
            model_activation.binding_.layout_ = source_memory_layout::linear_nv12;
            model_activation.binding_.sync_mode_ = source_sync_mode::implicit_ready;
            model_activation.binding_.color_profile_ = source_color_profile::bt709_limited;
            model_activation.binding_.chroma_site_ = source_chroma_site::mpeg2;
            model_activation.binding_.fw_memory_contract_ = "fw.dmabuf.v1";
            model_activation.binding_.backend_memory_contract_ = "qcom.dmabuf.v1";
            model_activation.binding_.preprocess_contract_ = model->preprocess_contract_;
            model_activation.cycle_id_ =
                static_cast<std::uint64_t>(source_slot) * 100U + model_slot + 1U;
            model_activation.job_timeout_ns_ = 1000000000ULL;
        }
    }

    // Optional feature activation. Only single_model features are wired by this harness;
    // temporal_join remains a documented activation-time gap.
    feature_activation_manager feature_manager;
    std::array<std::unique_ptr<feature_fanout>,
        deployment_limits::g_max_sources * deployment_limits::g_max_models_per_source>
        fanouts{};
    runtime_feature_activation feature_wiring;
    bool has_feature_wiring = false;
    if (!features.features_.empty()) {
        const auto configured = feature_manager.vqec_vision_ai_ftmgr_famgr_configure(
            features, catalog, deployment);
        if (configured.code_ != status_code::ok) {
            std::fprintf(stderr, "feature activation configure failed (%d): %s\n",
                static_cast<int>(configured.code_), configured.message_.c_str());
            return 1;
        }
        std::array<feature_activation_request,
            feature_activation_limits::g_max_associations> requests{};
        std::array<std::pair<std::uint16_t, std::uint16_t>,
            feature_activation_limits::g_max_associations> request_slots{};
        std::uint16_t request_count = 0;
        for (std::uint16_t source_slot = 0; source_slot < activation.source_count_; ++source_slot) {
            const auto& source = deployment.sources_[source_slot];
            for (const auto& feature : features.features_) {
                if (feature.input_mode_ != feature_input_mode::single_model ||
                    feature.model_dependencies_.size() != 1) {
                    continue;
                }
                const auto slot = vqec_vision_ai_appl_svcmn_model_slot(
                    source, feature.model_dependencies_[0].model_id_);
                if (slot == g_invalid_model_slot) {
                    continue;
                }
                auto& request = requests[request_count];
                request.source_id_ = source.source_id_;
                request.feature_id_ = feature.feature_id_;
                request.desired_enabled_ = true;
                request.entitlement_granted_ = true;
                request.resource_admitted_ = true;
                request.configuration_.schema_id_ = feature.configuration_schema_;
                request.configuration_.revision_ = 1;
                request_slots[request_count] = {source_slot, slot};
                ++request_count;
            }
        }
        if (request_count != 0) {
            feature_activation_snapshot snapshot;
            const auto reconciled = feature_manager.vqec_vision_ai_ftmgr_famgr_reconcile(
                requests, request_count, feature_registry, snapshot);
            if (reconciled.code_ != status_code::ok) {
                std::fprintf(stderr, "feature activation reconcile failed (%d): %s\n",
                    static_cast<int>(reconciled.code_), reconciled.message_.c_str());
                return 1;
            }
            // Explicit output entitlement for the wired associations. The fixture feature
            // emits one field, so the rule must list it or delivery is denied.
            output_policy policy;
            policy.revision_ = 1;
            policy.not_before_ns_ = 0;
            policy.expires_ns_ = 1000000000000000000ULL;
            for (std::uint16_t index = 0; index < request_count; ++index) {
                output_scope_rule rule;
                rule.source_id_ = requests[index].source_id_;
                rule.feature_id_ = requests[index].feature_id_;
                rule.attributes_.push_back("fixture.attribute");
                policy.rules_.push_back(std::move(rule));
            }
            const auto applied =
                output_policy_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0);
            if (applied.code_ != status_code::ok) {
                std::fprintf(stderr, "output policy apply failed (%d): %s\n",
                    static_cast<int>(applied.code_), applied.message_.c_str());
                return 1;
            }
            std::array<std::array<std::vector<feature_stage*>,
                deployment_limits::g_max_models_per_source>,
                deployment_limits::g_max_sources> stages_by_slot{};
            for (std::uint16_t index = 0; index < request_count; ++index) {
                auto* stage = feature_manager.vqec_vision_ai_ftmgr_famgr_get_stage(index);
                if (stage != nullptr) {
                    stages_by_slot[request_slots[index].first][request_slots[index].second]
                        .push_back(stage);
                }
            }
            for (std::uint16_t source_slot = 0; source_slot < activation.source_count_;
                 ++source_slot) {
                for (std::uint16_t model_slot = 0;
                     model_slot < deployment_limits::g_max_models_per_source; ++model_slot) {
                    auto& stages = stages_by_slot[source_slot][model_slot];
                    if (stages.empty() ||
                        stages.size() > feature_fanout_limits::g_max_feature_stages) {
                        continue;
                    }
                    std::array<feature_stage*,
                        feature_fanout_limits::g_max_feature_stages> stage_array{};
                    for (std::size_t index = 0; index < stages.size(); ++index) {
                        stage_array[index] = stages[index];
                    }
                    const std::size_t fanout_index =
                        static_cast<std::size_t>(source_slot) *
                            deployment_limits::g_max_models_per_source +
                        model_slot;
                    fanouts[fanout_index] = std::make_unique<feature_fanout>();
                    const auto fanout_configured =
                        fanouts[fanout_index]->vqec_vision_ai_appl_ftfan_configure(
                            stage_array, static_cast<std::uint16_t>(stages.size()));
                    if (fanout_configured.code_ != status_code::ok) {
                        std::fprintf(stderr, "feature fan-out configure failed (%d): %s\n",
                            static_cast<int>(fanout_configured.code_),
                            fanout_configured.message_.c_str());
                        return 1;
                    }
                    feature_wiring.sources_[source_slot].fanouts_[model_slot] =
                        fanouts[fanout_index].get();
                    has_feature_wiring = true;
                }
            }
        }
    }

    std::unique_ptr<runtime_composition_bundle> bundle;
    const auto created = vqec_vision_ai_appl_rcfac_create_bundle(
        deployment, catalog, activation, decoders, trackers, bundle,
        has_feature_wiring ? &feature_wiring : nullptr);
    if (created.code_ != status_code::ok) {
        std::fprintf(stderr, "runtime composition failed (%d): %s\n",
            static_cast<int>(created.code_), created.message_.c_str());
        return 1;
    }
    auto* executor = bundle->vqec_vision_ai_appl_rcfac_get_executor();
    if (executor == nullptr) {
        std::fprintf(stderr, "runtime composition returned no executor\n");
        return 1;
    }
    if (has_feature_wiring) {
        executor->vqec_vision_ai_appl_rtexe_bind_event_delivery(output_policy_gate, event_sink);
    }
    const auto activated =
        bundle->vqec_vision_ai_appl_rcfac_get_composition()->vqec_vision_ai_cntr_acomp_activate();
    if (activated.code_ != status_code::ok) {
        std::fprintf(stderr, "composition activation failed (%d): %s\n",
            static_cast<int>(activated.code_), activated.message_.c_str());
        return 1;
    }

    std::uint64_t now_ns = g_step_interval_ns;
    std::uint64_t steps = 0;
    std::uint32_t routed_source_mask = 0;
    status_code first_error_code = status_code::ok;
    while (!g_stop_requested && (args.max_steps == 0 || steps < args.max_steps)) {
        runtime_executor_report report;
        const auto stepped = executor->vqec_vision_ai_appl_rtexe_step(now_ns, report);
        if (report.first_error_code_ != status_code::ok && first_error_code == status_code::ok) {
            first_error_code = report.first_error_code_;
        }
        if (stepped.code_ == status_code::ok) {
            std::array<observation_batch, deployment_limits::g_max_models_per_source> tracked;
            std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages> events;
            runtime_executor_report taken;
            if (executor->vqec_vision_ai_appl_rtexe_take_result(
                    tracked, events, taken).code_ == status_code::ok) {
                routed_source_mask |= 1U << taken.source_index_;
                feature_dispatch_report dispatch_report;
                if (taken.has_feature_fanout_ && has_feature_wiring) {
                    const auto dispatched =
                        executor->vqec_vision_ai_appl_rtexe_dispatch_events(
                            events, taken.source_index_, taken.model_slot_, now_ns,
                            dispatch_report);
                    if (dispatched.code_ != status_code::ok) {
                        std::fprintf(stderr, "event delivery rejected: %s\n",
                            dispatched.message_.c_str());
                    }
                }
                std::printf("routed source=%u model=%u tracked=%zu delivered=%u\n",
                    static_cast<unsigned>(taken.source_index_),
                    static_cast<unsigned>(taken.model_slot_),
                    tracked[taken.model_slot_].observations_.size(),
                    static_cast<unsigned>(dispatch_report.delivered_));
            }
        } else if (stepped.code_ != status_code::pending) {
            if (first_error_code == status_code::ok) {
                first_error_code = stepped.code_;
            }
            std::fprintf(stderr, "executor step failed (%d): %s\n",
                static_cast<int>(stepped.code_), stepped.message_.c_str());
            break;
        }
        now_ns += g_step_interval_ns;
        ++steps;
    }

    std::printf("stopping after %llu steps\n", static_cast<unsigned long long>(steps));
    const auto stop = executor->vqec_vision_ai_appl_rtexe_request_stop(now_ns);
    (void)stop;
    bool stopped = false;
    for (unsigned drain = 0; drain < 1000 && !stopped; ++drain) {
        // Drain must consume/discard a retained result, otherwise the executor refuses to
        // advance and a result arriving at stop would prevent reaching stopped.
        if (executor->vqec_vision_ai_appl_rtexe_has_pending()) {
            executor->vqec_vision_ai_appl_rtexe_discard_pending();
        }
        now_ns += g_step_interval_ns;
        runtime_executor_report drain_report;
        const auto progressed = executor->vqec_vision_ai_appl_rtexe_step(now_ns, drain_report);
        if (drain_report.first_error_code_ != status_code::ok && first_error_code == status_code::ok) {
            first_error_code = drain_report.first_error_code_;
        }
        if (progressed.code_ != status_code::ok && progressed.code_ != status_code::pending) {
            if (first_error_code == status_code::ok) {
                first_error_code = progressed.code_;
            }
            break;
        }
        stopped = executor->vqec_vision_ai_appl_rtexe_get_snapshot().state_ ==
            application_composition_state::stopped;
    }
    std::uint32_t routed_sources = 0;
    for (std::uint32_t mask = routed_source_mask; mask != 0; mask &= mask - 1U) {
        ++routed_sources;
    }
    std::printf("service stopped=%s routed_sources=%u first_error=%d\n",
        stopped ? "true" : "false", routed_sources, static_cast<int>(first_error_code));
    // Owners (feature manager, fan-outs, registries, reference platform) outlive the
    // bundle; the bundle's composition must be stopped before they are destroyed.
    if (!stopped || first_error_code != status_code::ok) {
        return 1;
    }
    return routed_sources >= args.require_sources ? 0 : 1;
}
