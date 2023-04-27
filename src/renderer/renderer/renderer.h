#pragma once

#include <renderer/texture.h>
#include <renderer/scene.h>

namespace Renderer
{
    struct Renderer
    {
        virtual bool Render(const Scene& scene, Texture& texture) = 0;
    };
}