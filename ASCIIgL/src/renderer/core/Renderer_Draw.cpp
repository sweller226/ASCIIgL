#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>                // std::sort

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Model.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/Profiler.hpp>
#include <ASCIIgL/renderer/Material.hpp>

#include "renderer/core/RendererImpl.hpp"

namespace ASCIIgL {

// =============================================================================
// HIGH-LEVEL DRAWING API - MESHES AND MODELS (Internal + queued)
// =============================================================================

// Internal immediate primitive used by the draw-call system.
void Renderer::DrawMesh(const Mesh* mesh) {
    if (!impl_->_initialized || !mesh) {
        if (!mesh) {
            Logger::Error("DrawMesh: mesh is nullptr!");
        }
        return;
    }
    
    // Get or create GPU buffer cache for this mesh
    GPUMeshCache* cache = GetOrCreateMeshCache(mesh);
    if (!cache || !cache->vertexBuffer) return;
    
    // Bind cached vertex buffer with mesh's format stride
    UINT stride = mesh->GetVertFormat().GetStride();
    UINT offset = 0;
    impl_->_context->IASetVertexBuffers(0, 1, cache->vertexBuffer.GetAddressOf(), &stride, &offset);
    
    // Draw with or without indices
    if (cache->indexBuffer && cache->indexCount > 0) {
        impl_->_context->IASetIndexBuffer(cache->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        impl_->_context->DrawIndexed(static_cast<UINT>(cache->indexCount), 0, 0);
    } else {
        impl_->_context->Draw(static_cast<UINT>(cache->vertexCount), 0);
    }
}

// Public queued DrawModel: enqueue all meshes of a model as draw calls.
void Renderer::DrawModel(const Model& model,
                         Material* material,
                         const glm::mat4& mvp,
                         int layer,
                         bool transparent,
                         float sortKey) {
    if (!material || !IsInitialized()) return;

    // Resolve MVP uniform descriptor once per model draw.
    const UniformDescriptor* mvpDesc = material->GetUniformDescriptor("mvp");

    for (const auto& meshPtr : model.meshes) {
        if (!meshPtr) continue;

        DrawCall dc;
        dc.mesh        = meshPtr;
        dc.material    = material;
        dc.layer       = layer;
        dc.transparent = transparent;
        dc.sortKey     = sortKey;

        if (mvpDesc) {
            UniformOverride ov;
            ov.desc  = mvpDesc;
            ov.value = UniformValue(mvp);
            dc.overrides.push_back(std::move(ov));
        }

        SubmitDraw(dc);
    }
}

// =============================================================================
// DRAW CALL QUEUE API / GPU FRAME
// =============================================================================

void Renderer::BeginGpuFrame() {
    if (!impl_->_initialized) {
        Logger::Error("[Renderer] BeginFrame called before initialization!");
        return;
    }

    // Clear draw queues
    impl_->_opaqueDraws.clear();
    impl_->_transparentDraws.clear();

    // Clear render target: background is sRGB 0-255; RTV is sRGB so clear expects linear 0-1
    glm::vec3 linearBg = PaletteUtil::sRGB255ToLinear1(GetBackgroundCol());
    float clear_color[4] = { linearBg.r, linearBg.g, linearBg.b, 1.0f };
    impl_->_context->ClearRenderTargetView(impl_->_renderTargetView.Get(), clear_color);

    // Clear depth stencil
    impl_->_context->ClearDepthStencilView(impl_->_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);  // Clear to 1.0 (far plane)

    // Bind render targets
    impl_->_context->OMSetRenderTargets(1, impl_->_renderTargetView.GetAddressOf(), impl_->_depthStencilView.Get());

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(Screen::GetInst().GetWidth());
    viewport.Height = static_cast<float>(Screen::GetInst().GetHeight());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    impl_->_context->RSSetViewports(1, &viewport);

    // Set depth stencil and blend state (on for both 3D and 2D)
    impl_->_context->OMSetDepthStencilState(impl_->_depthStencilState.Get(), 0);
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    impl_->_context->OMSetBlendState(impl_->_blendStateAlpha.Get(), blendFactor, 0xFFFFFFFF);

    // Set rasterizer state based on wireframe, backface culling, and CCW modes
    bool wireframe = GetWireframe();
    bool backfaceCulling = GetBackfaceCulling();
    bool ccw = GetCCW();
    
    // Calculate index: wireframe(0/1) + cull(0/2) + ccw(0/4)
    int stateIndex = (wireframe ? 1 : 0) + (backfaceCulling ? 2 : 0) + (ccw ? 4 : 0);
    impl_->_context->RSSetState(impl_->_rasterizerStates[stateIndex].Get());

    // Bind sampler
    impl_->_context->PSSetSamplers(0, 1, impl_->_samplerLinear.GetAddressOf());

    // Set primitive topology
    impl_->_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Renderer::SubmitDraw(const DrawCall& call) {
    if (!call.mesh || !call.material) return;
    if (call.transparent) {
        impl_->_transparentDraws.push_back(call);
    } else {
        impl_->_opaqueDraws.push_back(call);
    }
}

void Renderer::SortOpaqueDraws() {
    std::sort(impl_->_opaqueDraws.begin(), impl_->_opaqueDraws.end(),
              [](const DrawCall& a, const DrawCall& b) {
                  if (a.layer != b.layer) return a.layer < b.layer;
                  return a.material < b.material;
              });
}

void Renderer::SortTransparentDraws() {
    std::sort(impl_->_transparentDraws.begin(), impl_->_transparentDraws.end(),
              [](const DrawCall& a, const DrawCall& b) {
                  if (a.layer != b.layer) return a.layer < b.layer;
                  // Higher sortKey drawn later (back-to-front)
                  return a.sortKey > b.sortKey;
              });
}

void Renderer::ExecuteDrawList(const std::vector<DrawCall>& list) {
    Material* lastMat = nullptr;
    bool currentCull = GetBackfaceCulling();

    for (const auto& dc : list) {
        if (!dc.mesh || !dc.material) continue;

        if (dc.backfaceCulling != currentCull) {
            currentCull = dc.backfaceCulling;
            const bool wireframe = GetWireframe();
            const bool ccw = GetCCW();
            const int stateIndex = (wireframe ? 1 : 0) + (currentCull ? 2 : 0) + (ccw ? 4 : 0);
            impl_->_context->RSSetState(impl_->_rasterizerStates[stateIndex].Get());
        }

        Material* mat = dc.material;

        if (mat != lastMat) {
            lastMat = mat;
            BindMaterial(mat);
        }

        for (const auto& ov : dc.overrides) {
            if (!ov.desc) continue;
            mat->ApplyUniformOverride(*ov.desc, ov.value);
        }

        UploadMaterialConstants(mat);

        DrawMesh(dc.mesh);
    }
}

void Renderer::FlushDraws() {
    if (!impl_->_initialized) return;

    // Ensure we're drawing to the main RT (quantization reads from it after resolve)
    impl_->_context->OMSetRenderTargets(1, impl_->_renderTargetView.GetAddressOf(), impl_->_depthStencilView.Get());

    // 1) Opaque pass: depth write ON, blending OFF
    SetDepthTestEnabled(true);
    SetDepthWriteEnabled(true);
    SetBlendEnabled(false);

    SortOpaqueDraws();
    ExecuteDrawList(impl_->_opaqueDraws);

    // 2) Transparent pass: depth write OFF, blending ON
    SetDepthTestEnabled(true);
    SetDepthWriteEnabled(false);
    SetBlendEnabled(true);

    SortTransparentDraws();
    ExecuteDrawList(impl_->_transparentDraws);
}

void Renderer::EndGpuFrame() {
    if (!impl_->_initialized) return;

    // Resolve MSAA render target to non-MSAA texture for CPU download
    D3D11_TEXTURE2D_DESC rtDesc;
    impl_->_renderTarget->GetDesc(&rtDesc);
    
    if (rtDesc.SampleDesc.Count > 1) {
        PROFILE_SCOPE("Renderer.EndGpuFrame.ResolveMSAA");
        // MSAA is enabled - resolve to non-MSAA texture (format must match source)
        impl_->_context->ResolveSubresource(
            impl_->_resolvedTexture.Get(),     // Destination (non-MSAA)
            0,                           // Dest subresource
            impl_->_renderTarget.Get(),         // Source (MSAA)
            0,                           // Source subresource
            rtDesc.Format                // Match render target (R8G8B8A8_UNORM_SRGB)
        );
    } else {
        PROFILE_SCOPE("Renderer.EndGpuFrame.CopyResolved");
        // No MSAA - just copy render target to resolved texture
        impl_->_context->CopyResource(impl_->_resolvedTexture.Get(), impl_->_renderTarget.Get());
    }
    
    // Present only in FastDebug for RenderDoc support.
#if defined(ASCIIGL_FASTDEBUG)
    if (impl_->_debugSwapChain) {
        PROFILE_SCOPE("Renderer.EndGpuFrame.DebugPresent");
        impl_->_debugSwapChain->Present(0, 0);
    }
#endif

    {
        PROFILE_SCOPE("Renderer.EndGpuFrame.QuantizationPass");
        RunQuantizationPass();
    }

    if (Screen::GetInst().IsRenderToTerminal()) {
        PROFILE_SCOPE("Renderer.EndGpuFrame.DownloadFramebuffer");
        DownloadFramebuffer();
    } else {
        PROFILE_SCOPE("Renderer.EndGpuFrame.AsciiWindowPass");
        RunAsciiWindowPass();
    }
}

void Renderer::ExecuteTransparentDrawList() {
    SortTransparentDraws();
    ExecuteDrawList(impl_->_transparentDraws);
}

void Renderer::ExecuteOpaqueDrawList() {
    SortOpaqueDraws();
    ExecuteDrawList(impl_->_opaqueDraws);
}

} // namespace ASCIIgL

