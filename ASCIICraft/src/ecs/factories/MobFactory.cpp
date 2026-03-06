#include <ASCIICraft/ecs/factories/MobFactory.hpp>
#include <ASCIICraft/ecs/components/MobComponents.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Renderable.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/util/Logger.hpp>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cfloat>

// ===========================================================================
// MobMeshBuilder — reusable helper that constructs MC 1.7.10 box-model meshes
// using the same vertex / UV / winding conventions that were verified for the
// pig model (CCW front-face, no V-flip, MC Y-down -> engine Y-up).
//
// Rotation order matches MC ModelRenderer.render():
//   glRotate(Z) -> glRotate(Y) -> glRotate(X)
//   i.e. vertex is transformed as  Rz * Ry * Rx * v,
//   so in code we apply X first, then Y, then Z.
// ===========================================================================
namespace {

struct MobMeshBuilder {
    struct FVert { int ci; glm::vec2 uv; };

    std::vector<std::byte> vertices;
    std::vector<int>       indices;
    float texW, texH;

    static constexpr float SCALE = 1.0f / 16.0f;

    MobMeshBuilder(float tw, float th) : texW(tw), texH(th) {}

    // Append one PosUV vertex
    void appendVert(const glm::vec3& pos, const glm::vec2& uv) {
        ASCIIgL::VertStructs::PosUV v;
        v.SetXYZ(pos);
        v.SetUV(uv);
        const auto* bytes = reinterpret_cast<const std::byte*>(&v);
        vertices.insert(vertices.end(), bytes, bytes + sizeof(v));
    }

    // -----------------------------------------------------------------
    // addBox — emit one MC ModelBox (6 faces, 24 verts, 36 indices).
    //
    //   texU, texV   : pixel offset into the entity atlas
    //   lx,ly,lz     : local box origin (first three args of addBox)
    //   w, h, d      : box dimensions (ints, pixels)
    //   rpX,rpY,rpZ  : ModelRenderer rotation point
    //   rotX/Y/Z     : baked rotation in radians
    // -----------------------------------------------------------------
    void addBox(int texU, int texV,
                float lx, float ly, float lz,
                int w, int h, int d,
                float rpX, float rpY, float rpZ,
                float rotXRad = 0.0f,
                float rotYRad = 0.0f,
                float rotZRad = 0.0f)
    {
        // 1. Eight corners in MC local space
        float x2 = lx + w, y2 = ly + h, z2 = lz + d;
        glm::vec3 c[8] = {
            {lx, ly, lz}, {x2, ly, lz}, {x2, y2, lz}, {lx, y2, lz},
            {lx, ly, z2}, {x2, ly, z2}, {x2, y2, z2}, {lx, y2, z2},
        };

        // 2. Apply rotations: X first, then Y, then Z (MC render order)
        if (std::abs(rotXRad) > 0.001f) {
            float co = std::cos(rotXRad), si = std::sin(rotXRad);
            for (auto& p : c) {
                float ny = p.y * co - p.z * si;
                float nz = p.y * si + p.z * co;
                p.y = ny; p.z = nz;
            }
        }
        if (std::abs(rotYRad) > 0.001f) {
            float co = std::cos(rotYRad), si = std::sin(rotYRad);
            for (auto& p : c) {
                float nx = p.x * co + p.z * si;
                float nz = -p.x * si + p.z * co;
                p.x = nx; p.z = nz;
            }
        }
        if (std::abs(rotZRad) > 0.001f) {
            float co = std::cos(rotZRad), si = std::sin(rotZRad);
            for (auto& p : c) {
                float nx = p.x * co - p.y * si;
                float ny = p.x * si + p.y * co;
                p.x = nx; p.y = ny;
            }
        }

        // 3. Add rotation-point offset, negate Y, scale to world units
        for (auto& p : c) {
            p.x = (p.x + rpX) * SCALE;
            p.y = -(p.y + rpY) * SCALE;   // MC Y-down -> engine Y-up
            p.z = (p.z + rpZ) * SCALE;
        }

        // 4. Per-face UV coordinates (traced from MC TexturedQuad / ModelBox)
        float u  = (float)texU, v  = (float)texV;
        float fw = (float)w,    fh = (float)h,    fd = (float)d;

        // Top face (engine Y+, MC small-Y)
        FVert topF[4] = {
            {0, {(u+fd)      /texW, (v+fd)/texH}},
            {4, {(u+fd)      /texW,  v    /texH}},
            {5, {(u+fd+fw)   /texW,  v    /texH}},
            {1, {(u+fd+fw)   /texW, (v+fd)/texH}},
        };
        // Bottom face (engine Y-, MC large-Y)
        FVert botF[4] = {
            {7, {(u+fd+fw)      /texW,  v        /texH}},
            {3, {(u+fd+fw)      /texW, (v+fd)    /texH}},
            {2, {(u+fd+fw+fw)   /texW, (v+fd)    /texH}},
            {6, {(u+fd+fw+fw)   /texW,  v        /texH}},
        };
        // North face Z+ (MC back)
        FVert northF[4] = {
            {7, {(u+fd+fw+fd+fw)/texW, (v+fd+fh)/texH}},
            {6, {(u+fd+fw+fd)   /texW, (v+fd+fh)/texH}},
            {5, {(u+fd+fw+fd)   /texW, (v+fd)   /texH}},
            {4, {(u+fd+fw+fd+fw)/texW, (v+fd)   /texH}},
        };
        // South face Z- (MC front)
        FVert southF[4] = {
            {2, {(u+fd+fw)/texW, (v+fd+fh)/texH}},
            {3, {(u+fd)   /texW, (v+fd+fh)/texH}},
            {0, {(u+fd)   /texW, (v+fd)   /texH}},
            {1, {(u+fd+fw)/texW, (v+fd)   /texH}},
        };
        // East face X+ (MC right)
        FVert eastF[4] = {
            {6, {(u+fd+fw+fd)/texW, (v+fd+fh)/texH}},
            {2, {(u+fd+fw)   /texW, (v+fd+fh)/texH}},
            {1, {(u+fd+fw)   /texW, (v+fd)   /texH}},
            {5, {(u+fd+fw+fd)/texW, (v+fd)   /texH}},
        };
        // West face X- (MC left)
        FVert westF[4] = {
            {3, {(u+fd)  /texW, (v+fd+fh)/texH}},
            {7, { u      /texW, (v+fd+fh)/texH}},
            {4, { u      /texW, (v+fd)   /texH}},
            {0, {(u+fd)  /texW, (v+fd)   /texH}},
        };

        // 5. Emit vertices and CCW indices for all 6 faces
        FVert* faces[6] = { topF, botF, northF, southF, eastF, westF };
        for (int f = 0; f < 6; ++f) {
            int base = (int)(vertices.size() / sizeof(ASCIIgL::VertStructs::PosUV));
            for (int vi = 0; vi < 4; ++vi)
                appendVert(c[faces[f][vi].ci], faces[f][vi].uv);
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
    }

    // Shift model up so lowest Y vertex sits at Y = 0, then build the Mesh
    std::shared_ptr<ASCIIgL::Mesh> finalize(ASCIIgL::Texture* tex) {
        size_t count = vertices.size() / sizeof(ASCIIgL::VertStructs::PosUV);
        float minY = FLT_MAX;
        for (size_t i = 0; i < count; ++i) {
            auto* pv = reinterpret_cast<ASCIIgL::VertStructs::PosUV*>(
                &vertices[i * sizeof(ASCIIgL::VertStructs::PosUV)]);
            if (pv->Y() < minY) minY = pv->Y();
        }
        for (size_t i = 0; i < count; ++i) {
            auto* pv = reinterpret_cast<ASCIIgL::VertStructs::PosUV*>(
                &vertices[i * sizeof(ASCIIgL::VertStructs::PosUV)]);
            glm::vec3 pos = pv->GetXYZ();
            pos.y -= minY;
            pv->SetXYZ(pos);
        }

        ASCIIgL::Logger::Debug("MobMeshBuilder: " + std::to_string(count)
            + " verts, " + std::to_string(indices.size()) + " indices");

        return std::make_shared<ASCIIgL::Mesh>(
            std::move(vertices), ASCIIgL::VertFormats::PosUV(),
            std::move(indices), tex);
    }
};

} // anonymous namespace

// ===========================================================================

namespace ecs::factories {

MobFactory::MobFactory(entt::registry& registry)
    : m_registry(registry) {
}

MobFactory::~MobFactory() {
    for (auto e : m_active) {
        if (m_registry.valid(e)) m_registry.destroy(e);
    }
    m_active.clear();
}

// ===========================================================================
// init — build meshes for every mob type.
//
//   Each mob's box definitions are taken verbatim from MCP 908 (MC 1.7.10)
//   model classes.  Coordinates are in MC model-space pixels; the builder
//   converts them to engine world units (1 block = 1.0).
//
//   Quadruped body parts that have rotateAngleX = PI/2 applied at render
//   time are baked with that rotation here.  Biped arms with zombie
//   -PI/2 forward pose are likewise baked.
// ===========================================================================
void MobFactory::init() {
    const float PI = 3.14159265358979f;

    // Helper: load texture, build mesh, register in maps
    auto registerMob = [&](uint32_t typeId, const char* texPath,
                           float tw, float th, auto&& buildFn)
    {
        try {
            auto tex = std::make_shared<ASCIIgL::Texture>(texPath);
            m_textures[typeId] = tex;
            MobMeshBuilder b(tw, th);
            buildFn(b);
            m_meshes[typeId] = b.finalize(tex.get());
            ASCIIgL::Logger::Debug("MobFactory: type "
                + std::to_string(typeId) + " mesh OK");
        } catch (const std::exception& e) {
            ASCIIgL::Logger::Error("MobFactory: type "
                + std::to_string(typeId) + ": " + e.what());
        }
    };

    // =================================================================
    //  1 — PIG  (ModelPig / ModelQuadruped, legHeight = 6)
    //      Texture: 64 x 32
    // =================================================================
    registerMob(1, "res/textures/mobs/pig.png", 64, 32,
        [&](MobMeshBuilder& b)
    {
        // Head: tex(0,0), box(-4,-4,-8, 8,8,8), RP(0,12,-6)
        b.addBox( 0,  0,  -4.f,-4.f,-8.f, 8,8,8,   0.f,12.f,-6.f);
        // Snout (child of head, same RP): tex(16,16), box(-2,0,-9, 4,3,1)
        b.addBox(16, 16,  -2.f, 0.f,-9.f, 4,3,1,   0.f,12.f,-6.f);
        // Body: tex(28,8), box(-5,-10,-7, 10,16,8), RP(0,11,2), rotX=PI/2
        b.addBox(28,  8,  -5.f,-10.f,-7.f, 10,16,8, 0.f,11.f, 2.f, PI/2);
        // Legs (tex 0,16, box -2,0,-2, 4,6,4)
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,6,4,  -3.f,18.f, 7.f);  // back-left
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,6,4,   3.f,18.f, 7.f);  // back-right
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,6,4,  -3.f,18.f,-5.f);  // front-left
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,6,4,   3.f,18.f,-5.f);  // front-right
    });

    // =================================================================
    //  2 — CHICKEN  (ModelChicken)
    //      Texture: 64 x 32
    // =================================================================
    registerMob(2, "res/textures/mobs/chicken.png", 64, 32,
        [&](MobMeshBuilder& b)
    {
        // Head: tex(0,0), box(-2,-6,-2, 4,6,3), RP(0,15,-4)
        b.addBox( 0,  0,  -2.f,-6.f,-2.f, 4,6,3,   0.f,15.f,-4.f);
        // Bill: tex(14,0), box(-2,-4,-4, 4,2,2), RP(0,15,-4)
        b.addBox(14,  0,  -2.f,-4.f,-4.f, 4,2,2,   0.f,15.f,-4.f);
        // Chin: tex(14,4), box(-1,-2,-3, 2,2,2), RP(0,15,-4)
        b.addBox(14,  4,  -1.f,-2.f,-3.f, 2,2,2,   0.f,15.f,-4.f);
        // Body: tex(0,9), box(-3,-4,-3, 6,8,6), RP(0,16,0), rotX=PI/2
        b.addBox( 0,  9,  -3.f,-4.f,-3.f, 6,8,6,   0.f,16.f, 0.f, PI/2);
        // Right leg: tex(26,0), box(-1,0,-3, 3,5,3), RP(-2,19,1)
        b.addBox(26,  0,  -1.f, 0.f,-3.f, 3,5,3,  -2.f,19.f, 1.f);
        // Left leg: tex(26,0), box(-1,0,-3, 3,5,3), RP(1,19,1)
        b.addBox(26,  0,  -1.f, 0.f,-3.f, 3,5,3,   1.f,19.f, 1.f);
        // Right wing: tex(24,13), box(0,0,-3, 1,4,6), RP(-4,13,0)
        b.addBox(24, 13,   0.f, 0.f,-3.f, 1,4,6,  -4.f,13.f, 0.f);
        // Left wing: tex(24,13), box(-1,0,-3, 1,4,6), RP(4,13,0)
        b.addBox(24, 13,  -1.f, 0.f,-3.f, 1,4,6,   4.f,13.f, 0.f);
    });

    // =================================================================
    //  3 — COW  (ModelCow extends ModelQuadruped, legHeight = 12)
    //      Texture: 64 x 32
    // =================================================================
    registerMob(3, "res/textures/mobs/cow.png", 64, 32,
        [&](MobMeshBuilder& b)
    {
        // Head: tex(0,0), box(-4,-4,-6, 8,8,6), RP(0,4,-8)
        b.addBox( 0,  0,  -4.f,-4.f,-6.f, 8,8,6,   0.f, 4.f,-8.f);
        // Horn left (child of head): tex(22,0), box(-5,-5,-4, 1,3,1)
        b.addBox(22,  0,  -5.f,-5.f,-4.f, 1,3,1,   0.f, 4.f,-8.f);
        // Horn right (child of head): tex(22,0), box(4,-5,-4, 1,3,1)
        b.addBox(22,  0,   4.f,-5.f,-4.f, 1,3,1,   0.f, 4.f,-8.f);
        // Body: tex(18,4), box(-6,-10,-7, 12,18,10), RP(0,5,2), rotX=PI/2
        b.addBox(18,  4,  -6.f,-10.f,-7.f, 12,18,10, 0.f, 5.f, 2.f, PI/2);
        // Udder (child of body, same RP+rot): tex(52,0), box(-2,2,-8, 4,6,1)
        b.addBox(52,  0,  -2.f, 2.f,-8.f, 4,6,1,   0.f, 5.f, 2.f, PI/2);
        // Legs (tex 0,16, box -2,0,-2, 4,12,4)
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4, -4.f,12.f, 7.f);  // back-left
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4,  4.f,12.f, 7.f);  // back-right
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4, -4.f,12.f,-6.f);  // front-left
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4,  4.f,12.f,-6.f);  // front-right
    });

    // =================================================================
    //  4 — CREEPER  (ModelCreeper — upright, NO body rotation)
    //      Texture: 64 x 32
    // =================================================================
    registerMob(4, "res/textures/mobs/creeper.png", 64, 32,
        [&](MobMeshBuilder& b)
    {
        // Head: tex(0,0), box(-4,-8,-4, 8,8,8), RP(0,4,0)
        b.addBox( 0,  0,  -4.f,-8.f,-4.f, 8,8,8,   0.f, 4.f, 0.f);
        // Body: tex(16,16), box(-4,0,-2, 8,12,4), RP(0,4,0)
        b.addBox(16, 16,  -4.f, 0.f,-2.f, 8,12,4,  0.f, 4.f, 0.f);
        // Legs (tex 0,16, box -2,0,-2, 4,6,4)
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,6,4,  -2.f,16.f, 4.f);  // back-left
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,6,4,   2.f,16.f, 4.f);  // back-right
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,6,4,  -2.f,16.f,-4.f);  // front-left
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,6,4,   2.f,16.f,-4.f);  // front-right
    });

    // =================================================================
    //  5 — SHEEP  (ModelSheep2 / ModelQuadruped, legHeight = 12)
    //      Texture: 64 x 32
    // =================================================================
    registerMob(5, "res/textures/mobs/sheep.png", 64, 32,
        [&](MobMeshBuilder& b)
    {
        // Head: tex(0,0), box(-3,-4,-6, 6,6,8), RP(0,6,-8)
        b.addBox( 0,  0,  -3.f,-4.f,-6.f, 6,6,8,   0.f, 6.f,-8.f);
        // Body: tex(28,8), box(-4,-10,-7, 8,16,6), RP(0,5,2), rotX=PI/2
        b.addBox(28,  8,  -4.f,-10.f,-7.f, 8,16,6,  0.f, 5.f, 2.f, PI/2);
        // Legs (tex 0,16, box -2,0,-2, 4,12,4)
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4, -3.f,12.f, 7.f);  // back-left
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4,  3.f,12.f, 7.f);  // back-right
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4, -3.f,12.f,-5.f);  // front-left
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4,  3.f,12.f,-5.f);  // front-right
    });

    // =================================================================
    //  6 — SKELETON  (ModelSkeleton extends ModelZombie/ModelBiped)
    //      Texture: 64 x 32   (thin 2-wide limbs)
    //      Arms baked at rotX = -PI/2 (zombie forward pose)
    // =================================================================
    registerMob(6, "res/textures/mobs/skeleton.png", 64, 32,
        [&](MobMeshBuilder& b)
    {
        // Head: tex(0,0), box(-4,-8,-4, 8,8,8), RP(0,0,0)
        b.addBox( 0,  0,  -4.f,-8.f,-4.f, 8,8,8,   0.f, 0.f, 0.f);
        // Body: tex(16,16), box(-4,0,-2, 8,12,4), RP(0,0,0)
        b.addBox(16, 16,  -4.f, 0.f,-2.f, 8,12,4,  0.f, 0.f, 0.f);
        // Right arm: tex(40,16), box(-1,-2,-1, 2,12,2), RP(-5,2,0), rotX=-PI/2
        b.addBox(40, 16,  -1.f,-2.f,-1.f, 2,12,2, -5.f, 2.f, 0.f, -PI/2);
        // Left arm: tex(40,16), box(-1,-2,-1, 2,12,2), RP(5,2,0), rotX=-PI/2
        b.addBox(40, 16,  -1.f,-2.f,-1.f, 2,12,2,  5.f, 2.f, 0.f, -PI/2);
        // Right leg: tex(0,16), box(-1,0,-1, 2,12,2), RP(-2,12,0)
        b.addBox( 0, 16,  -1.f, 0.f,-1.f, 2,12,2, -2.f,12.f, 0.f);
        // Left leg: tex(0,16), box(-1,0,-1, 2,12,2), RP(2,12,0)
        b.addBox( 0, 16,  -1.f, 0.f,-1.f, 2,12,2,  2.f,12.f, 0.f);
    });

    // =================================================================
    //  7 — SPIDER  (ModelSpider)
    //      Texture: 64 x 32
    //      8 legs with baked Y + Z rotation (static standing pose)
    // =================================================================
    registerMob(7, "res/textures/mobs/spider.png", 64, 32,
        [&](MobMeshBuilder& b)
    {
        const float Q  = PI / 4.0f;           // PI/4
        const float Q8 = 0.3926991f;          // PI/8
        const float QR = Q * 0.74f;           // reduced spread

        // Head: tex(32,4), box(-4,-4,-8, 8,8,8), RP(0,15,-3)
        b.addBox(32,  4,  -4.f,-4.f,-8.f, 8,8,8,   0.f,15.f,-3.f);
        // Neck: tex(0,0), box(-3,-3,-3, 6,6,6), RP(0,15,0)
        b.addBox( 0,  0,  -3.f,-3.f,-3.f, 6,6,6,   0.f,15.f, 0.f);
        // Body: tex(0,12), box(-5,-4,-6, 10,8,12), RP(0,15,9)
        b.addBox( 0, 12,  -5.f,-4.f,-6.f, 10,8,12,  0.f,15.f, 9.f);

        // ---- RIGHT legs (extend -X) ----
        b.addBox(18,  0, -15.f,-1.f,-1.f, 16,2,2, -4.f,15.f, 2.f, 0, Q, -Q);
        b.addBox(18,  0, -15.f,-1.f,-1.f, 16,2,2, -4.f,15.f, 1.f, 0, Q8, -QR);
        b.addBox(18,  0, -15.f,-1.f,-1.f, 16,2,2, -4.f,15.f, 0.f, 0, -Q8, -QR);
        b.addBox(18,  0, -15.f,-1.f,-1.f, 16,2,2, -4.f,15.f,-1.f, 0, -Q, -Q);

        // ---- LEFT legs (extend +X) ----
        b.addBox(18,  0,  -1.f,-1.f,-1.f, 16,2,2,  4.f,15.f, 2.f, 0, -Q, Q);
        b.addBox(18,  0,  -1.f,-1.f,-1.f, 16,2,2,  4.f,15.f, 1.f, 0, -Q8, QR);
        b.addBox(18,  0,  -1.f,-1.f,-1.f, 16,2,2,  4.f,15.f, 0.f, 0, Q8, QR);
        b.addBox(18,  0,  -1.f,-1.f,-1.f, 16,2,2,  4.f,15.f,-1.f, 0, Q, Q);
    });

    // =================================================================
    //  8 — ZOMBIE  (ModelZombie extends ModelBiped)
    //      Texture: 64 x 64   (note: taller atlas than most mobs)
    //      Arms baked at rotX = -PI/2 (stretched forward)
    // =================================================================
    registerMob(8, "res/textures/mobs/zombie.png", 64, 64,
        [&](MobMeshBuilder& b)
    {
        // Head: tex(0,0), box(-4,-8,-4, 8,8,8), RP(0,0,0)
        b.addBox( 0,  0,  -4.f,-8.f,-4.f, 8,8,8,   0.f, 0.f, 0.f);
        // Body: tex(16,16), box(-4,0,-2, 8,12,4), RP(0,0,0)
        b.addBox(16, 16,  -4.f, 0.f,-2.f, 8,12,4,  0.f, 0.f, 0.f);
        // Right arm: tex(40,16), box(-3,-2,-2, 4,12,4), RP(-5,2,0), rotX=-PI/2
        b.addBox(40, 16,  -3.f,-2.f,-2.f, 4,12,4, -5.f, 2.f, 0.f, -PI/2);
        // Left arm: tex(40,16), box(-1,-2,-2, 4,12,4), RP(5,2,0), rotX=-PI/2
        b.addBox(40, 16,  -1.f,-2.f,-2.f, 4,12,4,  5.f, 2.f, 0.f, -PI/2);
        // Right leg: tex(0,16), box(-2,0,-2, 4,12,4), RP(-1.9,12,0)
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4, -1.9f,12.f, 0.f);
        // Left leg: tex(0,16), box(-2,0,-2, 4,12,4), RP(1.9,12,0)
        b.addBox( 0, 16,  -2.f, 0.f,-2.f, 4,12,4,  1.9f,12.f, 0.f);
    });

    // =================================================================
    //  9/10/11 — OCELOT / SIAMESE / BLACK CAT  (ModelOcelot)
    //      Texture: 64 x 32     All three share the same geometry.
    // =================================================================
    auto buildOcelotBoxes = [&PI](MobMeshBuilder& b) {
        // Head main: tex(0,0), box(-2.5,-2,-3, 5,4,5), RP(0,15,-9)
        b.addBox( 0,  0, -2.5f,-2.f,-3.f, 5,4,5,   0.f,15.f,-9.f);
        // Head nose (child): tex(0,24), box(-1.5,0,-4, 3,2,2), RP(0,15,-9)
        b.addBox( 0, 24, -1.5f, 0.f,-4.f, 3,2,2,   0.f,15.f,-9.f);
        // Head ear1 (child): tex(0,10), box(-2,-3,0, 1,1,2), RP(0,15,-9)
        b.addBox( 0, 10,  -2.f,-3.f, 0.f, 1,1,2,   0.f,15.f,-9.f);
        // Head ear2 (child): tex(6,10), box(1,-3,0, 1,1,2), RP(0,15,-9)
        b.addBox( 6, 10,   1.f,-3.f, 0.f, 1,1,2,   0.f,15.f,-9.f);
        // Body: tex(20,0), box(-2,3,-8, 4,16,6), RP(0,12,-10), rotX=PI/2
        b.addBox(20,  0,  -2.f, 3.f,-8.f, 4,16,6,  0.f,12.f,-10.f, PI/2);
        // Tail 1: tex(0,15), box(-0.5,0,0, 1,8,1), RP(0,15,8), rotX=0.9
        b.addBox( 0, 15, -0.5f, 0.f, 0.f, 1,8,1,   0.f,15.f, 8.f, 0.9f);
        // Tail 2: tex(4,15), box(-0.5,0,0, 1,8,1), RP(0,20,14), rotX=0.9
        b.addBox( 4, 15, -0.5f, 0.f, 0.f, 1,8,1,   0.f,20.f,14.f, 0.9f);
        // Back left leg: tex(8,13), box(-1,0,1, 2,6,2), RP(1.1,18,5)
        b.addBox( 8, 13,  -1.f, 0.f, 1.f, 2,6,2,   1.1f,18.f, 5.f);
        // Back right leg: tex(8,13), box(-1,0,1, 2,6,2), RP(-1.1,18,5)
        b.addBox( 8, 13,  -1.f, 0.f, 1.f, 2,6,2,  -1.1f,18.f, 5.f);
        // Front left leg: tex(40,0), box(-1,0,0, 2,10,2), RP(1.2,13.8,-5)
        b.addBox(40,  0,  -1.f, 0.f, 0.f, 2,10,2,  1.2f,13.8f,-5.f);
        // Front right leg: tex(40,0), box(-1,0,0, 2,10,2), RP(-1.2,13.8,-5)
        b.addBox(40,  0,  -1.f, 0.f, 0.f, 2,10,2, -1.2f,13.8f,-5.f);
    };

    registerMob( 9, "res/textures/mobs/ocelot.png",  64, 32, buildOcelotBoxes);
    registerMob(10, "res/textures/mobs/siamese.png",  64, 32, buildOcelotBoxes);
    registerMob(11, "res/textures/mobs/black.png",    64, 32, buildOcelotBoxes);

    ASCIIgL::Logger::Info("MobFactory: init complete — "
        + std::to_string(m_meshes.size()) + " mob types loaded");
}

// ===========================================================================
// spawnMob — create an entity with all required components.
//   Collider half-extents are per-type, derived from vanilla MC hitboxes.
// ===========================================================================
entt::entity MobFactory::spawnMob(uint32_t typeId, const glm::ivec3& pos) {
    auto e = m_registry.create();
    m_registry.emplace<ecs::components::MobTag>(e, typeId);

    auto& t = m_registry.emplace<ecs::components::Transform>(e);
    t.setPosition(glm::vec3(
        static_cast<float>(pos.x),
        static_cast<float>(pos.y),
        static_cast<float>(pos.z)));
    t.setScale(glm::vec3(1.0f));

    auto& rend = m_registry.emplace<ecs::components::Renderable>(e);
    auto it = m_meshes.find(typeId);
    if (it != m_meshes.end()) {
        rend.SetMesh(it->second);
        rend.renderType = ecs::components::RenderType::ELEM_3D;
        rend.layer = 1;
        rend.visible = true;
        rend.materialName = "mobMaterial_" + std::to_string(typeId);

        // Chicken and Skeleton need both sides rendered:
        // - Chicken: feet geometry faces away from camera
        // - Skeleton: see-through texture shows ribs/spine from front
        if (typeId == 2 || typeId == 6) {
            rend.disableBackfaceCulling = true;
        }
    }

    // Per-type HP matching vanilla MC 1.7.10
    int mobHp = 10; // default
    switch (typeId) {
        case 1:  mobHp = 10; break; // Pig:      10 HP (5 hearts)
        case 2:  mobHp =  4; break; // Chicken:   4 HP (2 hearts)
        case 3:  mobHp = 10; break; // Cow:      10 HP (5 hearts)
        case 4:  mobHp = 20; break; // Creeper:  20 HP (10 hearts)
        case 5:  mobHp =  8; break; // Sheep:     8 HP (4 hearts)
        case 6:  mobHp = 20; break; // Skeleton: 20 HP (10 hearts)
        case 7:  mobHp = 16; break; // Spider:   16 HP (8 hearts)
        case 8:  mobHp = 20; break; // Zombie:   20 HP (10 hearts)
        case 9:  // Ocelot (fall through)
        case 10: // Siamese
        case 11: mobHp = 10; break; // Black Cat: 10 HP (5 hearts)
        default: break;
    }
    m_registry.emplace<ecs::components::Health>(e, mobHp, mobHp);
    m_registry.emplace<ecs::components::HurtState>(e);
    m_registry.emplace<ecs::components::DeathState>(e);

    auto& vel = m_registry.emplace<ecs::components::Velocity>(e);
    vel.maxSpeed = 3.5f;

    auto& body = m_registry.emplace<ecs::components::PhysicsBody>(e);
    body.SetMass(1.0f);

    m_registry.emplace<ecs::components::StepPhysics>(e);
    m_registry.emplace<ecs::components::Gravity>(e);
    m_registry.emplace<ecs::components::GroundPhysics>(e);
    m_registry.emplace<ecs::components::FlyingPhysics>(e);

    // Per-type collider sizing (half-extents derived from vanilla MC)
    glm::vec3 half(0.45f, 0.45f, 0.45f);
    glm::vec3 off (0.0f,  0.45f, 0.0f);
    switch (typeId) {
        case 1:  half = {0.45f, 0.45f, 0.45f}; off = {0, 0.45f, 0}; break; // pig
        case 2:  half = {0.20f, 0.35f, 0.20f}; off = {0, 0.35f, 0}; break; // chicken
        case 3:  half = {0.45f, 0.70f, 0.45f}; off = {0, 0.70f, 0}; break; // cow
        case 4:  half = {0.30f, 0.85f, 0.30f}; off = {0, 0.85f, 0}; break; // creeper
        case 5:  half = {0.45f, 0.65f, 0.45f}; off = {0, 0.65f, 0}; break; // sheep
        case 6:  half = {0.30f, 0.99f, 0.30f}; off = {0, 0.99f, 0}; break; // skeleton
        case 7:  half = {0.70f, 0.45f, 0.70f}; off = {0, 0.45f, 0}; break; // spider
        case 8:  half = {0.30f, 0.99f, 0.30f}; off = {0, 0.99f, 0}; break; // zombie
        case 9:  // ocelot  (fall through)
        case 10: // siamese
        case 11: half = {0.30f, 0.35f, 0.30f}; off = {0, 0.35f, 0}; break; // black cat
        default: break;
    }

    auto& col = m_registry.emplace<ecs::components::Collider>(e);
    col.halfExtents = half;
    col.localOffset = off;
    col.disabled = false;

    m_active.push_back(e);
    ASCIIgL::Logger::Debug("MobFactory: spawned mob type "
        + std::to_string(typeId));
    return e;
}

void MobFactory::despawnMob(entt::entity e) {
    if (!m_registry.valid(e)) return;
    m_registry.destroy(e);
    m_active.erase(
        std::remove(m_active.begin(), m_active.end(), e), m_active.end());
}

void MobFactory::update(float dt) {
    // Prune destroyed entities from active list
    m_active.erase(
        std::remove_if(m_active.begin(), m_active.end(),
            [this](entt::entity e) { return !m_registry.valid(e); }),
        m_active.end());
}

} // namespace ecs::factories
