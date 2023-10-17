#pragma once

#include <renderer/devicedx12.h>

namespace Renderer
{
    struct ImguiRenderer
    {
        ImguiRenderer(const DeviceDX12& device);

        void BeginRender();
        void EndRender();

    private:
        const DeviceDX12& deviceDX12;
    };
}