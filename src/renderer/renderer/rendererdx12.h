#pragma once

#include <renderer/renderer.h>
#include <string>
#include <memory>

#pragma comment(lib, "D3d12.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Renderer
{
    struct RendererDX12Context;

    struct RendererDX12 : public Renderer
    {
        RendererDX12(const std::string& path) : shaderPath(path) {};

        bool Render(const Scene& scene, Texture& texture) override;

    private:
        std::shared_ptr<RendererDX12Context> context;
        std::string shaderPath;
    };
}