#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include <unistd.h>

#include "vqec_vision_image_path_authorizer.hpp"

using namespace vqec::vision::ai;

int main() {
    char allowed_template[] = "/tmp/lacai-enrollment-allowed-XXXXXX";
    char denied_template[] = "/tmp/lacai-enrollment-denied-XXXXXX";
    const char* allowed_root = ::mkdtemp(allowed_template);
    const char* denied_root = ::mkdtemp(denied_template);
    assert(allowed_root != nullptr && denied_root != nullptr);
    const std::string image_path = std::string(allowed_root) + "/face.jpg";
    const std::string outside_path = std::string(denied_root) + "/outside.jpg";
    constexpr std::array<unsigned char, 6> jpeg_bytes{
        0xffU, 0xd8U, 0xffU, 0x00U, 0x01U, 0x02U};
    {
        std::ofstream image(image_path, std::ios::binary);
        image.write(reinterpret_cast<const char*>(jpeg_bytes.data()), jpeg_bytes.size());
        std::ofstream outside(outside_path, std::ios::binary);
        outside.write(reinterpret_cast<const char*>(jpeg_bytes.data()), jpeg_bytes.size());
    }
    image_path_authorizer authorizer;
    assert(authorizer.vqec_vision_ai_fwctl_ipath_configure(
        {{allowed_root}, 1024}).code_ == status_code::ok);
    authorized_image_path authorized;
    assert(authorizer.vqec_vision_ai_ports_ipath_authorize(image_path, authorized).code_ ==
        status_code::ok);
    assert(authorized.owner_ && authorized.path_.find("/proc/self/fd/") == 0);
    authorized_image_path preserved = authorized;
    assert(authorizer.vqec_vision_ai_ports_ipath_authorize(outside_path, authorized).code_ ==
        status_code::unauthorized);
    assert(authorized.path_ == preserved.path_ && authorized.owner_ == preserved.owner_);
    const std::string symlink_path = std::string(allowed_root) + "/escape.jpg";
    assert(::symlink(outside_path.c_str(), symlink_path.c_str()) == 0);
    assert(authorizer.vqec_vision_ai_ports_ipath_authorize(symlink_path, authorized).code_ ==
        status_code::unauthorized);
    std::remove(symlink_path.c_str());
    std::remove(image_path.c_str());
    std::remove(outside_path.c_str());
    ::rmdir(allowed_root);
    ::rmdir(denied_root);
    return 0;
}
