#pragma once

#include <renderer/scenerenderer.h>
#include <string>
#include <memory>

#pragma comment(lib, "D3d12.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Renderer
{
    struct RendererDX12Context;

    struct SceneRendererDX12 : public SceneRenderer
    {
        SceneRendererDX12(const std::string& path, const DeviceDX12& device) : deviceDX12(device), shaderPath(path) {};

        bool Render(const Scene& scene, Texture& texture) override;

    private:
        std::shared_ptr<RendererDX12Context> context;
        std::string shaderPath;
        const DeviceDX12& deviceDX12;
    };
}