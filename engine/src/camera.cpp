#include "camera.hpp"

void Camera::update_rot() {
    // Default rotation (0 yaw or pitch) is -Z, this way +X is right and +Y is up.
    // To rotate, we first do yaw by rotating around the global Y axis.
    // Then we do pitch by rotating around the local X axis (which is to the right)
    // This gives us a quaternion we can use to rotate a forward and up vector to create our view matrix.
    // We can also do fun stuff like tilting the camera.

    // TODO: This should probably be done outside of Camera, in some CameraControl struct, with Camera just having the rot quaternion
    glm::quat q_yaw = glm::angleAxis(glm::radians(m_yaw), glm::vec3{ 0, -1, 0 });
    glm::quat q_pitch = glm::angleAxis(glm::radians(m_pitch), glm::vec3{ 1, 0, 0 });
    m_rotation = q_yaw * q_pitch;

    glm::vec3 dir = m_rotation * glm::vec3(0, 0, -1);
    m_dir = dir;

    // For horizontal direction we can multiply by just the yaw quat
    glm::vec3 dir_xz = q_yaw * glm::vec3(0, 0, -1);
    m_dir_xz = dir_xz;
}

glm::mat4x4 Camera::calc_view_mtx() const {
    // Calculate an up direction from our rotation, this way it works looking 90 degrees up/down or with tilting.
    glm::vec3 up = m_rotation * glm::vec3(0.f, 1.f, 0.f);

    return glm::lookAt(m_pos, m_pos + m_dir, up);
}

glm::mat4x4 Camera::calc_proj_mtx(f32 aspect) const {
    glm::mat4x4 mtx = glm::perspective(glm::radians(m_fov), aspect, 0.1f, 100.f);
    mtx[1][1] *= -1; // Flip vertically to correct the screen space coordinates
    return mtx;
}
