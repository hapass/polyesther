#pragma once

#include <renderer/renderer.h>

namespace Renderer
{
    struct RendererSoftware : public Renderer
    {
        bool Render(const Scene& scene, Texture& texture) override;
    };
}