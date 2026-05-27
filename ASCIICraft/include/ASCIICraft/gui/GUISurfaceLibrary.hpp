#pragma once

#include <ASCIICraft/gui/GUISurface.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace gui {

class GUISurfaceLibrary {
public:
    void Register(const std::string& key, GUISurface surface);
    GUISurface Get(const std::string& key) const;
    GUISurface GetOrCreate(
        const std::string& key,
        const std::function<GUISurface()>& builder
    );

    void Clear();

private:
    std::unordered_map<std::string, GUISurface> m_surfaces;
};

} // namespace gui