#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>                // std::sort

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/renderer/Material.hpp>

namespace ASCIIgL {

// =============================================================================
// HIGH-LEVEL DRAWING API - MESHES AND MODELS (Internal + queued)
// =============================================================================

// Internal immediate primitive used by the draw-call system.
void Renderer::DrawMesh(const Mesh* mesh) {
    if (!_initialized || !mesh) {
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
    _context->IASetVertexBuffers(0, 1, cache->vertexBuffer.GetAddressOf(), &stride, &offset);
    
    // Draw with or without indices
    if (cache->indexBuffer && cache->indexCount > 0) {
        _context->IASetIndexBuffer(cache->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        _context->DrawIndexed(static_cast<UINT>(cache->indexCount), 0, 0);
    } else {
        _context->Draw(static_cast<UINT>(cache->vertexCount), 0);
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
    if (!_initialized) {
        Logger::Error("[Renderer] BeginFrame called before initialization!");
        return;
    }

    // Clear draw queues
    _opaqueDraws.clear();
    _transparentDraws.clear();

    // Clear render target: background is sRGB 0-255; RTV is sRGB so clear expects linear 0-1
    glm::vec3 linearBg = PaletteUtil::sRGB255ToLinear1(GetBackgroundCol());
    float clear_color[4] = { linearBg.r, linearBg.g, linearBg.b, 1.0f };
    _context->ClearRenderTargetView(_renderTargetView.Get(), clear_color);

    // Clear depth stencil
    _context->ClearDepthStencilView(_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);  // Clear to 1.0 (far plane)

    // Bind render targets
    _context->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilView.Get());

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(Screen::GetInst().GetWidth());
    viewport.Height = static_cast<float>(Screen::GetInst().GetHeight());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    _context->RSSetViewports(1, &viewport);

    // Set depth stencil and blend state (on for both 3D and 2D)
    _context->OMSetDepthStencilState(_depthStencilState.Get(), 0);
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    _context->OMSetBlendState(_blendStateAlpha.Get(), blendFactor, 0xFFFFFFFF);

    // Set rasterizer state based on wireframe, backface culling, and CCW modes
    bool wireframe = GetWireframe();
    bool backfaceCulling = GetBackfaceCulling();
    bool ccw = GetCCW();
    
    // Calculate index: wireframe(0/1) + cull(0/2) + ccw(0/4)
    int stateIndex = (wireframe ? 1 : 0) + (backfaceCulling ? 2 : 0) + (ccw ? 4 : 0);
    _context->RSSetState(_rasterizerStates[stateIndex].Get());

    // Bind sampler
    _context->PSSetSamplers(0, 1, _samplerLinear.GetAddressOf());

    // Set primitive topology
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Renderer::SubmitDraw(const DrawCall& call) {
    if (!call.mesh || !call.material) return;
    if (call.transparent) {
        _transparentDraws.push_back(call);
    } else {
        _opaqueDraws.push_back(call);
    }
}

void Renderer::SortOpaqueDraws() {
    std::sort(_opaqueDraws.begin(), _opaqueDraws.end(),
              [](const DrawCall& a, const DrawCall& b) {
                  if (a.layer != b.layer) return a.layer < b.layer;
                  return a.material < b.material;
              });
}

void Renderer::SortTransparentDraws() {
    std::sort(_transparentDraws.begin(), _transparentDraws.end(),
              [](const DrawCall& a, const DrawCall& b) {
                  if (a.layer != b.layer) return a.layer < b.layer;
                  // Higher sortKey drawn later (back-to-front)
                  return a.sortKey > b.sortKey;
              });
}

void Renderer::ExecuteDrawList(const std::vector<DrawCall>& list) {
    Material* lastMat = nullptr;

    for (const auto& dc : list) {
        if (!dc.mesh || !dc.material) continue;

        Material* mat = dc.material;

        if (mat != lastMat) {
            lastMat = mat;
            BindMaterial(mat);
        }

        // Apply per-draw uniform overrides using pre-resolved descriptors.
        for (const auto& ov : dc.overrides) {
            if (!ov.desc) continue;
            mat->ApplyUniformOverride(*ov.desc, ov.value);
        }
        UploadMaterialConstants(mat);

        DrawMesh(dc.mesh);
    }
}

void Renderer::FlushDraws() {
    if (!_initialized) return;

    // Ensure we're drawing to the main RT (quantization reads from it after resolve)
    _context->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilView.Get());

    // 1) Opaque pass: depth write ON, blending OFF
    SetDepthTestEnabled(true);
    SetDepthWriteEnabled(true);
    SetBlendEnabled(false);

    SortOpaqueDraws();
    ExecuteDrawList(_opaqueDraws);

    // 2) Transparent pass: depth write OFF, blending ON
    SetDepthTestEnabled(true);
    SetDepthWriteEnabled(false);
    SetBlendEnabled(true);

    SortTransparentDraws();
    ExecuteDrawList(_transparentDraws);
}

void Renderer::EndGpuFrame() {
    if (!_initialized) return;

    // Resolve MSAA render target to non-MSAA texture for CPU download
    D3D11_TEXTURE2D_DESC rtDesc;
    _renderTarget->GetDesc(&rtDesc);
    
    if (rtDesc.SampleDesc.Count > 1) {
        // MSAA is enabled - resolve to non-MSAA texture
        _context->ResolveSubresource(
            _resolvedTexture.Get(),     // Destination (non-MSAA)
            0,                           // Dest subresource
            _renderTarget.Get(),         // Source (MSAA)
            0,                           // Source subresource
            DXGI_FORMAT_R8G8B8A8_UNORM  // Format
        );
    } else {
        // No MSAA - just copy render target to resolved texture
        _context->CopyResource(_resolvedTexture.Get(), _renderTarget.Get());
    }
    
    // Present for RenderDoc (does nothing if no swap chain)
    if (_debugSwapChain) {
        _debugSwapChain->Present(0, 0);
    }

    RunQuantizationPass();

    if (Screen::GetInst().IsRenderToTerminal()) {
        DownloadFramebuffer();
    } else {
        RunAsciiWindowPass();
    }
}


} // namespace ASCIIgL

