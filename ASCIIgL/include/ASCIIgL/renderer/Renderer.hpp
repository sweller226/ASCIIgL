#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/UniformLayout.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/renderer/SamplerType.hpp>
#include <ASCIIgL/renderer/screen/ScreenTypes.hpp>

#ifdef _WIN32
struct ID3D11Device;
struct ID3D11ShaderResourceView;
#endif

namespace ASCIIgL {

class Mesh;
class Model;
class Texture;
class TextureArray;
class Shader;
class ShaderProgram;
class Material;

class Renderer
{
public:
    // =========================================================================
    // Draw-call types
    // =========================================================================
    struct UniformOverride {
        const UniformDescriptor* desc = nullptr;  // layout metadata (offset/size/type)
        UniformValue             value;           // concrete value to write
    };

    struct DrawCall {
        const Mesh*   mesh            = nullptr;
        Material*     material        = nullptr;  // non-owning
        int           layer           = 0;
        bool          transparent     = false;    // false = opaque pass, true = transparent pass
        bool          backfaceCulling = true;     // per-draw cull state override
        bool          depthTest       = true;     // false = HUD/overlay: no occlusion test; writes near depth for SSAA
        float         sortKey         = 0.0f;     // used for transparent sorting (e.g. depth or layer)
        std::vector<UniformOverride> overrides;   // per-draw uniform overrides
    };

    /// Opaque GPU mesh cache; storage defined in engine implementation.
    struct GPUMeshCache;
    /// Implementation state (see src/renderer/RendererImpl.hpp).
    struct Impl;

    // =========================================================================
    // Singleton
    // =========================================================================
    static Renderer& GetInst() {
        static Renderer instance;
        return instance;
    }

    // =========================================================================
    // Initialization
    // =========================================================================
    /// supersample2x: when true, 3D renders at 2x resolution and box-filters down before quantization.
    /// charRamp: optional custom chars; nullptr/empty = use the "chars" array from coverage JSON.
    /// charRampCount: when using JSON chars, subsample to this many with evenly spaced coverage;
    /// <= 0 (default -1) keeps the full ramp. Ignored if charRamp is set.
    void Initialize(bool supersample2x = false,
                    const wchar_t* charRamp = nullptr,
                    int charRampCount = -1);
    bool IsInitialized() const;
    bool GetSupersample2x() const;

    // =========================================================================
    // GPU frame lifecycle
    // =========================================================================
    /// Clears internal draw queues and begins the GPU render pass for this frame.
    void BeginGpuFrame();
    /// Enqueue a draw call; actual GPU draws are issued during FlushDraws().
    void SubmitDraw(const DrawCall& call);
    /// Execute queued draws in two passes: opaque then transparent.
    void FlushDraws();
    /// End the GPU render pass for this frame (resolve + download).
    void EndGpuFrame();

    // =========================================================================
    // Queued drawing
    // =========================================================================
    /// Enqueue all meshes of a model as draw calls using the given material and MVP.
    void DrawModel(const Model& model,
                   Material* material,
                   const glm::mat4& mvp,
                   int layer,
                   bool transparent,
                   float sortKey = 0.0f);

    // =========================================================================
    // Low-level pixel-buffer drawing (no GPU pipeline)
    // =========================================================================
    void DrawLinePxBuff(int x1, int y1, int x2, int y2, wchar_t pixel_type, unsigned short col);
    void DrawTriangleWireframePxBuff(const glm::vec2& vert1, const glm::vec2& vert2,
                                   const glm::vec2& vert3, wchar_t pixel_type, unsigned short col);
    void DrawScreenBorderPxBuff(unsigned short col);

    // =========================================================================
    // Render settings
    // =========================================================================
    void SetWireframe(bool wireframe);
    bool GetWireframe() const;

    void SetBackfaceCulling(bool backfaceCulling);
    bool GetBackfaceCulling() const;

    void SetCCW(bool ccw);
    bool GetCCW() const;

    glm::ivec3 GetBackgroundCol() const;
    void SetBackgroundCol(const glm::ivec3& color);

    /// Enables blue-noise ordered dithering for LUT quantization. Default: false.
    void SetDitheringEnabled(bool enabled);
    bool GetDitheringEnabled() const;

    /// Anisotropic filtering level for texture arrays. Valid: 1 (off), 2, 4, 8, 16. Default 16.
    void SetMaxAnisotropy(int level);
    int GetMaxAnisotropy() const;

    // =========================================================================
    // Utility
    // =========================================================================
    ScreenPixel GetCharInfo(const glm::ivec3& rgb);

    // =========================================================================
    // GPU resource helpers
    // =========================================================================
    /// Called by resource destructors.
    void ReleaseMeshCache(void* cachePtr);
    void InvalidateCachedTexture(const Texture* tex);
    /// Returns nullptr if using the default shader program.
    ShaderProgram* GetBoundShaderProgram() const;

private:
    friend class Shader;
    friend class ShaderProgram;
    friend class Material;

    struct DrawGpuState {
        bool backfaceCulling = true;
        bool depthTest       = true;
        bool depthWrite      = true;
        bool blend           = false;
    };

    struct BoundState {
        bool valid = false;
        DrawGpuState state{};
    };

    static constexpr size_t INITIAL_VERTEX_BUFFER_CAPACITY = 10000;
    static constexpr size_t BUFFER_GROWTH_FACTOR = 2;

    std::unique_ptr<Impl> impl_;

    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Shutdown();

    // -------------------------------------------------------------------------
    // Device and render-target setup
    // -------------------------------------------------------------------------
    bool InitializeDevice();
    bool InitializeRenderTarget();
    bool InitializeDepthStencil();
    bool InitializeSamplers();
    bool CreateAnisotropicSampler();  // (re)creates anisotropic sampler using _maxAnisotropy
    bool InitializeRasterizerStates();
    bool InitializeBlendStates();
    bool InitializeStagingTexture();
    bool InitializeDebugSwapChain();   // RenderDoc support
    bool InitializeWindowSwapChain();  // windowed ASCII output
    bool InitializeAsciiWindowPass();  // shaders, CB, palette SRV for window pass
    bool InitializeFontAtlas();
    bool InitializeCharInfoTarget();
    bool InitializeQuantizationShaders();
    bool InitializeBlueNoiseTexture();
    bool InitializeDownsampleShader();
    void RunDownsamplePass();

#ifdef _WIN32
    /// D3D device for shader compilation; nullptr if not initialized.
    ID3D11Device* GetD3D11Device() const;
#endif

    // -------------------------------------------------------------------------
    // ASCII quantization and LUT
    // -------------------------------------------------------------------------
    bool LoadCharCoverageFromJson(const wchar_t* charRamp = nullptr, int charRampCount = -1);
    void PrecomputeColorLUT();
    void PrecomputeMonochromeColorLUT(Palette& palette);
    void PrecomputeMultiColorLUT(Palette& palette);
    size_t MonochromeLuminanceToIndex(float L) const;
    void UploadLUTsToGPU();
    bool EnsureQuantizationResources();
    void RunQuantizationPass();
    void DownloadFramebuffer();
    void RunAsciiWindowPass();

    // -------------------------------------------------------------------------
    // Draw-call execution
    // -------------------------------------------------------------------------
    GPUMeshCache* GetOrCreateMeshCache(const Mesh* mesh);
    void DrawMesh(const Mesh* mesh);
    void SortOpaqueDraws();
    void SortTransparentDraws();
    void ExecuteDrawList(const std::vector<DrawCall>& list, const DrawGpuState& passState);
    void ApplyDrawState(const DrawGpuState& desired);
    void InvalidateBoundState();

    void DrawClippedLinePxBuff(int x0, int y0, int x1, int y1,
                               int minX, int maxX, int minY, int maxY,
                               wchar_t pixel_type, unsigned short col);

    // -------------------------------------------------------------------------
    // Materials, shaders, and textures
    // -------------------------------------------------------------------------
    void BindShaderProgram(ShaderProgram* program);
    void UnbindShaderProgram();
    void BindMaterial(Material* material);
    void UploadMaterialConstants(Material* material);

    bool CreateTextureFromASCIIgLTexture(const Texture* tex, ID3D11ShaderResourceView** srv);
    bool CreateTextureArraySRV(const TextureArray* texArray, ID3D11ShaderResourceView** srv);
    void BindTexture(const Texture* tex, int slot = 0, SamplerType type = SamplerType::Default);
    void BindTextureArray(const TextureArray* texArray, int slot = 0, SamplerType type = SamplerType::Default);
    void UnbindTexture(int slot = 0);
    void UnbindTextureArray(int slot = 0);

    void SetDepthState(bool testEnabled, bool writeEnabled);
    void SetBlendEnabled(bool enabled);
};

}  // namespace ASCIIgL
