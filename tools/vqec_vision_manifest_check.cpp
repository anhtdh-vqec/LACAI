#include <fstream>
#include <iostream>
#include <new>

#include "vqec_vision_output_manifest.hpp"

// main is the documented language-entrypoint naming exception. Read-only diagnostic.
int main(int _argc, char** _argv) {
    if (_argc != 2) {
        std::cerr << "usage: vqec_vision_ai_manifest_check <output-manifest.json>\n";
        return 2;
    }
    try {
        std::ifstream stream(_argv[1], std::ios::binary);
        if (!stream.is_open()) {
            std::cerr << "cannot open manifest\n";
            return 2;
        }
        vqec::vision::ai::model_outputs manifest;
        const auto result =
            vqec::vision::ai::vqec_vision_ai_mreg_otman_load_manifest(stream, manifest);
        if (result.code_ != vqec::vision::ai::status_code::ok) {
            std::cerr << result.message_ << '\n';
            return result.code_ == vqec::vision::ai::status_code::io_error ? 2 : 1;
        }
        std::cout << "metadata valid: model=" << manifest.model_id_
                  << " version=" << manifest.model_version_
                  << " outputs=" << manifest.outputs_.size()
                  << " budget_bytes=" << manifest.max_output_bytes_ << '\n'
                  << "artifact authenticity, decoder and board compatibility NOT verified\n";
        return 0;
    } catch (const std::bad_alloc&) {
        std::cerr << "insufficient memory to validate manifest\n";
        return 2;
    }
}
