#include "vqec_vision_metadata_service.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <variant>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

struct metadata_query_work {
    spatiotemporal_query query_;
    spatiotemporal_query_page page_;
    status result_{status_code::pending, "metadata query is pending"};
    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_complete_{false};
};

using metadata_work_payload = std::variant<trajectory_chunk, track_association_revision,
    event_episode_revision, aggregate_contribution_revision,
    std::shared_ptr<metadata_query_work>>;

struct metadata_work_item {
    metadata_work_payload payload_;
};

std::string vqec_vision_ai_appl_mdsvc_make_track_key(
    const spatiotemporal_track_key& _track) {
    constexpr char g_track_key_separator = '\x1f';
    return _track.device_id_ + g_track_key_separator + _track.source_id_ +
        g_track_key_separator + _track.boot_id_ + g_track_key_separator +
        std::to_string(_track.source_epoch_) + g_track_key_separator +
        std::to_string(_track.local_track_id_);
}

bool vqec_vision_ai_appl_mdsvc_is_service_config_valid(
    const metadata_service_config& _config) {
    if (_config.queue_capacity_ == 0U ||
        _config.queue_capacity_ > metadata_service_limits::g_maximum_queue_capacity ||
        _config.maximum_live_tracks_ == 0U ||
        _config.maximum_live_tracks_ > metadata_service_limits::g_maximum_live_tracks ||
        _config.maximum_live_deltas_ == 0U ||
        _config.maximum_live_deltas_ > metadata_service_limits::g_maximum_live_deltas ||
        _config.maximum_batch_records_ == 0U ||
        _config.maximum_batch_records_ > metadata_service_limits::g_maximum_batch_records ||
        _config.outbox_sinks_.size() > g_spatiotemporal_max_outbox_sinks) {
        return false;
    }
    std::vector<std::string> sinks = _config.outbox_sinks_;
    std::sort(sinks.begin(), sinks.end());
    return std::adjacent_find(sinks.begin(), sinks.end()) == sinks.end() &&
        std::all_of(sinks.begin(), sinks.end(), [](const auto& _sink) {
            return vqec_vision_ai_cntr_ident_is_valid(
                _sink, g_spatiotemporal_max_identifier_bytes);
        });
}

bool vqec_vision_ai_appl_mdsvc_is_projection_work(
    const metadata_work_item& _item) {
    return std::holds_alternative<event_episode_revision>(_item.payload_) ||
        std::holds_alternative<aggregate_contribution_revision>(_item.payload_);
}

bool vqec_vision_ai_appl_mdsvc_is_authorized(
    const trajectory_chunk& _chunk, std::uint32_t _allowed_access_domain_mask) {
    return _allowed_access_domain_mask != 0U &&
        (_chunk.required_access_domain_mask_ & _allowed_access_domain_mask) ==
            _chunk.required_access_domain_mask_;
}

}  // namespace

class metadata_service::implementation final {
public:
    explicit implementation(metadata_service_config _config)
        : config_(std::move(_config)), store_(config_.store_) {}

    ~implementation() noexcept {
        (void)vqec_vision_ai_appl_mdsvc_stop(true);
    }

    status vqec_vision_ai_appl_mdsvc_start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_started_ || worker_.joinable()) {
            return {status_code::invalid_state, "metadata service is already started"};
        }
        if (!vqec_vision_ai_appl_mdsvc_is_service_config_valid(config_)) {
            return {status_code::invalid_argument, "metadata service configuration is invalid"};
        }
        auto result = store_.vqec_vision_ai_stor_stsql_open();
        if (result.code_ != status_code::ok) {
            return result;
        }
        is_started_ = true;
        is_stop_requested_ = false;
        should_drain_ = true;
        worker_result_ = {};
        try {
            worker_ = std::thread([this]() { vqec_vision_ai_appl_mdsvc_run(); });
        } catch (...) {
            is_started_ = false;
            (void)store_.vqec_vision_ai_stor_stsql_close();
            return {status_code::resource_exhausted,
                "metadata service worker creation failed"};
        }
        return {};
    }

    status vqec_vision_ai_appl_mdsvc_stop(bool _drain) noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!is_started_ && !worker_.joinable()) {
                return {};
            }
            is_stop_requested_ = true;
            should_drain_ = _drain;
            if (!_drain) {
                vqec_vision_ai_appl_mdsvc_reject_pending_queries_locked();
                stats_.rejected_records_ += queue_.size();
                queue_.clear();
                stats_.queue_depth_ = 0U;
            }
        }
        condition_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        is_started_ = false;
        return worker_result_;
    }

    status vqec_vision_ai_appl_mdsvc_submit_trajectory(const trajectory_chunk& _chunk) {
        auto result = vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(_chunk);
        if (result.code_ != status_code::ok) {
            return result;
        }
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            result = vqec_vision_ai_appl_mdsvc_enqueue_locked(metadata_work_item{_chunk});
            if (result.code_ == status_code::ok) {
                vqec_vision_ai_appl_mdsvc_upsert_live_locked(_chunk);
            }
        } catch (const std::bad_alloc&) {
            result = {status_code::resource_exhausted,
                "metadata trajectory queue allocation failed"};
        }
        if (result.code_ == status_code::ok) {
            condition_.notify_one();
        }
        return result;
    }

    status vqec_vision_ai_appl_mdsvc_submit_association(
        const track_association_revision& _association) {
        const auto valid =
            vqec_vision_ai_cntr_stmet_validate_association_revision(_association);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        return vqec_vision_ai_appl_mdsvc_enqueue(metadata_work_item{_association});
    }

    status vqec_vision_ai_appl_mdsvc_submit_episode(
        const event_episode_revision& _episode) {
        const auto valid = vqec_vision_ai_cntr_stmet_validate_episode_revision(_episode);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        return vqec_vision_ai_appl_mdsvc_enqueue(metadata_work_item{_episode});
    }

    status vqec_vision_ai_appl_mdsvc_submit_aggregate_contribution(
        const aggregate_contribution_revision& _contribution) {
        const auto valid =
            vqec_vision_ai_cntr_stmet_validate_aggregate_contribution(_contribution);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        return vqec_vision_ai_appl_mdsvc_enqueue(metadata_work_item{_contribution});
    }

    status vqec_vision_ai_appl_mdsvc_remove_live_track(
        const spatiotemporal_track_key& _track) {
        const auto valid = vqec_vision_ai_cntr_stmet_validate_track_key(_track);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_started_ || is_stop_requested_) {
            return {status_code::invalid_state, "metadata service is not accepting live data"};
        }
        const auto key = vqec_vision_ai_appl_mdsvc_make_track_key(_track);
        const auto current = live_tracks_.find(key);
        if (current == live_tracks_.end()) {
            return {};
        }
        live_tracks_.erase(current);
        live_trajectory_delta delta;
        delta.operation_ = live_trajectory_operation::remove;
        delta.track_ = _track;
        vqec_vision_ai_appl_mdsvc_append_live_delta_locked(std::move(delta));
        stats_.live_track_count_ = live_tracks_.size();
        return {};
    }

    status vqec_vision_ai_appl_mdsvc_get_live_snapshot(
        std::uint32_t _allowed_access_domain_mask,
        live_trajectory_snapshot& _snapshot) const {
        if (_allowed_access_domain_mask == 0U ||
            (_allowed_access_domain_mask & ~g_spatiotemporal_all_access_domains) != 0U) {
            return {status_code::invalid_argument, "live snapshot access mask is invalid"};
        }
        std::lock_guard<std::mutex> lock(mutex_);
        _snapshot = {};
        _snapshot.sequence_ = stats_.live_sequence_;
        try {
            _snapshot.chunks_.reserve(live_tracks_.size());
            for (const auto& entry : live_tracks_) {
                if (vqec_vision_ai_appl_mdsvc_is_authorized(
                        entry.second, _allowed_access_domain_mask)) {
                    _snapshot.chunks_.push_back(entry.second);
                }
            }
        } catch (const std::bad_alloc&) {
            _snapshot = {};
            return {status_code::resource_exhausted, "live snapshot allocation failed"};
        }
        return {};
    }

    status vqec_vision_ai_appl_mdsvc_get_live_deltas(
        std::uint64_t _after_sequence, std::size_t _maximum_results,
        std::uint32_t _allowed_access_domain_mask,
        std::vector<live_trajectory_delta>& _deltas,
        bool& _has_sequence_gap) const {
        if (_maximum_results == 0U ||
            _maximum_results > config_.maximum_live_deltas_ ||
            _allowed_access_domain_mask == 0U ||
            (_allowed_access_domain_mask & ~g_spatiotemporal_all_access_domains) != 0U) {
            return {status_code::invalid_argument, "live delta request is invalid"};
        }
        std::lock_guard<std::mutex> lock(mutex_);
        _deltas.clear();
        _has_sequence_gap = false;
        if (!live_deltas_.empty() && _after_sequence != 0U &&
            _after_sequence < live_deltas_.front().sequence_ - 1U) {
            _has_sequence_gap = true;
            return {};
        }
        try {
            _deltas.reserve(std::min(_maximum_results, live_deltas_.size()));
            for (const auto& delta : live_deltas_) {
                if (delta.sequence_ <= _after_sequence) {
                    continue;
                }
                if (delta.operation_ != live_trajectory_operation::upsert ||
                    vqec_vision_ai_appl_mdsvc_is_authorized(
                        delta.chunk_, _allowed_access_domain_mask)) {
                    _deltas.push_back(delta);
                }
                if (_deltas.size() == _maximum_results) {
                    break;
                }
            }
        } catch (const std::bad_alloc&) {
            _deltas.clear();
            return {status_code::resource_exhausted, "live delta allocation failed"};
        }
        return {};
    }

    status vqec_vision_ai_appl_mdsvc_query(
        const spatiotemporal_query& _query, spatiotemporal_query_page& _page) {
        const auto valid = vqec_vision_ai_cntr_stmet_validate_query(_query);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        std::shared_ptr<metadata_query_work> work;
        try {
            work = std::make_shared<metadata_query_work>();
            work->query_ = _query;
        } catch (const std::bad_alloc&) {
            return {status_code::resource_exhausted, "metadata query allocation failed"};
        }
        auto result = vqec_vision_ai_appl_mdsvc_enqueue(metadata_work_item{work});
        if (result.code_ != status_code::ok) {
            return result;
        }
        const auto deadline = std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(_query.budget_.deadline_ns_));
        std::unique_lock<std::mutex> lock(work->mutex_);
        if (!work->condition_.wait_until(lock, deadline,
                [&work]() { return work->is_complete_; })) {
            return {status_code::pending, "metadata query deadline elapsed before execution"};
        }
        _page = std::move(work->page_);
        return work->result_;
    }

    metadata_service_stats vqec_vision_ai_appl_mdsvc_get_stats() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    status vqec_vision_ai_appl_mdsvc_get_store_stats(
        spatiotemporal_store_stats& _stats) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_store_stats_) {
            return {status_code::pending, "metadata store statistics are not available"};
        }
        _stats = store_stats_;
        return {};
    }

private:
    status vqec_vision_ai_appl_mdsvc_enqueue(metadata_work_item _item) {
        status result;
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            result = vqec_vision_ai_appl_mdsvc_enqueue_locked(std::move(_item));
        } catch (const std::bad_alloc&) {
            result = {status_code::resource_exhausted, "metadata queue allocation failed"};
        }
        if (result.code_ == status_code::ok) {
            condition_.notify_one();
        }
        return result;
    }

    status vqec_vision_ai_appl_mdsvc_enqueue_locked(metadata_work_item _item) {
        if (!is_started_ || is_stop_requested_) {
            ++stats_.rejected_records_;
            return {status_code::invalid_state, "metadata service is not accepting work"};
        }
        if (queue_.size() >= config_.queue_capacity_) {
            ++stats_.rejected_records_;
            return {status_code::resource_exhausted, "metadata service queue is full"};
        }
        queue_.push_back(std::move(_item));
        ++stats_.accepted_records_;
        stats_.queue_depth_ = queue_.size();
        stats_.queue_high_watermark_ =
            std::max(stats_.queue_high_watermark_, queue_.size());
        return {};
    }

    void vqec_vision_ai_appl_mdsvc_append_live_delta_locked(
        live_trajectory_delta _delta) {
        ++stats_.live_sequence_;
        _delta.sequence_ = stats_.live_sequence_;
        if (live_deltas_.size() == config_.maximum_live_deltas_) {
            live_deltas_.pop_front();
        }
        live_deltas_.push_back(std::move(_delta));
    }

    void vqec_vision_ai_appl_mdsvc_upsert_live_locked(const trajectory_chunk& _chunk) {
        const auto key = vqec_vision_ai_appl_mdsvc_make_track_key(_chunk.track_);
        const auto existing = live_tracks_.find(key);
        if (existing == live_tracks_.end() &&
            live_tracks_.size() == config_.maximum_live_tracks_) {
            auto oldest = std::min_element(live_tracks_.begin(), live_tracks_.end(),
                [](const auto& _left, const auto& _right) {
                    return _left.second.last_frame_.source_pts_ns_ <
                        _right.second.last_frame_.source_pts_ns_;
                });
            live_trajectory_delta removal;
            removal.operation_ = live_trajectory_operation::remove;
            removal.track_ = oldest->second.track_;
            vqec_vision_ai_appl_mdsvc_append_live_delta_locked(std::move(removal));
            live_tracks_.erase(oldest);
        }
        live_tracks_[key] = _chunk;
        live_trajectory_delta delta;
        delta.operation_ = live_trajectory_operation::upsert;
        delta.track_ = _chunk.track_;
        delta.chunk_ = _chunk;
        vqec_vision_ai_appl_mdsvc_append_live_delta_locked(std::move(delta));
        stats_.live_track_count_ = live_tracks_.size();
    }

    void vqec_vision_ai_appl_mdsvc_complete_query(
        const std::shared_ptr<metadata_query_work>& _work, status _result,
        spatiotemporal_query_page _page) {
        {
            std::lock_guard<std::mutex> lock(_work->mutex_);
            _work->result_ = std::move(_result);
            _work->page_ = std::move(_page);
            _work->is_complete_ = true;
        }
        _work->condition_.notify_one();
    }

    void vqec_vision_ai_appl_mdsvc_reject_pending_queries_locked() noexcept {
        for (auto& item : queue_) {
            auto* work = std::get_if<std::shared_ptr<metadata_query_work>>(&item.payload_);
            if (work != nullptr && *work != nullptr) {
                vqec_vision_ai_appl_mdsvc_complete_query(*work,
                    {status_code::invalid_state, "metadata service stopped before query"}, {});
            }
        }
    }

    void vqec_vision_ai_appl_mdsvc_process(metadata_work_item _item) {
        status result;
        if (const auto* chunk = std::get_if<trajectory_chunk>(&_item.payload_)) {
            result = store_.vqec_vision_ai_stor_stsql_ingest_trajectory(
                *chunk, config_.outbox_sinks_);
        } else if (const auto* association =
                       std::get_if<track_association_revision>(&_item.payload_)) {
            result = store_.vqec_vision_ai_stor_stsql_ingest_association(*association);
        } else if (const auto* episode =
                       std::get_if<event_episode_revision>(&_item.payload_)) {
            result = store_.vqec_vision_ai_stor_stsql_ingest_episode(
                *episode, config_.outbox_sinks_);
        } else if (const auto* contribution =
                       std::get_if<aggregate_contribution_revision>(&_item.payload_)) {
            result = store_.vqec_vision_ai_stor_stsql_ingest_aggregate_contribution(
                *contribution, config_.outbox_sinks_);
        } else {
            const auto work = std::get<std::shared_ptr<metadata_query_work>>(_item.payload_);
            spatiotemporal_query_page page;
            result = store_.vqec_vision_ai_stor_stsql_query(work->query_, page);
            vqec_vision_ai_appl_mdsvc_complete_query(work, result, std::move(page));
            std::lock_guard<std::mutex> lock(mutex_);
            if (result.code_ == status_code::ok) {
                ++stats_.completed_queries_;
            } else {
                ++stats_.failed_queries_;
            }
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (result.code_ == status_code::ok) {
            ++stats_.committed_records_;
        } else {
            ++stats_.failed_records_;
        }
        has_store_stats_ =
            store_.vqec_vision_ai_stor_stsql_get_stats(store_stats_).code_ == status_code::ok;
    }

    void vqec_vision_ai_appl_mdsvc_process_projection_batch(
        std::vector<metadata_work_item> _items) {
        std::vector<event_episode_revision> episodes;
        std::vector<aggregate_contribution_revision> contributions;
        try {
            episodes.reserve(_items.size());
            contributions.reserve(_items.size());
            for (auto& item : _items) {
                if (auto* episode = std::get_if<event_episode_revision>(&item.payload_)) {
                    episodes.push_back(std::move(*episode));
                } else if (auto* contribution =
                               std::get_if<aggregate_contribution_revision>(&item.payload_)) {
                    contributions.push_back(std::move(*contribution));
                }
            }
        } catch (const std::bad_alloc&) {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.failed_records_ += _items.size();
            return;
        }
        const auto result = store_.vqec_vision_ai_stor_stsql_ingest_projection_batch(
            episodes, contributions, config_.outbox_sinks_);
        std::lock_guard<std::mutex> lock(mutex_);
        if (result.code_ == status_code::ok) {
            stats_.committed_records_ += _items.size();
        } else {
            stats_.failed_records_ += _items.size();
        }
        has_store_stats_ =
            store_.vqec_vision_ai_stor_stsql_get_stats(store_stats_).code_ == status_code::ok;
    }

    void vqec_vision_ai_appl_mdsvc_run() noexcept {
        std::vector<metadata_work_item> projection_batch;
        try {
            projection_batch.reserve(config_.maximum_batch_records_);
        } catch (const std::bad_alloc&) {
            const auto close_result = store_.vqec_vision_ai_stor_stsql_close();
            std::lock_guard<std::mutex> lock(mutex_);
            worker_result_ = close_result.code_ == status_code::ok
                ? status{status_code::resource_exhausted,
                      "metadata batch allocation failed"}
                : close_result;
            return;
        }
        for (;;) {
            metadata_work_item item;
            projection_batch.clear();
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock,
                    [this]() { return is_stop_requested_ || !queue_.empty(); });
                if (is_stop_requested_ && (!should_drain_ || queue_.empty())) {
                    break;
                }
                item = std::move(queue_.front());
                queue_.pop_front();
                if (vqec_vision_ai_appl_mdsvc_is_projection_work(item)) {
                    projection_batch.push_back(std::move(item));
                    while (!queue_.empty() &&
                           projection_batch.size() < config_.maximum_batch_records_ &&
                           vqec_vision_ai_appl_mdsvc_is_projection_work(queue_.front())) {
                        projection_batch.push_back(std::move(queue_.front()));
                        queue_.pop_front();
                    }
                }
                stats_.queue_depth_ = queue_.size();
            }
            try {
                if (!projection_batch.empty()) {
                    vqec_vision_ai_appl_mdsvc_process_projection_batch(
                        std::move(projection_batch));
                } else {
                    vqec_vision_ai_appl_mdsvc_process(std::move(item));
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex_);
                ++stats_.failed_records_;
                worker_result_ = {status_code::io_error,
                    "metadata worker caught an unexpected exception"};
            }
        }
        const auto close_result = store_.vqec_vision_ai_stor_stsql_close();
        std::lock_guard<std::mutex> lock(mutex_);
        if (worker_result_.code_ == status_code::ok &&
            close_result.code_ != status_code::ok) {
            worker_result_ = close_result;
        }
    }

    metadata_service_config config_;
    sqlite_spatiotemporal_store store_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
    std::deque<metadata_work_item> queue_;
    std::map<std::string, trajectory_chunk> live_tracks_;
    std::deque<live_trajectory_delta> live_deltas_;
    metadata_service_stats stats_;
    spatiotemporal_store_stats store_stats_;
    status worker_result_;
    bool has_store_stats_{false};
    bool is_started_{false};
    bool is_stop_requested_{false};
    bool should_drain_{true};
};

metadata_service::metadata_service(metadata_service_config _config)
    : implementation_(std::make_unique<implementation>(std::move(_config))) {}

metadata_service::~metadata_service() noexcept = default;

status metadata_service::vqec_vision_ai_appl_mdsvc_start() {
    return implementation_->vqec_vision_ai_appl_mdsvc_start();
}

status metadata_service::vqec_vision_ai_appl_mdsvc_stop(bool _drain) noexcept {
    return implementation_->vqec_vision_ai_appl_mdsvc_stop(_drain);
}

status metadata_service::vqec_vision_ai_appl_mdsvc_submit_trajectory(
    const trajectory_chunk& _chunk) {
    return implementation_->vqec_vision_ai_appl_mdsvc_submit_trajectory(_chunk);
}

status metadata_service::vqec_vision_ai_appl_mdsvc_submit_association(
    const track_association_revision& _association) {
    return implementation_->vqec_vision_ai_appl_mdsvc_submit_association(_association);
}

status metadata_service::vqec_vision_ai_appl_mdsvc_submit_episode(
    const event_episode_revision& _episode) {
    return implementation_->vqec_vision_ai_appl_mdsvc_submit_episode(_episode);
}

status metadata_service::vqec_vision_ai_appl_mdsvc_submit_aggregate_contribution(
    const aggregate_contribution_revision& _contribution) {
    return implementation_->vqec_vision_ai_appl_mdsvc_submit_aggregate_contribution(
        _contribution);
}

status metadata_service::vqec_vision_ai_appl_mdsvc_remove_live_track(
    const spatiotemporal_track_key& _track) {
    return implementation_->vqec_vision_ai_appl_mdsvc_remove_live_track(_track);
}

status metadata_service::vqec_vision_ai_appl_mdsvc_get_live_snapshot(
    std::uint32_t _allowed_access_domain_mask,
    live_trajectory_snapshot& _snapshot) const {
    return implementation_->vqec_vision_ai_appl_mdsvc_get_live_snapshot(
        _allowed_access_domain_mask, _snapshot);
}

status metadata_service::vqec_vision_ai_appl_mdsvc_get_live_deltas(
    std::uint64_t _after_sequence, std::size_t _maximum_results,
    std::uint32_t _allowed_access_domain_mask,
    std::vector<live_trajectory_delta>& _deltas, bool& _has_sequence_gap) const {
    return implementation_->vqec_vision_ai_appl_mdsvc_get_live_deltas(
        _after_sequence, _maximum_results, _allowed_access_domain_mask,
        _deltas, _has_sequence_gap);
}

status metadata_service::vqec_vision_ai_appl_mdsvc_query(
    const spatiotemporal_query& _query, spatiotemporal_query_page& _page) {
    return implementation_->vqec_vision_ai_appl_mdsvc_query(_query, _page);
}

metadata_service_stats metadata_service::vqec_vision_ai_appl_mdsvc_get_stats() const noexcept {
    return implementation_->vqec_vision_ai_appl_mdsvc_get_stats();
}

status metadata_service::vqec_vision_ai_appl_mdsvc_get_store_stats(
    spatiotemporal_store_stats& _stats) const {
    return implementation_->vqec_vision_ai_appl_mdsvc_get_store_stats(_stats);
}

}  // namespace vqec::vision::ai
