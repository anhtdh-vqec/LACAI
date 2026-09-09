#include "vqec_vision_plugin_graph.hpp"
#include "vqec_vision_tensor_output.hpp"
#include "vqec_vision_frame_submission.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

#include <array>
#include <memory>
#include <string>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

namespace vqec::vision::ai {
namespace {

struct element_deleter {
    void operator()(GstElement* _element) const noexcept {
        if (_element != nullptr) {
            gst_object_unref(_element);
        }
    }
};

struct caps_deleter {
    void operator()(GstCaps* _caps) const noexcept {
        if (_caps != nullptr) {
            gst_caps_unref(_caps);
        }
    }
};

struct enum_deleter {
    void operator()(GEnumClass* _enum_class) const noexcept {
        if (_enum_class != nullptr) {
            g_type_class_unref(_enum_class);
        }
    }
};

struct error_deleter {
    void operator()(GError* _error) const noexcept {
        if (_error != nullptr) {
            g_error_free(_error);
        }
    }
};

using element_owner = std::unique_ptr<GstElement, element_deleter>;
using caps_owner = std::unique_ptr<GstCaps, caps_deleter>;

std::string vqec_vision_ai_qcom_plgr_bounded_text(const char* _text, std::size_t _limit) {
    if (_text == nullptr) {
        return "unknown";
    }
    std::size_t length = 0;
    while (length < _limit && _text[length] != '\0') {
        ++length;
    }
    return std::string(_text, length);
}

struct value_holder {
    explicit value_holder(GType _type) {
        g_value_init(&value_, _type);
    }
    ~value_holder() noexcept {
        g_value_unset(&value_);
    }
    value_holder(const value_holder& _other) = delete;
    value_holder& operator=(const value_holder& _other) = delete;

    GValue value_ = G_VALUE_INIT;
};

status vqec_vision_ai_qcom_plgr_set_value(
    GObject* _object, const char* _name, value_holder& _value) {
    auto* specification = g_object_class_find_property(G_OBJECT_GET_CLASS(_object), _name);
    if (specification == nullptr || (specification->flags & G_PARAM_WRITABLE) == 0 ||
        (specification->flags & G_PARAM_CONSTRUCT_ONLY) != 0 ||
        G_PARAM_SPEC_VALUE_TYPE(specification) != G_VALUE_TYPE(&_value.value_)) {
        return {status_code::incompatible_plugin,
                std::string("Property type/flags mismatch: ") + _name};
    }
    // Reject clamping instead of silently running with changed preprocessing.
    if (g_param_value_validate(specification, &_value.value_)) {
        return {status_code::invalid_argument,
                std::string("Property value out of range: ") + _name};
    }
    g_object_set_property(_object, _name, &_value.value_);
    return {};
}

status vqec_vision_ai_qcom_plgr_set_string(
    GObject* _object, const char* _name, const std::string& _text) {
    value_holder value(G_TYPE_STRING);
    g_value_set_string(&value.value_, _text.c_str());
    return vqec_vision_ai_qcom_plgr_set_value(_object, _name, value);
}

status vqec_vision_ai_qcom_plgr_set_boolean(
    GObject* _object, const char* _name, bool _enabled) {
    value_holder value(G_TYPE_BOOLEAN);
    g_value_set_boolean(&value.value_, _enabled);
    return vqec_vision_ai_qcom_plgr_set_value(_object, _name, value);
}

status vqec_vision_ai_qcom_plgr_set_uint(
    GObject* _object, const char* _name, std::uint32_t _number) {
    value_holder value(G_TYPE_UINT);
    g_value_set_uint(&value.value_, _number);
    return vqec_vision_ai_qcom_plgr_set_value(_object, _name, value);
}

status vqec_vision_ai_qcom_plgr_set_uint64(
    GObject* _object, const char* _name, std::uint64_t _number) {
    value_holder value(G_TYPE_UINT64);
    g_value_set_uint64(&value.value_, _number);
    return vqec_vision_ai_qcom_plgr_set_value(_object, _name, value);
}

status vqec_vision_ai_qcom_plgr_set_enum(
    GObject* _object, const char* _name, const char* _nick) {
    auto* specification = g_object_class_find_property(G_OBJECT_GET_CLASS(_object), _name);
    if (specification == nullptr || !G_IS_PARAM_SPEC_ENUM(specification)) {
        return {status_code::incompatible_plugin, std::string("Missing enum property: ") + _name};
    }
    const auto enum_type = G_PARAM_SPEC_VALUE_TYPE(specification);
    std::unique_ptr<GEnumClass, enum_deleter> enum_class(
        static_cast<GEnumClass*>(g_type_class_ref(enum_type)));
    const auto* enum_value = g_enum_get_value_by_nick(enum_class.get(), _nick);
    if (enum_value == nullptr) {
        return {status_code::incompatible_plugin, std::string("Unsupported enum nick: ") + _nick};
    }
    value_holder value(enum_type);
    g_value_set_enum(&value.value_, enum_value->value);
    return vqec_vision_ai_qcom_plgr_set_value(_object, _name, value);
}

status vqec_vision_ai_qcom_plgr_set_coefficients(
    GObject* _object, const char* _name, const std::array<double, 3>& _coefficients) {
    value_holder values(GST_TYPE_ARRAY);
    for (const auto coefficient : _coefficients) {
        value_holder value(G_TYPE_DOUBLE);
        g_value_set_double(&value.value_, coefficient);
        gst_value_array_append_value(&values.value_, &value.value_);
    }
    return vqec_vision_ai_qcom_plgr_set_value(_object, _name, values);
}

status vqec_vision_ai_qcom_plgr_set_caps(
    GObject* _object, GstCaps* _caps) {
    value_holder value(GST_TYPE_CAPS);
    g_value_set_boxed(&value.value_, _caps);
    return vqec_vision_ai_qcom_plgr_set_value(_object, "caps", value);
}

status vqec_vision_ai_qcom_plgr_add_element(
    GstElement* _pipeline, const char* _factory, const char* _name, GstElement*& _element) {
    element_owner element(gst_element_factory_make(_factory, _name));
    if (!element) {
        return {status_code::missing_plugin, std::string("Cannot create factory: ") + _factory};
    }
    if (!gst_bin_add(GST_BIN(_pipeline), element.get())) {
        return {status_code::incompatible_plugin, std::string("Cannot add element: ") + _name};
    }
    _element = element.release();  // Borrowed from pipeline after ownership transfer.
    return {};
}

caps_owner vqec_vision_ai_qcom_plgr_make_tensor_caps(const inference_plan& _plan) {
    caps_owner caps(gst_caps_new_simple(
        "neural-network/tensors", "type", G_TYPE_STRING,
        _plan.input_type_ == tensor_type::uint8 ? "UINT8" : "FLOAT32", nullptr));
    value_holder tensors(GST_TYPE_ARRAY);
    value_holder dimensions(GST_TYPE_ARRAY);
    const std::array<int, 4> shape{
        1, static_cast<int>(_plan.tensor_height_), static_cast<int>(_plan.tensor_width_), 3};
    for (const auto dimension : shape) {
        value_holder value(G_TYPE_INT);
        g_value_set_int(&value.value_, dimension);
        gst_value_array_append_value(&dimensions.value_, &value.value_);
    }
    gst_value_array_append_value(&tensors.value_, &dimensions.value_);
    gst_caps_set_value(caps.get(), "dimensions", &tensors.value_);
    return caps;
}

const char* vqec_vision_ai_qcom_plgr_get_placement_nick(image_placement _placement) noexcept {
    switch (_placement) {
        case image_placement::top_left:
            return "top-left";
        case image_placement::centre:
            return "centre";
        case image_placement::stretch:
            return "stretch";
        default:
            return "";  // Rejected by plan validation before this helper is called.
    }
}

}  // namespace

struct plugin_graph::implementation {
    element_owner pipeline_;
    inference_plan plan_;
    std::unique_ptr<source_binding> binding_;
    plugin_graph_state state_{plugin_graph_state::configured};
    plugin_graph_error last_error_;
    std::uint64_t warning_count_{0};
    GstElement* source_{nullptr};  // Borrowed from pipeline.
    GstElement* sink_{nullptr};
    std::vector<float_tensor_spec> outputs_;
    std::uint64_t max_output_bytes_{0};
    bool eos_seen_{false};
    std::unique_ptr<submission_window> window_;
    frame_submission job_;
    bool result_consumed_{false};
    ~implementation() noexcept {
        if (pipeline_ && state_ != plugin_graph_state::configured) {
            // Armed unsafe graphs never enter this destructor: retained by their domain.
            gst_element_set_state(pipeline_.get(), GST_STATE_NULL);
        }
    }
};

plugin_graph::plugin_graph() = default;

status plugin_graph::vqec_vision_ai_qcom_plgr_probe_factories(
    const std::vector<std::string>& _factory_names,
    std::vector<plugin_factory_capability>& _capabilities) const {
    _capabilities.clear();
    if (_factory_names.size() > 64U) {
        return {status_code::invalid_argument, "factory probe list exceeds bounded capacity"};
    }
    for (const auto& factory_name : _factory_names) {
        if (factory_name.empty() || factory_name.size() > 128U) {
            return {status_code::invalid_argument, "factory name is empty or too long"};
        }
    }
    if (_factory_names.empty()) {
        return {};
    }
    GError* init_error = nullptr;
    const auto is_initialized = gst_init_check(nullptr, nullptr, &init_error);
    std::unique_ptr<GError, error_deleter> error(init_error);
    if (!is_initialized) {
        return {status_code::incompatible_plugin,
                error ? error->message : "GStreamer initialization failed"};
    }
    _capabilities.reserve(_factory_names.size());
    for (const auto& factory_name : _factory_names) {
        auto* factory = gst_element_factory_find(factory_name.c_str());
        _capabilities.push_back({factory_name, factory != nullptr});
        if (factory != nullptr) {
            gst_object_unref(factory);
        }
    }
    return {};
}

struct graph_retention::implementation {
    std::array<bool, 4> reserved_{};
    std::array<std::unique_ptr<plugin_graph::implementation>, 4> retained_{};
    ~implementation() noexcept {
        for (auto& graph : retained_) {
            if (graph) {
                // Last-resort containment, never use pool destruction as hardware cancellation.
                (void)graph.release();
                g_warning("AI graph retained without supervisor; BSP recovery required");
            }
        }
    }
};

graph_retention::graph_retention() : implementation_(std::make_unique<implementation>()) {}
graph_retention::~graph_retention() noexcept = default;

unsigned graph_retention::vqec_vision_ai_qcom_plgr_get_retained_count() const noexcept {
    unsigned count = 0;
    for (const auto& graph : implementation_->retained_) {
        count += graph ? 1U : 0U;
    }
    return count;
}

status graph_retention::vqec_vision_ai_qcom_plgr_restore_graph(
    unsigned _slot, plugin_graph& _graph, const std::shared_ptr<graph_retention>& _domain) {
    if (_domain.get() != this || _slot >= implementation_->retained_.size() ||
        !implementation_->retained_[_slot] || _graph.implementation_ || _graph.retention_) {
        return {status_code::invalid_argument, "invalid domain, slot or nonempty restore target"};
    }
    _graph.implementation_ = std::move(implementation_->retained_[_slot]);
    _graph.retention_ = _domain;
    _graph.retention_slot_ = _slot;
    return {};
}

plugin_graph::~plugin_graph() noexcept {
    if (retention_ && implementation_) {
        auto& pool = *retention_->implementation_;
        if (implementation_->state_ != plugin_graph_state::configured) {
            pool.retained_[retention_slot_] = std::move(implementation_);
            g_warning("AI graph moved to retention slot %u; explicit recovery needed",
                      retention_slot_);
        } else {
            pool.reserved_[retention_slot_] = false;
        }
    }
}

bool plugin_graph::vqec_vision_ai_qcom_plgr_is_configured() const noexcept {
    return implementation_ != nullptr;
}

#if defined(VQEC_VISION_AI_GRAPH_TEST_FIXTURE)
status plugin_graph::vqec_vision_ai_qcom_plgr_configure_fixture(const inference_plan& _plan) {
    if (implementation_) {
        return {status_code::invalid_state, "fixture requires an empty graph"};
    }
    const auto validation = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (validation.code_ != status_code::ok) {
        return validation;
    }
    gst_init(nullptr, nullptr);
    auto candidate = std::make_unique<implementation>();
    candidate->pipeline_.reset(gst_pipeline_new(nullptr));
    if (!candidate->pipeline_) {
        return {status_code::resource_exhausted, "cannot allocate test pipeline"};
    }
    auto created = vqec_vision_ai_qcom_plgr_add_element(
        candidate->pipeline_.get(), "appsrc", "input", candidate->source_);
    if (created.code_ != status_code::ok) {
        return created;
    }
    created = vqec_vision_ai_qcom_plgr_add_element(
        candidate->pipeline_.get(), "appsink", "output", candidate->sink_);
    if (created.code_ != status_code::ok) {
        return created;
    }
    // Test bytes are zero-filled FLOAT32 payload, not real NV12 inference output.
    caps_owner caps(gst_caps_from_string(
        "neural-network/tensors,type=(string)FLOAT32,dimensions=<<(int)256>>"));
    if (!caps) {
        return {status_code::invalid_argument, "invalid fixture caps"};
    }
    g_object_set(candidate->source_, "caps", caps.get(), "format", GST_FORMAT_TIME,
                 "is-live", TRUE, "block", FALSE, nullptr);
    g_object_set(candidate->sink_, "sync", FALSE, "async", FALSE,
                 "enable-last-sample", FALSE, "max-buffers", 1U, "drop", FALSE, nullptr);
    if (!gst_element_link(candidate->source_, candidate->sink_)) {
        return {status_code::graph_link_failed, "cannot link test source to sink"};
    }
    candidate->plan_ = _plan;
    implementation_ = std::move(candidate);
    return {};
}
#endif

status plugin_graph::vqec_vision_ai_qcom_plgr_configure_graph(const inference_plan& _plan) {
    if (implementation_ && implementation_->state_ != plugin_graph_state::configured) {
        return {status_code::invalid_state, "unload model before reconfiguring graph"};
    }
    const auto validation = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (validation.code_ != status_code::ok) {
        return validation;
    }
    GError* init_error = nullptr;
    const auto is_initialized = gst_init_check(nullptr, nullptr, &init_error);
    std::unique_ptr<GError, error_deleter> error(init_error);
    if (!is_initialized) {
        return {status_code::incompatible_plugin,
                error ? error->message : "GStreamer initialization failed"};
    }
    // Process-wide GStreamer initialization is not undone by individual graphs.
    // No state transition: failed construction cannot start hardware readers.
    auto candidate = std::make_unique<implementation>();
    candidate->pipeline_.reset(gst_pipeline_new("vqec_vision_ai_inference"));
    if (!candidate->pipeline_) {
        return {status_code::incompatible_plugin, "Cannot allocate pipeline"};
    }

    GstElement* source = nullptr;
    GstElement* converter = nullptr;
    GstElement* tensor_filter = nullptr;
    GstElement* inference = nullptr;
    GstElement* sink = nullptr;
    const std::array<status, 5> creation{
        vqec_vision_ai_qcom_plgr_add_element(candidate->pipeline_.get(), "appsrc", "input", source),
        vqec_vision_ai_qcom_plgr_add_element(
            candidate->pipeline_.get(), "qtimlvconverter", "preprocess", converter),
        vqec_vision_ai_qcom_plgr_add_element(
            candidate->pipeline_.get(), "capsfilter", "model_input", tensor_filter),
        vqec_vision_ai_qcom_plgr_add_element(
            candidate->pipeline_.get(), "qtimlqnn", "inference", inference),
        vqec_vision_ai_qcom_plgr_add_element(
            candidate->pipeline_.get(), "appsink", "output", sink)};
    for (const auto& result : creation) {
        if (result.code_ != status_code::ok) {
            return result;
        }
    }

    caps_owner video_caps(gst_caps_new_simple(
        "video/x-raw", "format", G_TYPE_STRING, "NV12",
        "width", G_TYPE_INT, static_cast<int>(_plan.source_width_),
        "height", G_TYPE_INT, static_cast<int>(_plan.source_height_),
        "framerate", GST_TYPE_FRACTION, static_cast<int>(_plan.fps_numerator_),
        static_cast<int>(_plan.fps_denominator_), nullptr));
    auto tensor_caps = vqec_vision_ai_qcom_plgr_make_tensor_caps(_plan);

    const std::array<status, 21> settings{
        vqec_vision_ai_qcom_plgr_set_caps(G_OBJECT(source), video_caps.get()),
        vqec_vision_ai_qcom_plgr_set_enum(G_OBJECT(source), "format", "time"),
        vqec_vision_ai_qcom_plgr_set_boolean(G_OBJECT(source), "is-live", true),
        vqec_vision_ai_qcom_plgr_set_boolean(G_OBJECT(source), "do-timestamp", false),
        vqec_vision_ai_qcom_plgr_set_boolean(G_OBJECT(source), "block", false),
        vqec_vision_ai_qcom_plgr_set_uint64(
            G_OBJECT(source), "max-bytes", _plan.input_queue_bytes_),
        vqec_vision_ai_qcom_plgr_set_enum(G_OBJECT(converter), "engine", "fcv"),
        vqec_vision_ai_qcom_plgr_set_enum(
            G_OBJECT(converter), "mode", "image-batch-non-cumulative"),
        vqec_vision_ai_qcom_plgr_set_enum(
            G_OBJECT(converter), "image-disposition",
            vqec_vision_ai_qcom_plgr_get_placement_nick(_plan.placement_)),
        vqec_vision_ai_qcom_plgr_set_enum(
            G_OBJECT(converter), "subpixel-layout",
            _plan.channel_order_ == channel_order::rgb ? "regular" : "reverse"),
        vqec_vision_ai_qcom_plgr_set_coefficients(G_OBJECT(converter), "mean", _plan.mean_),
        vqec_vision_ai_qcom_plgr_set_coefficients(G_OBJECT(converter), "sigma", _plan.sigma_),
        vqec_vision_ai_qcom_plgr_set_caps(G_OBJECT(tensor_filter), tensor_caps.get()),
        vqec_vision_ai_qcom_plgr_set_string(G_OBJECT(inference), "model", _plan.model_path_),
        vqec_vision_ai_qcom_plgr_set_string(G_OBJECT(inference), "backend", _plan.backend_path_),
        vqec_vision_ai_qcom_plgr_set_string(G_OBJECT(inference), "system", _plan.system_path_),
        vqec_vision_ai_qcom_plgr_set_uint(
            G_OBJECT(sink), "max-buffers", _plan.output_queue_buffers_),
        vqec_vision_ai_qcom_plgr_set_boolean(G_OBJECT(sink), "drop", false),
        vqec_vision_ai_qcom_plgr_set_boolean(G_OBJECT(sink), "async", false),
        vqec_vision_ai_qcom_plgr_set_boolean(G_OBJECT(sink), "enable-last-sample", false),
        vqec_vision_ai_qcom_plgr_set_boolean(G_OBJECT(sink), "sync", false)};
    for (const auto& result : settings) {
        if (result.code_ != status_code::ok) {
            return result;
        }
    }
    if (!gst_element_link_many(source, converter, tensor_filter, inference, sink, nullptr)) {
        return {status_code::graph_link_failed, "Cannot link the configured inference graph"};
    }
    // Transaction commit; the previous NULL-state graph is safely released.
    candidate->source_ = source;
    candidate->sink_ = sink;
    candidate->plan_ = _plan;
    implementation_ = std::move(candidate);
    return {};
}

status plugin_graph::vqec_vision_ai_qcom_plgr_load_model() {
    if (!implementation_ || implementation_->state_ != plugin_graph_state::configured) {
        return {status_code::invalid_state, "model load requires a configured NULL graph"};
    }
    implementation_->last_error_ = {};
    implementation_->state_ = plugin_graph_state::loading;
    const auto result = gst_element_set_state(implementation_->pipeline_.get(), GST_STATE_READY);
    if (result == GST_STATE_CHANGE_FAILURE) {
        implementation_->state_ = plugin_graph_state::faulted;
        const auto diagnostic = vqec_vision_ai_qcom_plgr_poll_state();
        (void)diagnostic;
        return {status_code::incompatible_plugin, "plugin failed model initialization at READY"};
    }
    return vqec_vision_ai_qcom_plgr_poll_state();
}

status plugin_graph::vqec_vision_ai_qcom_plgr_unload_model() {
    if (!implementation_) {
        return {status_code::invalid_state, "graph is not configured"};
    }
    if (implementation_->state_ == plugin_graph_state::configured) {
        return {};
    }
    if (vqec_vision_ai_qcom_plgr_get_outstanding() != 0) {
        return {status_code::pending, "cannot unload while committed jobs remain"};
    }
    if (implementation_->state_ == plugin_graph_state::starting ||
        implementation_->state_ == plugin_graph_state::playing ||
        implementation_->state_ == plugin_graph_state::draining) {
        return {status_code::invalid_state, "request drain and consume output before unloading"};
    }
    const auto reset = implementation_->job_.vqec_vision_ai_qcom_frsub_reset();
    if (reset.code_ != status_code::ok) {
        return reset;
    }
    implementation_->result_consumed_ = false;
    implementation_->state_ = plugin_graph_state::unloading;
    const auto result = gst_element_set_state(implementation_->pipeline_.get(), GST_STATE_NULL);
    if (result == GST_STATE_CHANGE_FAILURE) {
        implementation_->state_ = plugin_graph_state::faulted;
        return {status_code::incompatible_plugin, "plugin failed model teardown at NULL"};
    }
    return vqec_vision_ai_qcom_plgr_poll_state();
}

status plugin_graph::vqec_vision_ai_qcom_plgr_poll_state() {
    if (!implementation_) {
        return {status_code::invalid_state, "graph is not configured"};
    }
    auto& state = implementation_->state_;
    auto* bus = gst_element_get_bus(implementation_->pipeline_.get());
    if (bus == nullptr) {
        state = plugin_graph_state::faulted;
        return {status_code::incompatible_plugin, "pipeline has no bus"};
    }
    // RAII through generic GstObject-compatible unref; no callback or worker created.
    const auto release_bus = [](GstBus* _bus) { gst_object_unref(_bus); };
    std::unique_ptr<GstBus, decltype(release_bus)> bus_owner(bus, release_bus);
    unsigned messages = 0;
    while (messages < 32) {
        auto* message = gst_bus_pop(bus);
        if (message == nullptr) {
            break;
        }
        const auto release_message = [](GstMessage* _message) { gst_message_unref(_message); };
        std::unique_ptr<GstMessage, decltype(release_message)> message_owner(
            message, release_message);
        ++messages;
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
            GError* raw_error = nullptr;
            gst_message_parse_error(message, &raw_error, nullptr);
            std::unique_ptr<GError, error_deleter> error(raw_error);
            state = plugin_graph_state::faulted;
            const auto* name = GST_MESSAGE_SRC(message) ?
                GST_OBJECT_NAME(GST_MESSAGE_SRC(message)) : nullptr;
            auto& stored = implementation_->last_error_;
            stored.source_ = vqec_vision_ai_qcom_plgr_bounded_text(name, 128);
            if (error) {
                const auto* domain = g_quark_to_string(error->domain);
                stored.domain_ = vqec_vision_ai_qcom_plgr_bounded_text(domain, 128);
                stored.code_ = error->code;
                stored.message_ = vqec_vision_ai_qcom_plgr_bounded_text(error->message, 1024);
            }
        } else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_WARNING) {
            if (implementation_->warning_count_ != UINT64_MAX) {
                ++implementation_->warning_count_;
            }
        } else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
            if (state == plugin_graph_state::draining || state == plugin_graph_state::drained) {
                implementation_->eos_seen_ = true;
            } else {
                state = plugin_graph_state::faulted;
                implementation_->last_error_.message_ = "unexpected EOS without drain request";
            }
        }
    }
    if (state == plugin_graph_state::faulted) {
        if (implementation_->window_) {
            implementation_->window_->vqec_vision_ai_core_subwn_mark_fault();
        }
        return {status_code::incompatible_plugin, "graph faulted; reconcile jobs before unload"};
    }
    if (messages == 32) {
        return {status_code::pending, "bus drain budget exhausted; poll again"};
    }
    GstState current = GST_STATE_VOID_PENDING;
    GstState pending = GST_STATE_VOID_PENDING;
    const auto transition = gst_element_get_state(
        implementation_->pipeline_.get(), &current, &pending, 0);
    if (transition == GST_STATE_CHANGE_FAILURE) {
        state = plugin_graph_state::faulted;
        return {status_code::incompatible_plugin, "graph state transition failed"};
    }
    if (transition == GST_STATE_CHANGE_ASYNC || pending != GST_STATE_VOID_PENDING) {
        return {status_code::pending, "graph state transition is pending"};
    }
    if (state == plugin_graph_state::unloading && current == GST_STATE_NULL) {
        state = plugin_graph_state::configured;
        implementation_->outputs_.clear();
        implementation_->binding_.reset();
        implementation_->eos_seen_ = false;
        implementation_->window_.reset();
        if (retention_) {
            retention_->implementation_->reserved_[retention_slot_] = false;
            retention_.reset();
            retention_slot_ = 4;
        }
    } else if (state == plugin_graph_state::loading && current == GST_STATE_READY) {
        state = plugin_graph_state::ready;
    } else if (state == plugin_graph_state::starting && current == GST_STATE_PLAYING) {
        state = plugin_graph_state::playing;
    } else if ((state == plugin_graph_state::ready && current != GST_STATE_READY) ||
               (state == plugin_graph_state::configured && current != GST_STATE_NULL) ||
               ((state == plugin_graph_state::playing || state == plugin_graph_state::draining ||
                 state == plugin_graph_state::drained) && current != GST_STATE_PLAYING)) {
        state = plugin_graph_state::faulted;
        return {status_code::invalid_state, "unexpected graph state"};
    } else if (state == plugin_graph_state::loading || state == plugin_graph_state::unloading ||
               state == plugin_graph_state::starting) {
        return {status_code::pending, "graph has not reached requested state"};
    }
    if (state == plugin_graph_state::draining) {
        if (implementation_->eos_seen_ &&
            gst_app_sink_is_eos(GST_APP_SINK(implementation_->sink_)) &&
            vqec_vision_ai_qcom_plgr_get_outstanding() == 0) {
            state = plugin_graph_state::drained;
        } else {
            return {status_code::pending, "waiting for EOS and output consumption"};
        }
    }
    return {};
}

status plugin_graph::vqec_vision_ai_qcom_plgr_bind_source(const source_binding& _binding) {
    if (!implementation_ || implementation_->state_ != plugin_graph_state::ready) {
        return {status_code::invalid_state, "source binding requires a READY model"};
    }
    const auto validation = vqec_vision_ai_core_srcbd_validate_binding(
        _binding, implementation_->plan_);
    if (validation.code_ != status_code::ok) {
        return validation;
    }
    // Allocate/copy before mutating the appsrc. Keep the prior binding on failure.
    auto candidate = std::make_unique<source_binding>(_binding);
    caps_owner caps(gst_app_src_get_caps(GST_APP_SRC(implementation_->source_)));
    if (!caps || !gst_caps_is_fixed(caps.get()) || gst_caps_get_size(caps.get()) != 1) {
        return {status_code::incompatible_plugin, "appsrc has no fixed input caps"};
    }
    caps_owner updated(gst_caps_copy(caps.get()));
    if (!updated) {
        return {status_code::resource_exhausted, "cannot copy source caps"};
    }
    gst_caps_set_simple(updated.get(),
        "colorimetry", G_TYPE_STRING,
        _binding.color_profile_ == source_color_profile::bt601_limited ? "bt601" : "bt709",
        "chroma-site", G_TYPE_STRING,
        _binding.chroma_site_ == source_chroma_site::mpeg2 ? "mpeg2" : "jpeg", nullptr);
    const auto configured = vqec_vision_ai_qcom_plgr_set_caps(
        G_OBJECT(implementation_->source_), updated.get());
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    implementation_->binding_ = std::move(candidate);
    return {};
}

status plugin_graph::vqec_vision_ai_qcom_plgr_start_stream(
    const std::vector<float_tensor_spec>& _outputs, std::uint64_t _max_output_bytes) {
    if (!implementation_ || implementation_->state_ != plugin_graph_state::ready) {
        return {status_code::invalid_state, "start_stream requires a READY model"};
    }
    if (!implementation_->binding_) {
        return {status_code::invalid_state, "bind explicit source color and memory policy first"};
    }
    std::uint64_t required_bytes = 0;
    const auto outputs = vqec_vision_ai_core_tnctr_validate_outputs(
        _outputs, _max_output_bytes, required_bytes);
    if (outputs.code_ != status_code::ok) {
        return outputs;
    }
    implementation_->outputs_ = _outputs;
    implementation_->max_output_bytes_ = _max_output_bytes;
    implementation_->eos_seen_ = false;
    implementation_->state_ = plugin_graph_state::starting;
    if (gst_element_set_state(implementation_->pipeline_.get(), GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        implementation_->state_ = plugin_graph_state::faulted;
        return {status_code::incompatible_plugin, "cannot start inference pipeline"};
    }
    return vqec_vision_ai_qcom_plgr_poll_state();
}

status plugin_graph::vqec_vision_ai_qcom_plgr_request_drain() {
    if (!implementation_) {
        return {status_code::invalid_state, "graph not configured"};
    }
    auto& state = implementation_->state_;
    if (state == plugin_graph_state::draining || state == plugin_graph_state::drained) {
        return vqec_vision_ai_qcom_plgr_poll_state();
    }
    if (state != plugin_graph_state::playing) {
        return {status_code::invalid_state, "drain requires a PLAYING graph"};
    }
    state = plugin_graph_state::draining;
    if (implementation_->window_) {
        implementation_->window_->vqec_vision_ai_core_subwn_begin_drain();
    }
    if (gst_app_src_end_of_stream(GST_APP_SRC(implementation_->source_)) != GST_FLOW_OK) {
        state = plugin_graph_state::faulted;
        return {status_code::io_error, "appsrc rejected EOS request"};
    }
    return vqec_vision_ai_qcom_plgr_poll_state();
}

status plugin_graph::vqec_vision_ai_qcom_plgr_poll_output(
    std::uint64_t _expected_pipeline_pts_ns, tensor_result& _result) {
    if (!implementation_ ||
        (implementation_->state_ != plugin_graph_state::playing &&
         implementation_->state_ != plugin_graph_state::draining &&
         implementation_->state_ != plugin_graph_state::faulted)) {
        return {status_code::invalid_state, "output polling requires an active or faulted graph"};
    }
    if (_expected_pipeline_pts_ns == UINT64_MAX) {
        return {status_code::invalid_argument, "expected job PTS must be valid"};
    }
    if (!implementation_->window_ ||
        !implementation_->job_.vqec_vision_ai_qcom_frsub_has_submission() ||
        implementation_->result_consumed_ ||
        implementation_->job_.vqec_vision_ai_qcom_frsub_get_ticket().pipeline_pts_ns_ !=
            _expected_pipeline_pts_ns) {
        return {status_code::invalid_state, "no pending submitted ticket matching expected PTS"};
    }
    const auto health = vqec_vision_ai_qcom_plgr_poll_state();
    const bool is_faulted = implementation_->state_ == plugin_graph_state::faulted;
    if (health.code_ != status_code::ok && health.code_ != status_code::pending && !is_faulted) {
        return health;
    }
    const auto release_sample = [](GstSample* _sample) { gst_sample_unref(_sample); };
    std::unique_ptr<GstSample, decltype(release_sample)> sample(
        gst_app_sink_try_pull_sample(GST_APP_SINK(implementation_->sink_), 0), release_sample);
    if (!sample) {
        return {status_code::pending, "no tensor sample available"};
    }
    auto* buffer = gst_sample_get_buffer(sample.get());
    if (buffer == nullptr || GST_BUFFER_PTS(buffer) != _expected_pipeline_pts_ns) {
        implementation_->state_ = plugin_graph_state::faulted;
        implementation_->window_->vqec_vision_ai_core_subwn_mark_fault();
        return {status_code::protocol_error, "tensor sample PTS does not match expected job"};
    }
    tensor_result candidate;
    const auto copied = vqec_vision_ai_qcom_tnout_copy_sample(
        sample.get(), implementation_->outputs_, implementation_->max_output_bytes_, candidate);
    if (copied.code_ != status_code::ok) {
        implementation_->state_ = plugin_graph_state::faulted;
        implementation_->window_->vqec_vision_ai_core_subwn_mark_fault();
        return copied;
    }
    sample.reset();  // Result bookkeeping follows release of all adapter sample readers.
    const auto completed = implementation_->job_.vqec_vision_ai_qcom_frsub_complete_result(
        *implementation_->window_, _expected_pipeline_pts_ns);
    if (completed.code_ != status_code::ok) {
        implementation_->state_ = plugin_graph_state::faulted;
        implementation_->window_->vqec_vision_ai_core_subwn_mark_fault();
        return completed;
    }
    implementation_->result_consumed_ = true;
    if (is_faulted) {
        return {status_code::incompatible_plugin, "late result discarded on faulted graph"};
    }
    _result = std::move(candidate);
    return {};
}

status plugin_graph::vqec_vision_ai_qcom_plgr_arm_submission(
    std::uint64_t _cycle_id, std::uint64_t _source_epoch, std::uint64_t _job_timeout_ns,
    const std::shared_ptr<graph_retention>& _retention) {
    if (!implementation_ || implementation_->state_ != plugin_graph_state::playing ||
        !implementation_->binding_ || implementation_->window_ || !_retention) {
        return {status_code::invalid_state,
                "arm requires bound PLAYING graph and retention domain"};
    }
    auto& pool = *_retention->implementation_;
    unsigned slot = 0;
    while (slot < pool.reserved_.size() && pool.reserved_[slot]) {
        ++slot;
    }
    if (slot == pool.reserved_.size()) {
        return {status_code::resource_exhausted, "graph retention domain is full"};
    }
    const auto release_clock = [](GstClock* _clock) { gst_object_unref(_clock); };
    std::unique_ptr<GstClock, decltype(release_clock)> clock(
        gst_element_get_clock(implementation_->pipeline_.get()), release_clock);
    if (!clock) {
        return {status_code::pending, "pipeline has not selected a clock"};
    }
    const auto base = gst_element_get_base_time(implementation_->pipeline_.get());
    const auto now = gst_clock_get_time(clock.get());
    if (!GST_CLOCK_TIME_IS_VALID(base) || !GST_CLOCK_TIME_IS_VALID(now) || now < base) {
        return {status_code::invalid_state, "pipeline running-time anchor unavailable"};
    }
    auto window = std::make_unique<submission_window>();
    const auto configured = window->vqec_vision_ai_core_subwn_configure(
        {_cycle_id, _source_epoch, now - base, _job_timeout_ns, 1});
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    pool.reserved_[slot] = true;
    retention_ = _retention;
    retention_slot_ = slot;
    implementation_->window_ = std::move(window);
    return {};
}

unsigned plugin_graph::vqec_vision_ai_qcom_plgr_get_outstanding() const noexcept {
    return implementation_ && implementation_->window_ ?
        implementation_->window_->vqec_vision_ai_core_subwn_get_outstanding() : 0;
}

submission_ticket plugin_graph::vqec_vision_ai_qcom_plgr_get_pending_ticket() const noexcept {
    return implementation_ && implementation_->job_.vqec_vision_ai_qcom_frsub_has_submission() ?
        implementation_->job_.vqec_vision_ai_qcom_frsub_get_ticket() : submission_ticket{};
}

status plugin_graph::vqec_vision_ai_qcom_plgr_poll_result(
    std::uint64_t _steady_now_ns, tensor_result& _result) {
    if (!implementation_ || !implementation_->window_) {
        return {status_code::invalid_state, "result polling requires armed graph"};
    }
    const auto jobs = vqec_vision_ai_qcom_plgr_poll_jobs(_steady_now_ns);
    if (implementation_->state_ != plugin_graph_state::playing &&
        implementation_->state_ != plugin_graph_state::draining &&
        implementation_->state_ != plugin_graph_state::faulted) {
        return jobs.code_ == status_code::ok ?
            status{status_code::pending, "no active result"} : jobs;
    }
    if (!implementation_->job_.vqec_vision_ai_qcom_frsub_has_submission() ||
        implementation_->result_consumed_) {
        const auto health = vqec_vision_ai_qcom_plgr_poll_state();
        if (jobs.code_ != status_code::ok && jobs.code_ != status_code::pending) {
            return jobs;
        }
        return health.code_ == status_code::ok ?
            status{status_code::pending, "no pending result"} : health;
    }
    const auto output = vqec_vision_ai_qcom_plgr_poll_output(
        implementation_->job_.vqec_vision_ai_qcom_frsub_get_ticket().pipeline_pts_ns_, _result);
    return jobs.code_ != status_code::ok && jobs.code_ != status_code::pending ? jobs : output;
}

status plugin_graph::vqec_vision_ai_qcom_plgr_poll_jobs(std::uint64_t _steady_now_ns) {
    if (!implementation_ || !implementation_->window_) {
        return {status_code::invalid_state, "graph submission window is not armed"};
    }
    auto& impl = *implementation_;
    if (impl.state_ != plugin_graph_state::playing && impl.state_ != plugin_graph_state::draining &&
        impl.state_ != plugin_graph_state::drained && impl.state_ != plugin_graph_state::faulted) {
        return {status_code::invalid_state, "job polling cannot interrupt graph state transitions"};
    }
    if (impl.state_ == plugin_graph_state::faulted) {
        impl.window_->vqec_vision_ai_core_subwn_mark_fault();
    }
    const auto deadlines = impl.window_->vqec_vision_ai_core_subwn_check_deadlines(_steady_now_ns);
    if (deadlines.code_ != status_code::ok) {
        impl.state_ = plugin_graph_state::faulted;
    }
    if (impl.job_.vqec_vision_ai_qcom_frsub_has_submission()) {
        const auto input = impl.job_.vqec_vision_ai_qcom_frsub_poll_input(*impl.window_);
        if (input.code_ != status_code::ok && input.code_ != status_code::pending) {
            impl.state_ = plugin_graph_state::faulted;
            impl.window_->vqec_vision_ai_core_subwn_mark_fault();
            return input;
        }
        if (impl.window_->vqec_vision_ai_core_subwn_get_outstanding() == 0) {
            const auto reset = impl.job_.vqec_vision_ai_qcom_frsub_reset();
            if (reset.code_ != status_code::ok) {
                return reset;
            }
            impl.result_consumed_ = false;
        }
    }
    if (deadlines.code_ != status_code::ok) {
        return deadlines;
    }
    return vqec_vision_ai_qcom_plgr_get_outstanding() == 0 ? status{} :
        status{status_code::pending, "waiting for input and result completion"};
}

status plugin_graph::vqec_vision_ai_qcom_plgr_submit_frame(
    const frame_descriptor& _descriptor, int _frame_fd, std::shared_ptr<const void> _owner,
    std::uint64_t _steady_now_ns, submission_ticket& _ticket) {
    if (!implementation_ || !implementation_->window_ ||
        implementation_->state_ != plugin_graph_state::playing) {
        return {status_code::invalid_state, "submit requires armed PLAYING graph"};
    }
    if (_ticket.token_.cycle_id_ != 0 || _ticket.token_.job_id_ != 0) {
        return {status_code::invalid_argument, "submit output ticket must be empty"};
    }
    const auto jobs = vqec_vision_ai_qcom_plgr_poll_jobs(_steady_now_ns);
    if (jobs.code_ != status_code::ok) {
        return jobs;
    }
    const auto health = vqec_vision_ai_qcom_plgr_poll_state();
    if (health.code_ != status_code::ok) {
        return health;
    }
    auto& impl = *implementation_;
    const dmabuf_bridge_profile profile{impl.plan_.source_width_, impl.plan_.source_height_,
                                        64ULL * 1024 * 1024};
    const auto pushed = vqec_vision_ai_qcom_frsub_push_frame(
        GST_APP_SRC(impl.source_), *impl.window_, _descriptor, _frame_fd, std::move(_owner),
        profile, impl.plan_.input_queue_bytes_, _steady_now_ns, impl.job_);
    if (impl.job_.vqec_vision_ai_qcom_frsub_has_submission()) {
        _ticket = impl.job_.vqec_vision_ai_qcom_frsub_get_ticket();
        if (pushed.code_ != status_code::ok) {
            impl.state_ = plugin_graph_state::faulted;
        }
    }
    return pushed;
}

plugin_graph_state plugin_graph::vqec_vision_ai_qcom_plgr_get_state() const noexcept {
    return implementation_ ? implementation_->state_ : plugin_graph_state::empty;
}

plugin_graph_error plugin_graph::vqec_vision_ai_qcom_plgr_get_last_error() const {
    return implementation_ ? implementation_->last_error_ : plugin_graph_error{};
}

std::uint64_t plugin_graph::vqec_vision_ai_qcom_plgr_get_warning_count() const noexcept {
    return implementation_ ? implementation_->warning_count_ : 0;
}

}  // namespace vqec::vision::ai
