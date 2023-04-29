#pragma once

#include <renderer/renderer.h>
#include <memory>

namespace Renderer
{
    struct RendererDX12Context;

    struct RendererDX12 : public Renderer
    {
        bool Render(const Scene& scene, Texture& texture) override;

    private:
        std::shared_ptr<RendererDX12Context> context;
    };
}