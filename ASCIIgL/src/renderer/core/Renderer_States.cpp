#include <ASCIIgL/renderer/Renderer.hpp>

#include "renderer/core/RendererImpl.hpp"

namespace ASCIIgL {

// =========================================================================
// Depth and Blend State Management
// =========================================================================

void Renderer::SetDepthTestEnabled(bool enabled) {
    if (!impl_->_initialized) return;
    ID3D11DepthStencilState* state = enabled ? impl_->_depthStencilState.Get() : impl_->_depthStencilStateNoTest.Get();
    impl_->_context->OMSetDepthStencilState(state, 0);
}

void Renderer::SetDepthWriteEnabled(bool enabled) {
    if (!impl_->_initialized) return;
    ID3D11DepthStencilState* state = enabled ? impl_->_depthStencilState.Get() : impl_->_depthStencilStateNoWrite.Get();
    impl_->_context->OMSetDepthStencilState(state, 0);
}

void Renderer::SetBlendEnabled(bool enabled) {
    if (!impl_->_initialized) return;
    ID3D11BlendState* state = enabled ? impl_->_blendStateAlpha.Get() : impl_->_blendStateOpaque.Get();
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    impl_->_context->OMSetBlendState(state, blendFactor, 0xFFFFFFFF);
}

} // namespace ASCIIgL

