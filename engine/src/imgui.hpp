#include <imgui.h>

template <class... Args>
void imgui_text(fmt::format_string<Args...> fmt, Args&&... args) {
    // Use a cached growing buffer for efficiency
    static fmt::memory_buffer buf;

    buf.clear();
    fmt::format_to(std::back_inserter(buf), fmt, std::forward<Args>(args)...);

    ImGui::TextUnformatted(buf.begin(), buf.end());
}
