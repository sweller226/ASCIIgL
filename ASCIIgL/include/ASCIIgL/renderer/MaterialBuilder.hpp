#pragma once

#include <functional>

#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/UniformLayout.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

struct MaterialBuildDesc {
    const char* registryName = nullptr;
    const char* vsSource = nullptr;
    const char* psSource = nullptr;
    const VertFormat& vertFormat;
    UniformBufferLayout psLayout;
    bool useHLSLIncludes = true;
    std::function<bool(Material&)> bindResources;
};

bool BuildAndRegisterMaterial(const MaterialBuildDesc& desc);

} // namespace ASCIIgL
