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

// Forward declarations
class Mesh;
class Model;
class Texture;
class TextureArray;
class Shader;
class ShaderProgram;
class Material;

class Renderer
{
    friend class Shader;
    friend class ShaderProgram;
    friend class Material;

public:
    // =========================================================================
    // Draw Call Queue Types
    // =========================================================================
    struct UniformOverride {
        const UniformDescriptor* desc = nullptr;  // layout metadata (offset/size/type)
        UniformValue             value;          // concrete value to write
    };

    struct DrawCall {
        const Mesh*   mesh        = nullptr;
        Material*     material    = nullptr;   // non-owning
        int           layer       = 0;
        bool          transparent = false;     // false = opaque pass, true = transparent pass
        bool          backfaceCulling = true;  // per-draw cull state override
        bool          depthTest = true;        // false = HUD/overlay draws that must not be depth-occluded
        float         sortKey     = 0.0f;      // used for transparent sorting (e.g. depth or layer)
        std::vector<UniformOverride> overrides; // per-draw uniform overrides
    };

    /// Opaque GPU mesh cache; storage defined in engine implementation.
    struct GPUMeshCache;
    /// Implementation state (see src/renderer/RendererImpl.hpp).
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;

    // =========================================================================
    // Buffer Configuration Constants
    // =========================================================================
    static constexpr size_t INITIAL_VERTEX_BUFFER_CAPACITY = 10000;
    static constexpr size_t BUFFER_GROWTH_FACTOR = 2;

    // =========================================================================
    // Singleton and Construction
    // =========================================================================
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void RunQuantizationPass();
    void UploadLUTsToGPU();
    bool EnsureQuantizationResources();
    bool InitializeCharInfoTarget();
    bool InitializeQuantizationShaders();

    bool InitializeFontAtlas();

    GPUMeshCache* GetOrCreateMeshCache(const Mesh* mesh);

    void LoadCharCoverageFromJson(const wchar_t* charRamp = nullptr, int charRampCount = 10);
    void PrecomputeColorLUT();
    void PrecomputeMonochromeColorLUT(Palette& palette);
    void PrecomputeMultiColorLUT(Palette& palette);
    size_t MonochromeLuminanceToIndex(float L) const;

    void DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, wchar_t pixel_type, unsigned short col);

    // =========================================================================
    // GPU Initialization Methods
    // =========================================================================
    bool InitializeDevice();
    bool InitializeRenderTarget();
    bool InitializeDepthStencil();
    bool InitializeSamplers();
    bool CreateAnisotropicSampler();  // (re)creates anisotropic sampler using _maxAnisotropy
    bool InitializeRasterizerStates();
    bool InitializeBlendStates();
    bool InitializeStagingTexture();
    bool InitializeDebugSwapChain();  // For RenderDoc support
    bool InitializeWindowSwapChain(); // For windowed ASCII output
    bool InitializeAsciiWindowPass(); // Compile shaders, create CB and palette SRV for window pass

    // =========================================================================
    // Texture Management (Internal)
    // =========================================================================
    bool CreateTextureFromASCIIgLTexture(const Texture* tex, ID3D11ShaderResourceView** srv);
    bool CreateTextureArraySRV(const TextureArray* texArray, ID3D11ShaderResourceView** srv);

    // =========================================================================
    // Cleanup
    // =========================================================================
    void Shutdown();

    // =========================================================================
    // Frame Management (Internal)
    // =========================================================================
    void DownloadFramebuffer();
    void RunAsciiWindowPass();  // GPU ASCII->window pass (window mode)

    // Immediate GPU draw primitive, used internally by the draw-call system.
    void DrawMesh(const Mesh* mesh);

    // =========================================================================
    // Draw Call Queues
    // =========================================================================
    void SortOpaqueDraws();
    void SortTransparentDraws();
    void ExecuteDrawList(const std::vector<DrawCall>& list);

#ifdef _WIN32
    /// D3D device for shader compilation; nullptr if not initialized.
    ID3D11Device* GetD3D11Device() const;
#endif

public:
    // =========================================================================
    // Singleton Access
    // =========================================================================
    static Renderer& GetInst() {
        static Renderer instance;
        return instance;
    }

    // =========================================================================
    // Initialization and Core API
    // =========================================================================
    /// charRamp: custom chars for coverage; nullptr or empty = use default ramp.
    /// charRampCount: when using default ramp, subsample to this many chars with evenly spaced coverage (default 10); ignored if charRamp is set.
    void Initialize(bool antialiasing = false, int antialiasing_samples = 4, const wchar_t* charRamp = nullptr, int charRampCount = 10);
    bool IsInitialized() const;

    // =========================================================================
    // Drawing API (queued)
    // =========================================================================
    // Enqueue all meshes of a model as draw calls using the given material and MVP.
    void DrawModel(const Model& model,
                   Material* material,
                   const glm::mat4& mvp,
                   int layer,
                   bool transparent,
                   float sortKey = 0.0f);

    // =========================================================================
    // Low-Level Drawing API - Primitives, No pipeline involved
    // =========================================================================
    void DrawLinePxBuff(int x1, int y1, int x2, int y2, wchar_t pixel_type, const unsigned short col);

    void DrawTriangleWireframePxBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, wchar_t pixel_type, const unsigned short col);

    void DrawScreenBorderPxBuff(const unsigned short col);

    // =========================================================================
    // Utility
    // =========================================================================
    ScreenPixel GetCharInfo(const glm::ivec3& rgb);

    // =========================================================================
    // Settings API
    // =========================================================================
    int GetAntialiasingsamples() const;

    void SetWireframe(const bool wireframe);
    bool GetWireframe() const;

    void SetBackfaceCulling(const bool backfaceCulling);
    bool GetBackfaceCulling() const;

    void SetCCW(const bool ccw);
    bool GetCCW() const;

    bool GetAntialiasing() const;

    glm::ivec3 GetBackgroundCol() const;
    void SetBackgroundCol(const glm::ivec3& color);

    /// Enables 4x4 Bayer ordered dithering for the monochrome LUT path.
    /// Default: false.
    void SetDitheringEnabled(bool enabled);
    bool GetDitheringEnabled() const;

    /// Anisotropic filtering level for texture arrays. Valid: 1 (off), 2, 4, 8, 16. Default 16.
    void SetMaxAnisotropy(int level);
    int GetMaxAnisotropy() const;

    // =========================================================================
    // GPU Public API
    // =========================================================================
    
    // Resource cache cleanup (called by resource destructors)
    void ReleaseMeshCache(void* cachePtr);
    void InvalidateCachedTexture(const Texture* tex);
    
    // Get currently bound shader program (nullptr if using default)
    ShaderProgram* GetBoundShaderProgram() const;

    // =========================================================================
    // Draw Call Queue API / GPU Frame
    // =========================================================================
    // Clears internal draw queues and begins the GPU render pass for this frame.
    void BeginGpuFrame();
    // Enqueue a draw call; actual GPU draws are issued during FlushDraws().
    void SubmitDraw(const DrawCall& call);
    // Execute queued draws in two passes: opaque then transparent.
    void FlushDraws();
    // End the GPU render pass for this frame (resolve + download).
    void EndGpuFrame();

    void ExecuteTransparentDrawList();
    void ExecuteOpaqueDrawList();

private:
    // Bind a custom shader program (nullptr to use default)
    void BindShaderProgram(ShaderProgram* program);
    
    // Bind a material (sets shader + uniforms + textures)
    void BindMaterial(Material* material);
    
    // Unbind custom shader (reverts to default)
    void UnbindShaderProgram();

    // Helper: Upload material's constant buffer
    void UploadMaterialConstants(Material* material);

    // Texture Management (internal)
    void BindTexture(const Texture* tex, int slot = 0, SamplerType type = SamplerType::Default);
    void BindTextureArray(const TextureArray* texArray, int slot = 0, SamplerType type = SamplerType::Default);
    
    // Unbind any texture
    void UnbindTexture(int slot = 0);
    void UnbindTextureArray(int slot = 0);
    
    /// Set depth test on/off. Use false for 2D overlay (GUI) so it is not clipped by 3D depth buffer.
    void SetDepthTestEnabled(bool enabled);
    
    /// Set depth write on/off. Use false for transparent/2D so they don't occlude geometry behind.
    void SetDepthWriteEnabled(bool enabled);
    
    /// Set alpha blending on/off. Use true for 2D GUI so transparent PNG regions blend correctly.
    void SetBlendEnabled(bool enabled);
};

}  // namespace ASCIIgL
