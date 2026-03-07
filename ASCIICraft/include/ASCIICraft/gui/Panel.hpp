#pragma once

#include <ASCIICraft/gui/Widget.hpp>
#include <memory>
#include <vector>

namespace ASCIIgL { class Mesh; class Material; }
namespace ecs::systems { class RenderSystem; }

namespace gui {

/// Widget that draws a background quad and contains child widgets.
class Panel : public Widget {
public:
    void SetBackgroundMesh(std::shared_ptr<ASCIIgL::Mesh> mesh) { backgroundMesh = std::move(mesh); }
    /// Set the material for this panel's background. Material should have texture already set.
    void SetBackgroundMaterial(std::shared_ptr<ASCIIgL::Material> material) { backgroundMaterial = std::move(material); }

    void AddChild(std::unique_ptr<Widget> child);
    std::vector<std::unique_ptr<Widget>>& GetChildren() { return children; }
    const std::vector<std::unique_ptr<Widget>>& GetChildren() const { return children; }

    void Layout(glm::vec2 screenSize, const glm::vec2* parentTopLeft = nullptr) override;
    Widget* HitTest(glm::vec2 point) override;
    const Widget* HitTest(glm::vec2 point) const override;
    void Update(float dt) override;
    void Draw(::ecs::systems::RenderSystem& renderSystem) const override;

private:
    std::shared_ptr<ASCIIgL::Mesh> backgroundMesh;
    std::shared_ptr<ASCIIgL::Material> backgroundMaterial;  // Material with texture for this panel
    std::vector<std::unique_ptr<Widget>> children;
};

} // namespace gui
