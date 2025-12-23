#include "camera.hpp"

static constexpr f32 DEGREES_TO_RADIANS = std::numbers::pi_v<f32> / 180.f;

void Camera::update_rot() {
    m_pitch_radians = m_pitch * DEGREES_TO_RADIANS;
    m_yaw_radians = m_yaw * DEGREES_TO_RADIANS;

    m_dir_xz = {
        cos(m_yaw_radians),
        0,
        sin(m_yaw_radians)
    };

    f32 v_mul = sin(m_pitch_radians);
    f32 h_mul = cos(m_pitch_radians);

    m_dir = {
        m_dir_xz.x * h_mul,
        v_mul,
        m_dir_xz.z * h_mul,
    };
}

glm::mat4x4 Camera::calc_view_mtx() const {
    f32 xz_mag = m_dir_xz.length();

    f32 v_amount = sin(m_pitch_radians);
    f32 h_amount = cos(m_pitch_radians);
    
    // Calculate a direction that should be 90 degrees up from the viewing direction.
    glm::vec3 up{
        m_dir_xz.x * -v_amount,
        xz_mag * h_amount,
        m_dir_xz.z * -v_amount,
    };

    return glm::lookAt(m_pos, m_pos + m_dir, up);
}

glm::mat4x4 Camera::calc_proj_mtx(f32 aspect) const {
    glm::mat4x4 mtx = glm::perspective(glm::radians(m_fov), aspect, 0.1f, 100.f);
    mtx[1][1] *= -1; // Flip vertically to correct the screen space coordinates
    return mtx;
}
