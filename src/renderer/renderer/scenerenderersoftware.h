#pragma once

#include <renderer/scenerenderer.h>

namespace Renderer
{
    struct SceneRendererSoftwareContext;

    struct SceneRendererSoftware : public SceneRenderer
    {
        bool Render(const Scene& scene, Texture& texture) override;

    private:
        std::shared_ptr<SceneRendererSoftwareContext> context;
    };
}