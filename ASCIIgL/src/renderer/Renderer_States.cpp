#include <ASCIIgL/renderer/Renderer.hpp>

namespace ASCIIgL {

// =========================================================================
// Depth and Blend State Management
// =========================================================================

void Renderer::SetDepthTestEnabled(bool enabled) {
    if (!_initialized) return;
    ID3D11DepthStencilState* state = enabled ? _depthStencilState.Get() : _depthStencilStateNoTest.Get();
    _context->OMSetDepthStencilState(state, 0);
}

void Renderer::SetDepthWriteEnabled(bool enabled) {
    if (!_initialized) return;
    ID3D11DepthStencilState* state = enabled ? _depthStencilState.Get() : _depthStencilStateNoWrite.Get();
    _context->OMSetDepthStencilState(state, 0);
}

void Renderer::SetBlendEnabled(bool enabled) {
    if (!_initialized) return;
    ID3D11BlendState* state = enabled ? _blendStateAlpha.Get() : _blendStateOpaque.Get();
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    _context->OMSetBlendState(state, blendFactor, 0xFFFFFFFF);
}

} // namespace ASCIIgL

