#include <ASCIIgL/renderer/Renderer.hpp>

#include "renderer/core/RendererImpl.hpp"

namespace ASCIIgL {

// =========================================================================
// Depth and Blend State Management
// =========================================================================

void Renderer::InvalidateBoundState() {
    if (!impl_) return;
    impl_->_boundState.valid = false;
}

void Renderer::ApplyDrawState(const Renderer::DrawGpuState& desired) {
    if (!impl_->_initialized) return;

    const bool mustBindAll = !impl_->_boundState.valid;
    const Renderer::DrawGpuState& current = impl_->_boundState.state;

    if (mustBindAll || desired.backfaceCulling != current.backfaceCulling) {
        const bool wireframe = GetWireframe();
        const bool ccw = GetCCW();
        const int stateIndex = (wireframe ? 1 : 0) + (desired.backfaceCulling ? 2 : 0) + (ccw ? 4 : 0);
        impl_->_context->RSSetState(impl_->_rasterizerStates[stateIndex].Get());
    }

    if (mustBindAll || desired.depthTest != current.depthTest || desired.depthWrite != current.depthWrite) {
        SetDepthState(desired.depthTest, desired.depthWrite);

        // Overlay draws must stamp near depth so depth-aware SSAA does not resolve
        // using scene geometry behind the HUD. Viewport MinDepth=MaxDepth=0 forces
        // written depth to 0 regardless of the mesh's clip-space Z.
        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(impl_->_renderTargetWidth);
        viewport.Height = static_cast<float>(impl_->_renderTargetHeight);
        if (desired.depthTest) {
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
        } else {
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 0.0f;
        }
        impl_->_context->RSSetViewports(1, &viewport);
    }

    if (mustBindAll || desired.blend != current.blend) {
        SetBlendEnabled(desired.blend);
    }

    impl_->_boundState.state = desired;
    impl_->_boundState.valid = true;
}

void Renderer::SetDepthState(bool testEnabled, bool writeEnabled) {
    if (!impl_->_initialized) return;

    ID3D11DepthStencilState* state = impl_->_depthStencilStateOverlayWrite.Get();
    if (testEnabled) {
        state = writeEnabled
            ? impl_->_depthStencilState.Get()
            : impl_->_depthStencilStateNoWrite.Get();
    }

    impl_->_context->OMSetDepthStencilState(state, 0);
}

void Renderer::SetBlendEnabled(bool enabled) {
    if (!impl_->_initialized) return;
    ID3D11BlendState* state = enabled ? impl_->_blendStateAlpha.Get() : impl_->_blendStateOpaque.Get();
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    impl_->_context->OMSetBlendState(state, blendFactor, 0xFFFFFFFF);
}

} // namespace ASCIIgL

