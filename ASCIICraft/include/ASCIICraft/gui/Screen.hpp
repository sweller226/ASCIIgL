#pragma once

#include <ASCIICraft/gui/Panel.hpp>
#include <functional>
#include <memory>
#include <string>

namespace gui {

namespace ecs::systems { class RenderSystem; }

/// Root container for a full screen. Has blocksInput flag; when true, gameplay input is disabled.
class Screen : public Panel {
public:
    bool blocksInput = false;

    /// Called each frame for layout (screen size already set on manager).
    virtual void OnLayout(glm::vec2 screenSize) { (void)screenSize; }
    /// Called each frame for update. Default runs Panel::Update(dt) for the widget tree.
    virtual void OnUpdate(float dt);
    /// Called to collect draw items.
    virtual void OnDraw(::ecs::systems::RenderSystem& renderSystem) const;
    /// Optional: key press (action id). Return true if consumed.
    virtual bool OnKey(const std::string& actionId) { (void)actionId; return false; }
    /// Optional: cursor moved to screen position.
    virtual void OnCursorMove(glm::vec2 position) { (void)position; }
    /// Optional: click at position, button 0=left 1=right. Return true if consumed.
    virtual bool OnClick(glm::vec2 position, int button) { (void)position; (void)button; return false; }
};

} // namespace gui
