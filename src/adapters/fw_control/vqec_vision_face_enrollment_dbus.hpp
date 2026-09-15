#ifndef VQEC_VISION_AI_FWCTL_FACE_ENROLLMENT_DBUS_HPP
#define VQEC_VISION_AI_FWCTL_FACE_ENROLLMENT_DBUS_HPP

#include <memory>

#include "vqec/vision/ai/ports/vqec_vision_face_enrollment.hpp"

namespace vqec::vision::ai {

namespace face_enrollment_dbus_protocol {
inline constexpr char g_bus_name[] = "com.vqec.Lacai";
inline constexpr char g_object_path[] = "/com/vqec/Lacai/FaceEnrollment";
inline constexpr char g_interface_name[] = "com.vqec.Lacai.FaceEnrollment1";
inline constexpr char g_begin_method[] = "BeginEnrollment";
inline constexpr char g_cancel_method[] = "CancelEnrollment";
inline constexpr char g_remove_method[] = "RemoveSubject";
inline constexpr char g_status_method[] = "GetEnrollmentStatus";
inline constexpr char g_begin_signature[] = "(sssuututu)";
inline constexpr char g_cancel_signature[] = "(s)";
inline constexpr char g_remove_signature[] = "(st)";
inline constexpr char g_status_signature[] = "(s)";
}

class face_enrollment_dbus_server final {
public:
    face_enrollment_dbus_server();
    ~face_enrollment_dbus_server() noexcept;
    face_enrollment_dbus_server(const face_enrollment_dbus_server& _other) = delete;
    face_enrollment_dbus_server& operator=(
        const face_enrollment_dbus_server& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_fwctl_fedbs_open(
        face_enrollment_port& _port, bool _use_session_bus);
    // Run a bounded number of pending callbacks from the caller-owned main context.
    // The runtime owner remains serialized because callbacks execute synchronously.
    void vqec_vision_ai_fwctl_fedbs_poll() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FWCTL_FACE_ENROLLMENT_DBUS_HPP
