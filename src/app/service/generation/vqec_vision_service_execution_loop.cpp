#include "vqec_vision_service_execution_loop.hpp"

#include <chrono>
#include <cstdio>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vqec_vision_event_delivery_seam.hpp"
#include "vqec_vision_feature_fanout.hpp"
#include "vqec_vision_metadata_runtime.hpp"
#include "vqec_vision_multi_model_session.hpp"
#include "vqec_vision_overlay_preparation.hpp"
#include "vqec_vision_production_platform.hpp"
#include "vqec_vision_recognition_session.hpp"
#include "vqec_vision_runtime_composition_factory.hpp"
#include "vqec_vision_runtime_executor.hpp"
#include "vqec_vision_service_enrollment_runtime.hpp"
#include "vqec_vision_service_fixture.hpp"
#include "vqec_vision_service_options.hpp"
#include "vqec_vision_usecase_control_manager.hpp"

namespace vqec::vision::ai {
namespace {

using model_observation_cache =
    std::array<observation_batch, deployment_limits::g_max_models_per_source>;

constexpr std::size_t g_max_active_track_labels = 256;
constexpr std::uint64_t g_routed_log_interval_ns = 1000000000ULL;
constexpr std::uint64_t g_diagnostic_log_interval_ns = 2000000000ULL;
constexpr std::uint64_t g_nanoseconds_per_millisecond = 1000000ULL;
constexpr std::uint32_t g_fallback_source_fps = 30U;
constexpr char g_preview_feature_id[] = "preview";
constexpr char g_overlay_attribute_id[] = "overlay";

std::uint64_t vqec_vision_ai_appl_svxlp_monotonic_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

const model_catalog_entry* vqec_vision_ai_appl_svxlp_find_model(
    const model_catalog& _catalog, const std::string& _model_id) noexcept {
    for (const auto& model : _catalog.models_) {
        if (model.model_id_ == _model_id) {
            return &model;
        }
    }
    return nullptr;
}

void vqec_vision_ai_appl_svxlp_merge_observations(
    const model_observation_cache& _models, std::uint16_t _model_count,
    observation_batch& _merged) {
    observation_batch merged;
    for (std::uint16_t slot = 0;
         slot < _model_count && slot < deployment_limits::g_max_models_per_source;
         ++slot) {
        const auto& batch = _models[slot];
        if (batch.frame_.source_epoch_ == 0) {
            continue;
        }
        if (merged.frame_.source_epoch_ == 0) {
            merged.frame_ = batch.frame_;
            merged.geometry_ = batch.geometry_;
        } else if (merged.frame_.source_epoch_ != batch.frame_.source_epoch_) {
            continue;
        }
        for (const auto& item : batch.observations_) {
            if (merged.observations_.size() >= observation_limits::g_max_observations) {
                break;
            }
            auto merged_item = item;
            merged_item.frame_ = merged.frame_;
            if (merged_item.track_id_ != 0) {
                merged_item.track_id_ =
                    (static_cast<std::uint64_t>(slot + 1) << 32) |
                    (merged_item.track_id_ & 0xFFFFFFFFULL);
            }
            merged.observations_.push_back(std::move(merged_item));
        }
    }
    _merged = std::move(merged);
}

bool vqec_vision_ai_appl_svxlp_has_required_context(
    const service_execution_context& _context) noexcept {
    return _context.arguments_ != nullptr && _context.deployment_ != nullptr &&
        _context.catalog_ != nullptr && _context.tracker_contract_ != nullptr &&
        _context.bundle_ != nullptr && _context.executor_ != nullptr &&
        _context.production_ != nullptr && _context.output_policy_gate_ != nullptr &&
        _context.recognition_ != nullptr && _context.enrollment_ != nullptr &&
        _context.cascade_owners_ != nullptr &&
        static_cast<bool>(_context.stop_requested_);
}

}  // namespace

bool vqec_vision_ai_appl_svxlp_select_preview(
    std::uint32_t _source_fps, std::uint32_t _output_fps,
    std::uint32_t& _phase, bool& _initialized) noexcept {
    if (_source_fps == 0 || _output_fps == 0 || _output_fps >= _source_fps) {
        return true;
    }
    if (!_initialized) {
        _phase = _source_fps;
        _initialized = true;
    }
    _phase += _output_fps;
    if (_phase < _source_fps) {
        return false;
    }
    _phase -= _source_fps;
    return true;
}

bool vqec_vision_ai_appl_svxlp_is_source_replacement(
    status_code _step_code, bool _composition_stopped,
    status_code _first_error_code) noexcept {
    return _step_code == status_code::invalid_state &&
        _composition_stopped &&
        _first_error_code == status_code::source_lost;
}

status vqec_vision_ai_appl_svxlp_run(
    const service_execution_context& _context,
    service_execution_result& _result) {
    _result = {};
    if (!vqec_vision_ai_appl_svxlp_has_required_context(_context)) {
        return {status_code::invalid_argument,
            "service execution loop context is incomplete"};
    }
    const auto& arguments = *_context.arguments_;
    const auto& deployment = *_context.deployment_;
    const auto& catalog = *_context.catalog_;
    const auto& tracker_contract = *_context.tracker_contract_;
    auto& bundle = *_context.bundle_;
    auto& executor = *_context.executor_;
    auto& production = *_context.production_;
    auto& output_policy_gate = *_context.output_policy_gate_;
    auto& recognition = *_context.recognition_;
    auto& enrollment = *_context.enrollment_;
    auto& cascade_owners = *_context.cascade_owners_;

    std::array<model_observation_cache, deployment_limits::g_max_sources>
        latest_model_observations;
    std::array<observation_batch, deployment_limits::g_max_sources>
        latest_overlay_observations;
    std::array<bool, deployment_limits::g_max_sources> cascade_error_reported{};
    std::array<std::unordered_map<std::uint64_t, std::string>,
        deployment_limits::g_max_sources> active_track_labels;
    std::array<std::uint32_t, deployment_limits::g_max_sources> preview_phase{};
    std::array<bool, deployment_limits::g_max_sources> preview_phase_initialized{};
    std::uint64_t last_routed_log_ns = 0;
    std::uint64_t last_pts_drop_log_ns = 0;
    std::uint64_t last_prepare_failure_log_ns = 0;
    std::uint64_t now_ns = vqec_vision_ai_appl_svxlp_monotonic_ns();
    std::uint64_t steps = 0;
    std::uint32_t routed_source_mask = 0;
    status_code first_error_code = status_code::ok;
    bool replacement_requested = false;
    bool generation_published = _context.control_manager_ == nullptr;

    while (!_context.stop_requested_() &&
           (arguments.max_steps == 0 || steps < arguments.max_steps)) {
        if (_context.poll_control_) {
            _context.poll_control_();
        }
        if (_context.reconcile_requested_ && _context.reconcile_requested_()) {
            if (!executor.vqec_vision_ai_appl_rtexe_is_activation_quiescent()) {
                // The worker exclusively owns its session call. Progress below first
                // consumes queued/in-flight work and its result; activation mutates model
                // masks and feature wiring only at the next quiescent control point.
            } else if (_context.apply_runtime_control_) {
                const auto applied = _context.apply_runtime_control_();
                if (applied.code_ == status_code::ok) {
                    continue;
                }
                if (applied.code_ == status_code::pending) {
                    // The executor step below progresses the accepted per-slot lifecycle.
                    // The complete snapshot remains pending until all sessions report the
                    // requested active mask.
                } else if (applied.code_ != status_code::unsupported) {
                    return applied;
                } else {
                    replacement_requested = true;
                    break;
                }
            } else {
                replacement_requested = true;
                break;
            }
        }
        if (_context.control_manager_ != nullptr && generation_published &&
            _context.control_manager_->vqec_vision_ai_ftmgr_ucmgr_has_pending()) {
            replacement_requested = true;
            break;
        }
        const auto clock_now = vqec_vision_ai_appl_svxlp_monotonic_ns();
        now_ns = clock_now > now_ns ? clock_now :
            now_ns + arguments.runtime_step_interval_ns;

        if (_context.production_platform_enabled_) {
            std::array<bool, deployment_limits::g_max_sources> cascade_source_ready{};
            if (executor.vqec_vision_ai_appl_rtexe_is_activation_quiescent()) {
                for (std::uint16_t source_slot = 0;
                     source_slot < deployment.sources_.size(); ++source_slot) {
                    const auto* session =
                        bundle.vqec_vision_ai_appl_rcfac_get_session(source_slot);
                    if (session == nullptr) {
                        continue;
                    }
                    const auto state = session->vqec_vision_ai_appl_mmses_get_snapshot().
                        session_state_;
                    cascade_source_ready[source_slot] =
                        state == multi_model_session_state::configuring ||
                        state == multi_model_session_state::loading ||
                        state == multi_model_session_state::binding ||
                        state == multi_model_session_state::starting ||
                        state == multi_model_session_state::running;
                }
            }
            const auto cascades_started =
                vqec_vision_ai_appl_svcsc_start_ready_graphs(
                    cascade_owners, cascade_source_ready);
            if (cascades_started.code_ != status_code::ok) {
                first_error_code = cascades_started.code_;
                std::fprintf(stderr, "cascade graph startup failed (%d): %s\n",
                    static_cast<int>(cascades_started.code_),
                    cascades_started.message_.c_str());
                break;
            }
            const auto cascades_synced =
                vqec_vision_ai_appl_svcsc_sync_executor(
                    cascade_owners, executor);
            if (cascades_synced.code_ != status_code::ok) {
                first_error_code = cascades_synced.code_;
                std::fprintf(stderr, "cascade execution gate failed (%d): %s\n",
                    static_cast<int>(cascades_synced.code_),
                    cascades_synced.message_.c_str());
                break;
            }
        }

        runtime_executor_report report;
        const auto stepped = executor.vqec_vision_ai_appl_rtexe_step(now_ns, report);
        const auto composition_snapshot =
            executor.vqec_vision_ai_appl_rtexe_get_snapshot();
        if (vqec_vision_ai_appl_svxlp_is_source_replacement(
                stepped.code_, composition_snapshot.state_ ==
                    application_composition_state::stopped,
                composition_snapshot.first_error_code_)) {
            std::fprintf(stderr,
                "source lost after complete drain; replacing runtime generation\n");
            replacement_requested = true;
            break;
        }
        if (report.first_error_code_ != status_code::ok &&
            first_error_code == status_code::ok) {
            first_error_code = report.first_error_code_;
        }
        if (!generation_published && first_error_code == status_code::ok &&
            executor.vqec_vision_ai_appl_rtexe_is_activation_quiescent()) {
            bool all_sources_running = true;
            for (std::uint16_t source_slot = 0;
                 source_slot < deployment.sources_.size(); ++source_slot) {
                const auto* session = bundle.vqec_vision_ai_appl_rcfac_get_session(source_slot);
                if (session == nullptr ||
                    session->vqec_vision_ai_appl_srcsn_get_health().phase_ !=
                        source_session_phase::running) {
                    all_sources_running = false;
                    break;
                }
            }
            if (all_sources_running) {
                const auto published = _context.pending_control_revision_ == 0
                    ? (_context.runtime_generation_ == 1
                        ? _context.control_manager_->
                              vqec_vision_ai_ftmgr_ucmgr_publish_initial(
                                  _context.runtime_generation_)
                        : status{})
                    : _context.control_manager_->
                          vqec_vision_ai_ftmgr_ucmgr_publish_pending(
                              _context.pending_control_revision_,
                              _context.runtime_generation_);
                if (published.code_ != status_code::ok) {
                    first_error_code = published.code_;
                    break;
                }
                generation_published = true;
            }
        }

        if (stepped.code_ == status_code::ok) {
            std::array<observation_batch,
                deployment_limits::g_max_models_per_source> tracked;
            std::array<feature_event_batch,
                feature_fanout_limits::g_max_feature_stages> events;
            std::vector<embedding_result> embeddings;
            if (_context.recognition_enabled_) {
                embeddings.reserve(observation_limits::g_max_observations);
            }
            runtime_executor_report taken;
            const auto taken_status = _context.recognition_enabled_
                ? executor.vqec_vision_ai_appl_rtexe_take_result_with_embeddings(
                    tracked, events, embeddings, taken)
                : executor.vqec_vision_ai_appl_rtexe_take_result(
                    tracked, events, taken);
            if (taken_status.code_ == status_code::ok) {
                routed_source_mask |= 1U << taken.source_index_;
                feature_dispatch_report dispatch_report;
                if (taken.has_feature_fanout_) {
                    const auto dispatched =
                        executor.vqec_vision_ai_appl_rtexe_dispatch_events(
                            events, taken.source_index_, taken.model_slot_,
                            taken.captured_policy_revision_,
                            taken.features_.processed_mask_, now_ns, dispatch_report);
                    if (dispatched.code_ != status_code::ok) {
                        std::fprintf(stderr, "event delivery rejected: %s\n",
                            dispatched.message_.c_str());
                        if (_context.metadata_required_) {
                            first_error_code = dispatched.code_;
                            break;
                        }
                    }
                }
                if (_context.recognition_enabled_ && !embeddings.empty() &&
                    taken.source_index_ < deployment_limits::g_max_sources) {
                    if (_context.enrollment_port_ != nullptr) {
                        face_enrollment_status enrollment_status;
                        const auto root_slot =
                            cascade_owners[taken.source_index_].root_model_slot_;
                        const auto face_count = root_slot < tracked.size()
                            ? tracked[root_slot].observations_.size() : 0;
                        const auto accepted = _context.enrollment_port_->
                            vqec_vision_ai_ports_fenrl_accept_batch(
                                deployment.sources_[taken.source_index_].source_id_,
                                embeddings, face_count, enrollment_status);
                        if (accepted.code_ != status_code::ok &&
                            accepted.code_ != status_code::pending &&
                            accepted.code_ != status_code::invalid_argument &&
                            accepted.code_ != status_code::invalid_state) {
                            std::fprintf(stderr, "FR enrollment failed (%d): %s\n",
                                static_cast<int>(accepted.code_),
                                accepted.message_.c_str());
                        }
                    }
                    std::vector<recognition_match_result> recognition_results;
                    recognition_results.reserve(embeddings.size());
                    const auto recognized =
                        recognition.vqec_vision_ai_embed_rcses_recognize_batch(
                            embeddings, recognition_results);
                    if (recognized.code_ != status_code::ok) {
                        std::fprintf(stderr, "FR recognition failed (%d): %s\n",
                            static_cast<int>(recognized.code_),
                            recognized.message_.c_str());
                    } else {
                        const auto& owner = cascade_owners[taken.source_index_];
                        if (owner.root_model_slot_ <
                            deployment_limits::g_max_models_per_source) {
                            const output_authorization identity_scope{
                                taken.captured_policy_revision_,
                                deployment.sources_[taken.source_index_].source_id_,
                                arguments.fr_feature_id,
                                {arguments.fr_identity_attribute}};
                            const auto authorized = output_policy_gate
                                .vqec_vision_ai_core_otgat_authorize(
                                    identity_scope, now_ns);
                            if (authorized.code_ != status_code::ok) {
                                std::fprintf(stderr,
                                    "FR identity output denied (%d): %s "
                                    "(captured_policy=%llu active_policy=%llu)\n",
                                    static_cast<int>(authorized.code_),
                                    authorized.message_.c_str(),
                                    static_cast<unsigned long long>(
                                        taken.captured_policy_revision_),
                                    static_cast<unsigned long long>(output_policy_gate
                                        .vqec_vision_ai_core_otgat_get_revision()));
                            } else {
                                const auto labelled = recognition
                                    .vqec_vision_ai_embed_rcses_apply_labels(
                                        recognition_results,
                                        tracked[owner.root_model_slot_]);
                                if (labelled.code_ != status_code::ok) {
                                    std::fprintf(stderr,
                                        "FR label correlation failed (%d): %s\n",
                                        static_cast<int>(labelled.code_),
                                        labelled.message_.c_str());
                                } else {
                                    for (const auto& match : recognition_results) {
                                        if (match.decision_ == recognition_decision::known &&
                                            !match.subject_ref_.empty()) {
                                            active_track_labels[taken.source_index_]
                                                [match.track_id_] = match.subject_ref_;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (_context.metadata_ != nullptr &&
                    taken.source_index_ < deployment.sources_.size() &&
                    taken.model_slot_ < tracked.size()) {
                    const auto& metadata_source =
                        deployment.sources_[taken.source_index_];
                    const auto* profile =
                        _context.metadata_->vqec_vision_ai_appl_mdrun_find_source(
                            metadata_source.source_id_);
                    const auto* model =
                        taken.model_slot_ < metadata_source.model_ids_.size()
                        ? vqec_vision_ai_appl_svxlp_find_model(
                              catalog,
                              metadata_source.model_ids_[taken.model_slot_])
                        : nullptr;
                    if (profile != nullptr && model != nullptr &&
                        profile->trajectory_model_id_ == model->model_id_) {
                        const output_authorization trajectory_scope{
                            taken.captured_policy_revision_,
                            metadata_source.source_id_,
                            profile->trajectory_authorization_feature_id_,
                            {profile->trajectory_authorization_attribute_id_}};
                        const bool trajectory_authorized = output_policy_gate
                            .vqec_vision_ai_core_otgat_authorize(
                                trajectory_scope, now_ns).code_ == status_code::ok;
                        const auto submitted = _context.metadata_->
                            vqec_vision_ai_appl_mdrun_submit_observations(
                                metadata_source.source_id_,
                                model->model_id_ + "." + model->model_version_,
                                tracker_contract, tracked[taken.model_slot_],
                                trajectory_authorized);
                        if (submitted.code_ != status_code::ok &&
                            submitted.code_ != status_code::unauthorized &&
                            submitted.code_ != status_code::unsupported) {
                            std::fprintf(stderr,
                                "metadata trajectory failed (%d): %s\n",
                                static_cast<int>(submitted.code_),
                                submitted.message_.c_str());
                            if (_context.metadata_required_) {
                                first_error_code = submitted.code_;
                                break;
                            }
                        }
                        const auto maintained = _context.metadata_->
                            vqec_vision_ai_appl_mdrun_maintain(
                                tracked[taken.model_slot_].frame_.source_pts_ns_);
                        if (maintained.code_ != status_code::ok) {
                            std::fprintf(stderr,
                                "metadata maintenance failed (%d): %s\n",
                                static_cast<int>(maintained.code_),
                                maintained.message_.c_str());
                            if (_context.metadata_required_) {
                                first_error_code = maintained.code_;
                                break;
                            }
                        }
                    }
                }

                const auto tracked_count =
                    tracked[taken.model_slot_].observations_.size();
                if (_context.production_platform_enabled_ &&
                    taken.source_index_ < deployment_limits::g_max_sources &&
                    taken.model_slot_ < deployment_limits::g_max_models_per_source) {
                    const auto& cascade_owner =
                        cascade_owners[taken.source_index_];
                    if (taken.model_slot_ == cascade_owner.root_model_slot_) {
                        for (auto& observation :
                             tracked[taken.model_slot_].observations_) {
                            if (observation.box_.label_.empty()) {
                                const auto label =
                                    active_track_labels[taken.source_index_].find(
                                        observation.track_id_);
                                if (label !=
                                    active_track_labels[taken.source_index_].end()) {
                                    observation.box_.label_ = label->second;
                                }
                            }
                        }
                        if (active_track_labels[taken.source_index_].size() >
                            g_max_active_track_labels) {
                            active_track_labels[taken.source_index_].clear();
                        }
                    }
                    latest_model_observations[taken.source_index_][taken.model_slot_] =
                        std::move(tracked[taken.model_slot_]);
                    vqec_vision_ai_appl_svxlp_merge_observations(
                        latest_model_observations[taken.source_index_],
                        static_cast<std::uint16_t>(deployment
                            .sources_[taken.source_index_].model_ids_.size()),
                        latest_overlay_observations[taken.source_index_]);
                }
                if (now_ns - last_routed_log_ns >= g_routed_log_interval_ns) {
                    std::printf("routed source=%u model=%u tracked=%zu accepted=%u "
                        "cascade_accepted=%u embedded=%u cascade_failed=%u\n",
                        static_cast<unsigned>(taken.source_index_),
                        static_cast<unsigned>(taken.model_slot_), tracked_count,
                        static_cast<unsigned>(dispatch_report.accepted_),
                        static_cast<unsigned>(taken.cascade_.accepted_),
                        static_cast<unsigned>(taken.cascade_.embedded_),
                        static_cast<unsigned>(taken.cascade_.failed_));
                    last_routed_log_ns = now_ns;
                }
                if (taken.cascade_.failed_ != 0 &&
                    taken.source_index_ < cascade_owners.size() &&
                    !cascade_error_reported[taken.source_index_] &&
                    cascade_owners[taken.source_index_].coordinator_ != nullptr) {
                    const auto& cascade_error = cascade_owners[taken.source_index_]
                        .coordinator_->vqec_vision_ai_appl_cscrd_get_last_task_error();
                    std::fprintf(stderr, "cascade task failed (%d): %s\n",
                        static_cast<int>(cascade_error.code_),
                        cascade_error.message_.c_str());
                    cascade_error_reported[taken.source_index_] = true;
                }
            }
        } else if (stepped.code_ != status_code::pending) {
            if (first_error_code == status_code::ok) {
                first_error_code = stepped.code_;
            }
            std::fprintf(stderr, "executor step failed (%d): %s\n",
                static_cast<int>(stepped.code_), stepped.message_.c_str());
            break;
        }

        if (_context.recognition_enabled_) {
            const auto enrolled =
                enrollment.vqec_vision_ai_appl_svenr_poll(now_ns);
            if (enrolled.code_ != status_code::ok &&
                enrolled.code_ != status_code::pending) {
                std::fprintf(stderr, "enrollment runtime poll failed (%d): %s\n",
                    static_cast<int>(enrolled.code_), enrolled.message_.c_str());
            }
        }

        if (_context.production_platform_enabled_ &&
            (!arguments.use_session_workers ||
             executor.vqec_vision_ai_appl_rtexe_is_activation_quiescent())) {
            for (std::uint16_t source_slot = 0;
                 source_slot < deployment.sources_.size(); ++source_slot) {
                auto* session = bundle.vqec_vision_ai_appl_rcfac_get_session(source_slot);
                raw_frame preview_frame;
                if (session == nullptr ||
                    session->vqec_vision_ai_appl_mmses_take_preview_frame(preview_frame)
                            .code_ != status_code::ok ||
                    !preview_frame.owner_) {
                    continue;
                }
                const std::uint32_t source_fps =
                    deployment.sources_[source_slot].profile_.fps_numerator_ > 0
                    ? deployment.sources_[source_slot].profile_.fps_numerator_
                    : g_fallback_source_fps;
                if (!vqec_vision_ai_appl_svxlp_select_preview(
                        source_fps,
                        static_cast<std::uint32_t>(arguments.output_fps),
                        preview_phase[source_slot],
                        preview_phase_initialized[source_slot])) {
                    continue;
                }
                auto& overlay = latest_overlay_observations[source_slot];
                if (overlay.frame_.source_epoch_ != 0 &&
                    overlay.frame_.source_epoch_ !=
                        preview_frame.descriptor_.session_epoch_) {
                    latest_model_observations[source_slot] = {};
                    overlay = {};
                }
                if (overlay.frame_.source_pts_ns_ != UINT64_MAX &&
                    preview_frame.descriptor_.pts_ns_ != UINT64_MAX &&
                    preview_frame.descriptor_.pts_ns_ != 0 &&
                    preview_frame.descriptor_.pts_ns_ >
                        overlay.frame_.source_pts_ns_ &&
                    preview_frame.descriptor_.pts_ns_ -
                        overlay.frame_.source_pts_ns_ >
                        service_harness::g_overlay_max_age_ns) {
                    if (now_ns - last_pts_drop_log_ns >
                        g_diagnostic_log_interval_ns) {
                        std::fprintf(stderr,
                            "overlay dropped due to PTS delta > %llums "
                            "(preview_pts=%llu, overlay_pts=%llu)\n",
                            static_cast<unsigned long long>(
                                service_harness::g_overlay_max_age_ns /
                                g_nanoseconds_per_millisecond),
                            static_cast<unsigned long long>(
                                preview_frame.descriptor_.pts_ns_),
                            static_cast<unsigned long long>(
                                overlay.frame_.source_pts_ns_));
                        last_pts_drop_log_ns = now_ns;
                    }
                    overlay = {};
                }
                prepared_overlay prepared;
                observation_batch render_observations = overlay;
                if (render_observations.frame_.source_epoch_ == 0) {
                    render_observations.frame_.camera_id_ =
                        deployment.sources_[source_slot].camera_id_;
                    render_observations.frame_.channel_id_ =
                        deployment.sources_[source_slot].channel_id_;
                    render_observations.frame_.source_epoch_ =
                        preview_frame.descriptor_.session_epoch_;
                    render_observations.frame_.frame_id_ =
                        preview_frame.descriptor_.buffer_id_;
                    render_observations.frame_.source_pts_ns_ =
                        preview_frame.descriptor_.pts_ns_;
                    render_observations.geometry_.width_ =
                        preview_frame.descriptor_.width_;
                    render_observations.geometry_.height_ =
                        preview_frame.descriptor_.height_;
                }
                overlay_preparation_context preparation;
                preparation.source_id_ = deployment.sources_[source_slot].source_id_;
                preparation.feature_id_ = g_preview_feature_id;
                preparation.policy_revision_ = output_policy_gate
                    .vqec_vision_ai_core_otgat_get_revision();
                preparation.prepared_monotonic_ns_ = now_ns;
                preparation.max_age_ns_ = service_harness::g_overlay_max_age_ns;
                preparation.attributes_ = {g_overlay_attribute_id};
                const auto prepared_status =
                    vqec_vision_ai_outpt_ovrpr_prepare_authorized(
                        render_observations, preparation,
                        output_policy_gate, prepared);
                if (prepared_status.code_ != status_code::ok) {
                    if (now_ns - last_prepare_failure_log_ns >
                        g_diagnostic_log_interval_ns) {
                        std::fprintf(stderr, "overlay prepare failed (%d): %s\n",
                            static_cast<int>(prepared_status.code_),
                            prepared_status.message_.c_str());
                        last_prepare_failure_log_ns = now_ns;
                    }
                    continue;
                }
                const auto rendered = production.vqec_vision_ai_appl_pdplt_render(
                    source_slot, preview_frame, prepared);
                if (rendered.code_ != status_code::ok &&
                    rendered.code_ != status_code::pending) {
                    std::fprintf(stderr, "render failed (%d): %s\n",
                        static_cast<int>(rendered.code_),
                        rendered.message_.c_str());
                }
            }
        }

        ++steps;
        if (report.has_cascade_) {
            continue;
        }
        const auto step_end_ns = vqec_vision_ai_appl_svxlp_monotonic_ns();
        const auto step_cost_ns = step_end_ns > clock_now
            ? step_end_ns - clock_now : 0U;
        if (step_cost_ns < arguments.runtime_step_interval_ns) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(
                arguments.runtime_step_interval_ns - step_cost_ns));
        }
    }

    _result.steady_now_ns_ = now_ns;
    _result.steps_ = steps;
    _result.routed_source_mask_ = routed_source_mask;
    _result.first_error_code_ = first_error_code;
    _result.generation_published_ = generation_published;
    _result.replacement_requested_ = replacement_requested;
    return {};
}

}  // namespace vqec::vision::ai
