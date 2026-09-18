#include <iostream>
#include <sstream>
#include <string>

#include "vqec_vision_output_manifest.hpp"

int main() {
    using namespace vqec::vision::ai;
    const std::string valid = R"({"schema_version":1,"model_id":"fixture",
"model_version":"1.0","artifact_sha256":
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
"decoder_contract":"fixture.raw.v1","max_output_bytes":20,
"outputs":[{"name":"boxes","dtype":"float32","shape":[1,4]},
{"name":"scores","dtype":"float32","shape":[1]}]})";
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const auto parse = [&](const std::string& _text, status_code _expected) {
        std::istringstream stream(_text);
        model_outputs output;
        output.model_id_ = "preserved";
        output.max_output_bytes_ = 777;
        const auto status = vqec_vision_ai_mreg_otman_load_manifest(stream, output);
        check(status.code_ == _expected);
        if (_expected != status_code::ok) {
            check(output.model_id_ == "preserved" && output.max_output_bytes_ == 777);
        } else {
            check(output.model_id_ == "fixture" && output.outputs_.size() == 2);
            check(output.max_output_bytes_ == 20 && !output.outputs_.empty() &&
                  output.outputs_[0].name_ == "boxes");
        }
    };
    const auto replace = [&](const std::string& _from, const std::string& _to) {
        auto text = valid;
        const auto index = text.find(_from);
        if (index == std::string::npos) {
            ++failures;
        } else {
            text.replace(index, _from.size(), _to);
        }
        return text;
    };
    parse(valid, status_code::ok);
    parse("", status_code::invalid_argument);
    parse(valid + "{}", status_code::invalid_argument);
    parse(replace("\"schema_version\":1", "\"schema_version\":2"), status_code::unsupported);
    parse(replace("\"schema_version\":1", "\"schema_version\":1.0"), status_code::invalid_argument);
    parse(replace("\"model_id\":\"fixture\"", "\"model_id\":\"fixture\",\"model_id\":\"x\""),
          status_code::invalid_argument);
    parse(replace("\"shape\":[1,4]", "\"shape\":[1,4],\"shape\":[1,4]"),
          status_code::invalid_argument);
    // Reviewed dtypes parse; an unknown spelling is rejected.
    {
        std::istringstream stream(replace("\"dtype\":\"float32\"", "\"dtype\":\"int8\""));
        model_outputs parsed;
        check(vqec_vision_ai_mreg_otman_load_manifest(stream, parsed).code_ ==
              status_code::ok);
        check(parsed.outputs_.size() == 2 &&
              parsed.outputs_[0].dtype_ == tensor_element_type::int8);
    }
    {
        auto text = replace(
            "{\"name\":\"boxes\",\"dtype\":\"float32\",\"shape\":[1,4]}",
            "{\"name\":\"boxes\",\"dtype\":\"int8\",\"shape\":[1,4],"
            "\"quantization\":{\"scale\":0.5,\"zero_point\":-1}}");
        std::istringstream stream(text);
        model_outputs parsed;
        check(vqec_vision_ai_mreg_otman_load_manifest(stream, parsed).code_ ==
              status_code::ok);
        check(parsed.outputs_.size() == 2 && parsed.outputs_[0].quantization_.is_quantized_ &&
              parsed.outputs_[0].quantization_.scale_ == 0.5F &&
              parsed.outputs_[0].quantization_.zero_point_ == -1);
    }
    parse(replace("\"dtype\":\"float32\"", "\"dtype\":\"bfloat16\""),
          status_code::unsupported);
    parse(replace("\"shape\":[1,4]", "\"shape\":[0,4]"), status_code::invalid_argument);
    parse(replace("\"shape\":[1,4]", "\"shape\":[-1,4]"), status_code::invalid_argument);
    parse(replace("\"shape\":[1,4]", "\"shape\":[true,4]"), status_code::invalid_argument);
    parse(replace("\"name\":\"scores\"", "\"name\":\"boxes\""), status_code::invalid_argument);
    parse(replace("\"max_output_bytes\":20", "\"max_output_bytes\":19"),
          status_code::resource_exhausted);
    parse(replace("\"model_version\"", "\"unknown\""), status_code::invalid_argument);
    parse(replace("\"shape\":[1,4]", "\"shape\":[18446744073709551616,4]"),
          status_code::invalid_argument);
    parse(replace("\"shape\":[1,4]", "\"shape\":" + std::string(32, '[') + "1" +
                  std::string(32, ']')), status_code::invalid_argument);
    parse(valid + std::string(65536 - valid.size(), ' '), status_code::ok);
    parse(valid + std::string(65537 - valid.size(), ' '), status_code::resource_exhausted);
    std::istringstream exceptions(valid);
    exceptions.exceptions(std::ios::failbit | std::ios::badbit);
    model_outputs output;
    check(vqec_vision_ai_mreg_otman_load_manifest(exceptions, output).code_ == status_code::ok);
    std::istringstream broken(valid);
    broken.setstate(std::ios::badbit);
    output.model_id_ = "preserved";
    check(vqec_vision_ai_mreg_otman_load_manifest(broken, output).code_ == status_code::io_error);
    check(output.model_id_ == "preserved");
    std::cout << "output manifest failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
