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
    _opaqueDraws.clear();
    _transparentDraws.clear();
    BeginColBuffFrame();
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
    bool cullingDisabled = false; // track rasterizer state to minimize RSSetState calls

    for (const auto& dc : list) {
        if (!dc.mesh || !dc.material) continue;

        // Per-draw backface culling toggle.
        // Most draws use the global setting, but mobs like skeleton/chicken
        // need both-sided rendering.  We only call RSSetState when the flag
        // actually changes to avoid redundant D3D11 state switches.
        if (dc.disableBackfaceCulling != cullingDisabled) {
            cullingDisabled = dc.disableBackfaceCulling;
            bool wireframe = GetWireframe();
            bool backfaceCulling = cullingDisabled ? false : GetBackfaceCulling();
            bool ccw = GetCCW();
            int stateIndex = (wireframe ? 1 : 0) + (backfaceCulling ? 2 : 0) + (ccw ? 4 : 0);
            _context->RSSetState(_rasterizerStates[stateIndex].Get());
        }

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

    // Restore backface culling if it was disabled
    if (cullingDisabled) {
        bool wireframe = GetWireframe();
        bool backfaceCulling = GetBackfaceCulling();
        bool ccw = GetCCW();
        int stateIndex = (wireframe ? 1 : 0) + (backfaceCulling ? 2 : 0) + (ccw ? 4 : 0);
        _context->RSSetState(_rasterizerStates[stateIndex].Get());
    }
}

void Renderer::FlushDraws() {
    if (!_initialized) return;

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
    EndColBuffFrame();
}

} // namespace ASCIIgL

