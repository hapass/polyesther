#pragma once

#include <d3d12.h>
#include <memory>

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
}