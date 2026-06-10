#include <ASCIICraft/gui/GUISurfaceLibrary.hpp>

namespace gui {

void GUISurfaceLibrary::Register(const std::string& key, GUISurface surface) {
    if (key.empty() || !surface.mesh) return;
    m_surfaces[key] = std::move(surface);
}

GUISurface GUISurfaceLibrary::Get(const std::string& key) const {
    if (key.empty()) return {};
    auto it = m_surfaces.find(key);
    if (it == m_surfaces.end()) return {};
    return it->second;
}

GUISurface GUISurfaceLibrary::GetOrCreate(
    const std::string& key,
    const std::function<GUISurface()>& builder
) {
    if (key.empty()) return {};
    if (auto existing = Get(key); existing.mesh) return existing;
    if (!builder) return {};
    GUISurface surface = builder();
    if (!surface.mesh) return {};
    m_surfaces[key] = surface;
    return surface;
}

void GUISurfaceLibrary::Clear() {
    m_surfaces.clear();
}

} // namespace gui