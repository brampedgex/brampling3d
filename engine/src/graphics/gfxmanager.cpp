#include "gfxmanager.hpp"

bool GraphicsManager::initialize() {
    return true;
}

void GraphicsManager::update() {
    // Handle swapchain recreation before rendering a frame.
    if (m_window_resized || m_need_swapchain_recreate) {
        m_window_resized = m_need_swapchain_recreate = false;

        recreate_swapchain();
    }

    render_frame();
}
