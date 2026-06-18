#include <ASCIIgL/renderer/MaterialBuilder.hpp>

#include <ASCIIgL/renderer/HLSLIncludes.hpp>
#include <ASCIIgL/renderer/Shader.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <glm/glm.hpp>

namespace ASCIIgL {

bool BuildAndRegisterMaterial(const MaterialBuildDesc& desc) {
    if (!desc.registryName || !desc.vsSource || !desc.psSource) {
        Logger::Error("Material build failed: missing registry name or shader source");
        return false;
    }

    auto vs = Shader::CreateFromSource(desc.vsSource, ShaderType::Vertex);

    std::unique_ptr<Shader> ps;
    if (desc.useHLSLIncludes) {
        ShaderIncludeMap includes;
        HLSLIncludes::AddToMap(includes);
        ps = Shader::CreateFromSource(desc.psSource, ShaderType::Pixel, "main", &includes);
    } else {
        ps = Shader::CreateFromSource(desc.psSource, ShaderType::Pixel);
    }

    if (!vs || !vs->IsValid()) {
        const std::string err = vs ? vs->GetCompileError() : "allocation failed";
        Logger::Error(std::string("Failed to compile ") + desc.registryName + " vertex shader: " + err);
        return false;
    }
    if (!ps || !ps->IsValid()) {
        const std::string err = ps ? ps->GetCompileError() : "allocation failed";
        Logger::Error(std::string("Failed to compile ") + desc.registryName + " pixel shader: " + err);
        return false;
    }

    auto program = ShaderProgram::Create(
        std::move(vs), std::move(ps),
        desc.vertFormat,
        desc.psLayout
    );
    if (!program || !program->IsValid()) {
        Logger::Error(std::string("Failed to create ") + desc.registryName + " shader program");
        return false;
    }

    auto material = Material::Create(std::move(program));
    if (!material) {
        Logger::Error(std::string("Failed to create ") + desc.registryName + " material");
        return false;
    }

    if (desc.applyScreenGradient) {
        auto& palette = Screen::GetInst().GetPalette();
        material->SetFloat4("gradientStart", glm::vec4(palette.GetRGBNormalized(palette.GetMinLumIdx()), 1.0f));
        material->SetFloat4("gradientEnd", glm::vec4(palette.GetRGBNormalized(palette.GetMaxLumIdx()), 1.0f));
    }

    if (desc.bindResources && !desc.bindResources(*material)) {
        return false;
    }

    MaterialLibrary::GetInst().Register(desc.registryName, std::shared_ptr<Material>(std::move(material)));
    return true;
}

} // namespace ASCIIgL
