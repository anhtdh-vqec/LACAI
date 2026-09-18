#include <iostream>
#include <sstream>

#include "vqec_vision_artifact_digest.hpp"

int main() {
    using namespace vqec::vision::ai;
    const std::string abc = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const auto verify = [&](const std::string& _bytes, const std::string& _hash,
                            std::uint64_t _limit, status_code _expected, bool _exceptions) {
        std::istringstream stream(_bytes);
        if (_exceptions) {
            stream.exceptions(std::ios::badbit | std::ios::failbit);
        }
        artifact_digest_receipt receipt{"preserved", 777};
        const auto result = vqec_vision_ai_mreg_ardgt_verify_stream(stream, _hash, _limit, receipt);
        check(result.code_ == _expected);
        if (_expected == status_code::ok) {
            check(receipt.sha256_ == _hash && receipt.bytes_ == _bytes.size());
        } else {
            check(receipt.sha256_ == "preserved" && receipt.bytes_ == 777);
        }
    };
    verify("abc", abc, 3, status_code::ok, false);
    verify("abc", abc, 4, status_code::ok, true);
    verify("abc", abc, 2, status_code::resource_exhausted, false);
    verify("abc", abc, 2, status_code::resource_exhausted, true);
    verify("abc", std::string(64, '0'), 4, status_code::protocol_error, false);
    verify("", abc, 4, status_code::invalid_argument, false);
    verify("abc", "invalid", 4, status_code::invalid_argument, false);
    verify("abc", std::string(64, 'A'), 4, status_code::invalid_argument, false);
    verify("abc", abc, 0, status_code::invalid_argument, false);
    verify("abc", abc, 4ULL * 1024 * 1024 * 1024 + 1, status_code::invalid_argument, false);
    // Published SHA-256 million-'a' vector crosses many scratch-buffer boundaries.
    verify(std::string(1000000, 'a'),
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        1000000, status_code::ok, true);
    std::istringstream broken("abc");
    broken.setstate(std::ios::badbit);
    artifact_digest_receipt receipt{"preserved", 777};
    check(vqec_vision_ai_mreg_ardgt_verify_stream(broken, abc, 3, receipt).code_ ==
          status_code::io_error);
    check(receipt.sha256_ == "preserved" && receipt.bytes_ == 777);
    std::istringstream untouched("abc");
    check(vqec_vision_ai_mreg_ardgt_verify_stream(untouched, "bad", 3, receipt).code_ ==
          status_code::invalid_argument);
    check(untouched.tellg() == std::streampos(0));
    std::cout << "artifact digest failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
