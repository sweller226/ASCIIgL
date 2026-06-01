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
    /// Optional: cursor moved to screen position.
    virtual void OnCursorMove(glm::vec2 position) { (void)position; }
    /// When this screen is opened, GUIManager may place the cursor here (screen pixels).
    virtual bool TryGetInitialCursorPosition(glm::vec2 screenSize, glm::vec2& out) const {
        (void)screenSize;
        (void)out;
        return false;
    }
    /// Optional: click at position, button 0=left 1=right. Return true if consumed.
    virtual bool OnClick(glm::vec2 position, int button) { (void)position; (void)button; return false; }
};

} // namespace gui
