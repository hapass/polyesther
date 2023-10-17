#pragma once

#include <renderer/scenerenderer.h>

namespace Renderer
{
    struct SceneRendererSoftware : public SceneRenderer
    {
        SceneRendererSoftware() {}

        bool Render(const Scene& scene, Texture& texture) override;
    };
}