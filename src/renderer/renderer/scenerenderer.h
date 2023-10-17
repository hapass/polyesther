#pragma once

#include <renderer/devicedx12.h>
#include <renderer/texture.h>
#include <renderer/scene.h>

namespace Renderer
{
    struct SceneRenderer
    {
        virtual bool Render(const Scene& scene, Texture& texture) = 0;
    };
}