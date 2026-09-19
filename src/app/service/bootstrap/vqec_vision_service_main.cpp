// Required external executable entrypoint. Service lifecycle and composition live in
// dedicated application owners; main only delegates and returns their process result.

#include "vqec_vision_service_runtime.hpp"

int main(int _argc, char** _argv) {
    return vqec_vision_ai_appl_svcmn_run_service(_argc, _argv);
}
