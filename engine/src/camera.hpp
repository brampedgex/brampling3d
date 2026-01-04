#pragma once

// The Camera manages a view and projection matrix based on its position and rotation in the world.
class Camera {
public:
    Camera() = default;
    Camera(const glm::vec3& pos, f32 pitch, f32 yaw, f32 fov) :
        m_pos(pos), m_pitch(pitch), m_yaw(yaw), m_fov(fov) {
    }

public: // Methods
    // Calculates directions on pitch/yaw change.
    void update_rot();

    // Calculates view and projection matrices.
    void update_matrices();

public: // Getters/setters
    [[nodiscard]] const auto& pos() const { return m_pos; }
    [[nodiscard]] auto pitch() const { return m_pitch; }
    [[nodiscard]] auto yaw() const { return m_yaw; }
    [[nodiscard]] auto fov() const { return m_fov; }
    [[nodiscard]] auto aspect_ratio() const { return m_aspect_ratio; }
    [[nodiscard]] auto near_plane() const { return m_near_plane; }
    [[nodiscard]] auto far_plane() const { return m_far_plane; }
    [[nodiscard]] const auto& rotation() const { return m_rotation; }
    [[nodiscard]] const auto& dir() const { return m_dir; }
    [[nodiscard]] const auto& dir_xz() const { return m_dir_xz; }
    [[nodiscard]] const auto& view_mtx() const { return m_view_mtx; }
    [[nodiscard]] const auto& proj_mtx() const { return m_proj_mtx; }

    void set_pos(const glm::vec3& pos) { m_pos = pos; }
    void set_pitch(f32 pitch) { m_pitch = pitch; }
    void set_yaw(f32 yaw) { m_yaw = yaw; }
    void set_fov(f32 fov) { m_fov = fov; }
    void set_aspect_ratio(f32 aspect_ratio) { m_aspect_ratio = aspect_ratio; }
    void set_near_plane(f32 near_plane) { m_near_plane = near_plane; }
    void set_far_plane(f32 far_plane) { m_far_plane = far_plane; }

private: // Private fields
    glm::vec3 m_pos{};
    // In the future, the Camera won't have any pitch/yaw and it'll just have the quaternion to be supplied by something else.
    // For now we don't have anywhere better to put pitch/yaw so it goes here.
    f32 m_pitch = 0;
    f32 m_yaw = 0;
    f32 m_fov = 0;
    f32 m_aspect_ratio = 1;
    f32 m_near_plane = 0.1;
    f32 m_far_plane = 1000;

    glm::quat m_rotation{};
    glm::vec3 m_dir{};
    glm::vec3 m_dir_xz{};

    glm::mat4x4 m_view_mtx;
    glm::mat4x4 m_proj_mtx;
};
