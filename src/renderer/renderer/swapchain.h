#pragma once

#include <renderer/texture.h>

namespace Renderer
{
    class Swapchain
    {
    public:
        virtual void Present(const Texture& image) = 0;
    };
}