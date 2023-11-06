#pragma once

#include <renderer/devicedx12.h>
#include <functional>
#include <dxgi1_4.h>

namespace Renderer
{
    struct ImguiRenderer
    {
        ImguiRenderer(const DeviceDX12& device, uint32_t gameWidth, uint32_t gameHeight, HWND window);

        void Render(const std::function<void()>& func);

    private:
        ID3D12Resource* mainRenderTargetResource[2] = {};
        D3D12_CPU_DESCRIPTOR_HANDLE mainRenderTargetDescriptor[2] = {};

        IDXGISwapChain3* swapChain = nullptr;
        ID3D12DescriptorHeap* rootDescriptorHeap = nullptr;
        const DeviceDX12& deviceDX12;
    };
}