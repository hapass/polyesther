#pragma once

#include <d3d12.h>
#include <memory>
#include <vector>

namespace Renderer
{
    struct GraphicsQueue
    {
        GraphicsQueue(ID3D12Device* device);

        GraphicsQueue(const GraphicsQueue& other) = delete;
        GraphicsQueue(GraphicsQueue&& other) noexcept = delete;
        GraphicsQueue& operator=(const GraphicsQueue& other) = delete;
        GraphicsQueue& operator=(GraphicsQueue&& other) noexcept = delete;

        ~GraphicsQueue();

        ID3D12GraphicsCommandList* GetList();
        ID3D12CommandQueue* GetQueue();

        void AddBarrierToList(ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);
        void SetCurrentPipelineStateObject(ID3D12PipelineState* pso);

        void Execute();

    private:
        void WaitForCommandListCompletion();

        ID3D12PipelineState* currentPSO = nullptr; // not owned
        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12GraphicsCommandList* commandList = nullptr;
        ID3D12CommandQueue* queue = nullptr;
        ID3D12Fence* fence = nullptr;

        UINT64 currentFenceValue = 0;
        HANDLE fenceEventHandle = 0;
    };

    struct DeviceDX12
    {
        DeviceDX12();
        ~DeviceDX12();

        GraphicsQueue& GetQueue() const;
        ID3D12Device* GetDevice() const;

    private:
        std::unique_ptr<GraphicsQueue> graphicsQueue;
        ID3D12Device* device = nullptr;
    };

    struct RenderTarget
    {
        enum Type
        {
            GBuffer,
            FinalImage
        };

        RenderTarget(const DeviceDX12& device, size_t width, size_t height, Type type);

        RenderTarget(const RenderTarget& other) = delete;
        RenderTarget(RenderTarget&& other) noexcept = delete;
        RenderTarget& operator=(const RenderTarget& other) = delete;
        RenderTarget& operator=(RenderTarget&& other) noexcept = delete;

        ~RenderTarget();

        void ClearAndSetRenderTargets(GraphicsQueue& queue);

        ID3D12Resource* GetBuffer(size_t i);

    private:
        void CreateDepthBuffer(size_t width, size_t height);
        void CreateBuffer(size_t i, size_t width, size_t height);

        D3D12_CLEAR_VALUE CreateClearValue(D3D12_RESOURCE_DESC textureDescription);
        D3D12_RESOURCE_DESC CreateTextureDescription(DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

        const DeviceDX12& deviceDX12;

        ID3D12Resource* depthStencilBuffer = nullptr;
        ID3D12DescriptorHeap* depthStencilViewDescriptorHeap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE depthBufferHandle{};

        std::vector<ID3D12Resource*> buffers;
        std::vector<ID3D12DescriptorHeap*> renderTargetViewDescriptorHeaps;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> bufferHandles;

        FLOAT clearColor[4] = { 0.0f, 0.f, 0.f, 1.000000000f };
        Type bufferType;
        size_t targetWidth;
        size_t targetHeight;
    };
}