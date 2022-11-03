#pragma once

#include <renderer/texture.h>

namespace Renderer
{
    class Swapchain
    {
    public:
        void Present(const Texture& image);
    };
}