#pragma once

class Camera {
public:
    Camera() = default;
    Camera(const glm::vec3& pos, f32 pitch, f32 yaw, f32 fov) :
        m_pos(pos), m_pitch(pitch), m_yaw(yaw), m_fov(fov) {
    }

public: // Methods
    // Calculates directions on pitch/yaw change.
    void update_rot();

    [[nodiscard]] glm::mat4x4 calc_view_mtx() const;
    [[nodiscard]] glm::mat4x4 calc_proj_mtx(f32 aspect) const;

public: // Getters/setters
    [[nodiscard]] const auto& pos() const { return m_pos; }
    [[nodiscard]] auto pitch() const { return m_pitch; }
    [[nodiscard]] auto yaw() const { return m_yaw; }
    [[nodiscard]] auto fov() const { return m_fov; }

    [[nodiscard]] auto pitch_radians() const { return m_pitch_radians; }
    [[nodiscard]] auto yaw_radians() const { return m_yaw_radians; }
    [[nodiscard]] const auto& dir() const { return m_dir; }
    [[nodiscard]] const auto& dir_xz() const { return m_dir_xz; }

    void set_pos(const glm::vec3& pos) { m_pos = pos; }
    void set_pitch(f32 pitch) { m_pitch = pitch; }
    void set_yaw(f32 yaw) { m_yaw = yaw; }
    void set_fov(f32 fov) { m_fov = fov; }

private:
    glm::vec3 m_pos{};
    f32 m_pitch = 0;
    f32 m_yaw = 0;
    f32 m_fov = 0;

    // Calculated from above members:
    f32 m_pitch_radians = 0;
    f32 m_yaw_radians = 0;
    glm::vec3 m_dir{};
    glm::vec3 m_dir_xz{};
};
