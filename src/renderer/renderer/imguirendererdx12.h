#pragma once

#include <renderer/devicedx12.h>
#include <dxgi.h>

namespace Renderer
{
    struct ImguiRenderer
    {
        ImguiRenderer(const DeviceDX12& device, uint32_t gameWidth, uint32_t gameHeight, HWND window);

        void BeginRender();
        void EndRender();

    private:
        IDXGISwapChain* swapChain = nullptr;
        ID3D12DescriptorHeap* rootDescriptorHeap = nullptr;
        const DeviceDX12& deviceDX12;
    };
}