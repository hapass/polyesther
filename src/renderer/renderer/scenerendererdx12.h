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
    
    struct SigDefinition
    {
        std::vector<std::tuple<std::string, std::string>> vertex;
        std::vector<std::tuple<std::string, std::string>> constants;
    };

    struct SceneRendererDX12 : public SceneRenderer
    {
        SceneRendererDX12(const std::string& path, const DeviceDX12& device) : deviceDX12(device), shaderFolderPath(path) {};

        bool Render(const Scene& scene, Texture& texture) override;

    private:
        std::shared_ptr<RendererDX12Context> context;
        std::string shaderFolderPath;
        const DeviceDX12& deviceDX12;
    };

    bool Load(const std::string& path, SigDefinition& definition);
}