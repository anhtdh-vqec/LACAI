#include "vqec_vision_hardware_admission_profile.hpp"

#include <initializer_list>
#include <limits>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using profile_json = nlohmann::json;
struct invalid_profile {};

void vqec_vision_ai_admis_hwprf_require_keys(
    const profile_json& _object, std::initializer_list<const char*> _keys) {
    if (!_object.is_object() || _object.size() != _keys.size()) {
        throw invalid_profile{};
    }
    for (const auto* key : _keys) {
        if (!_object.contains(key)) {
            throw invalid_profile{};
        }
    }
}

std::uint64_t vqec_vision_ai_admis_hwprf_read_u64(const profile_json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_profile{};
    }
    return _value.get<std::uint64_t>();
}

std::uint32_t vqec_vision_ai_admis_hwprf_read_u32(const profile_json& _value) {
    const auto value = vqec_vision_ai_admis_hwprf_read_u64(_value);
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw invalid_profile{};
    }
    return static_cast<std::uint32_t>(value);
}

std::uint16_t vqec_vision_ai_admis_hwprf_read_u16(const profile_json& _value) {
    const auto value = vqec_vision_ai_admis_hwprf_read_u64(_value);
    if (value > std::numeric_limits<std::uint16_t>::max()) {
        throw invalid_profile{};
    }
    return static_cast<std::uint16_t>(value);
}

std::string vqec_vision_ai_admis_hwprf_read_text(const profile_json& _value) {
    if (!_value.is_string()) {
        throw invalid_profile{};
    }
    auto value = _value.get<std::string>();
    if (value.empty() ||
        value.size() > hardware_profile_document_limits::g_max_text_bytes ||
        value.find('\0') != std::string::npos) {
        throw invalid_profile{};
    }
    return value;
}

}  // namespace

status vqec_vision_ai_admis_hwprf_load(
    std::istream& _stream, hardware_admission_profile& _profile) {
    std::string document;
    try {
        document.reserve(1024);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == hardware_profile_document_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "hardware admission profile exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "hardware admission profile stream read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "hardware admission profile allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read hardware admission profile"};
    }

    try {
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int _depth, profile_json::parse_event_t _event,
                                  profile_json& _value) {
            if (_depth > hardware_profile_document_limits::g_max_json_depth) {
                throw invalid_profile{};
            }
            if (_event == profile_json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == profile_json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_profile{};
                }
            } else if (_event == profile_json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        const auto root = profile_json::parse(document, callback);
        vqec_vision_ai_admis_hwprf_require_keys(root,
            {"schema_version", "profile_id", "target_id", "revision",
             "measurement_reference", "max_total_resident_bytes",
             "max_frame_pool_bytes", "max_tensor_pool_bytes",
             "max_encoder_pool_bytes", "max_cascade_roi_bytes",
             "max_ddr_bandwidth_mbps", "max_fw_concurrency_slots",
             "max_worker_concurrency", "min_thermal_headroom_pct"});
        if (vqec_vision_ai_admis_hwprf_read_u32(root.at("schema_version")) != 1U) {
            return {status_code::unsupported,
                "unsupported hardware admission profile schema"};
        }
        hardware_admission_profile candidate;
        candidate.profile_id_ =
            vqec_vision_ai_admis_hwprf_read_text(root.at("profile_id"));
        candidate.target_id_ =
            vqec_vision_ai_admis_hwprf_read_text(root.at("target_id"));
        candidate.revision_ = vqec_vision_ai_admis_hwprf_read_u64(root.at("revision"));
        candidate.measurement_reference_ =
            vqec_vision_ai_admis_hwprf_read_text(root.at("measurement_reference"));
        candidate.max_total_resident_bytes_ =
            vqec_vision_ai_admis_hwprf_read_u64(root.at("max_total_resident_bytes"));
        candidate.max_frame_pool_bytes_ =
            vqec_vision_ai_admis_hwprf_read_u64(root.at("max_frame_pool_bytes"));
        candidate.max_tensor_pool_bytes_ =
            vqec_vision_ai_admis_hwprf_read_u64(root.at("max_tensor_pool_bytes"));
        candidate.max_encoder_pool_bytes_ =
            vqec_vision_ai_admis_hwprf_read_u64(root.at("max_encoder_pool_bytes"));
        candidate.max_cascade_roi_bytes_ =
            vqec_vision_ai_admis_hwprf_read_u64(root.at("max_cascade_roi_bytes"));
        candidate.max_ddr_bandwidth_mbps_ =
            vqec_vision_ai_admis_hwprf_read_u32(root.at("max_ddr_bandwidth_mbps"));
        candidate.max_fw_concurrency_slots_ =
            vqec_vision_ai_admis_hwprf_read_u16(root.at("max_fw_concurrency_slots"));
        candidate.max_worker_concurrency_ =
            vqec_vision_ai_admis_hwprf_read_u16(root.at("max_worker_concurrency"));
        candidate.min_thermal_headroom_pct_ =
            vqec_vision_ai_admis_hwprf_read_u16(root.at("min_thermal_headroom_pct"));
        if (!candidate.is_valid()) {
            return {status_code::invalid_argument,
                "hardware admission profile values are invalid"};
        }
        _profile = std::move(candidate);
        return {};
    } catch (const invalid_profile&) {
        return {status_code::invalid_argument, "invalid hardware admission profile"};
    } catch (const nlohmann::json::exception&) {
        return {status_code::invalid_argument, "invalid hardware admission profile JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "hardware admission profile parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
