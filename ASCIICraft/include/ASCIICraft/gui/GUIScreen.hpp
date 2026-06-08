#pragma once

#include <ASCIICraft/gui/Panel.hpp>
#include <functional>
#include <memory>
#include <string>

namespace gui {

/// Root container for a full screen. Has blocksInput flag; when true, gameplay input is disabled.
class GUIScreen : public Panel {
public: 
    std::string name;
    bool blocksInput = false;
    
    /// Optional: key press (action id). Return true if consumed.
    virtual bool OnKey(const std::string& actionId) { (void)actionId; return false; }
    /// Optional: cursor hotspot moved (screen pixels).
    virtual void OnCursorMove(glm::vec2 hotspot) { (void)hotspot; }
    /// When this screen is opened, GUIManager places the cursor hotspot here (screen pixels).
    virtual bool TryGetInitialCursorPosition(glm::vec2 screenSize, glm::vec2& outHotspot) const {
        (void)screenSize;
        (void)outHotspot;
        return false;
    }
    /// Optional: click at cursor hotspot, button 0=left 1=right. Return true if consumed.
    virtual bool OnClick(glm::vec2 hotspot, int button) { (void)hotspot; (void)button; return false; }
};

} // namespace gui
