#pragma once

#include <memory>

namespace ASCIIgL { class Mesh; }

namespace ASCIICraft::gui {

/// Create a PosUV quad mesh for the GUI pipeline (panels, slot backgrounds).
/// Matches guiMaterial vertex layout. Texture is bound per-draw by screens.
std::shared_ptr<ASCIIgL::Mesh> CreateQuadMesh();

} // namespace ASCIICraft::gui
