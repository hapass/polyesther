#pragma once

#include <renderer/texture.h>
#include <renderer/scene.h>

namespace Renderer
{
    class Renderer
    {
    public:
        virtual Texture* Render(const Scene& scene) = 0;
    };
}