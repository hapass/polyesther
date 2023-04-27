#pragma once

#include <renderer/renderer.h>

namespace Renderer
{
    struct RendererDX12 : public Renderer
    {
        bool Render(const Scene& scene, Texture& texture) override;
    };
}