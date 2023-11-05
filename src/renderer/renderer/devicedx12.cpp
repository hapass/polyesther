#include <renderer/devicedx12.h>
#include <renderer/utils.h>

namespace Renderer
{
    GraphicsQueue::GraphicsQueue(ID3D12Device* device)
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        D3D_NOT_FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));

        D3D_NOT_FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        D3D_NOT_FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&commandList)));

        commandList->SetName(L"Main command list");

        D3D_NOT_FAILED(device->CreateFence(currentFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        NOT_FAILED(fenceEventHandle = CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS), 0);
    }

    GraphicsQueue::~GraphicsQueue()
    {
        fence->Release();
        commandList->Release();
        allocator->Release();
        queue->Release();
        currentPSO = nullptr;
    }

    ID3D12GraphicsCommandList* GraphicsQueue::GetList()
    {
        return commandList;
    }

    ID3D12CommandQueue* GraphicsQueue::GetQueue()
    {
        return queue;
    }

    void GraphicsQueue::AddBarrierToList(ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
    {
        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        barrier.Transition.StateBefore = from;
        barrier.Transition.StateAfter = to;
        barrier.Transition.pResource = resource;

        commandList->ResourceBarrier(1, &barrier);
    }

    void GraphicsQueue::SetCurrentPipelineStateObject(ID3D12PipelineState* pso)
    {
        currentPSO = pso;
        commandList->SetPipelineState(pso);
    }

    void GraphicsQueue::Execute()
    {
        commandList->Close();

        ID3D12CommandList* cmdsLists[1] = { commandList };
        queue->ExecuteCommandLists(1, cmdsLists);

        WaitForCommandListCompletion();

        D3D_NOT_FAILED(allocator->Reset());
        D3D_NOT_FAILED(commandList->Reset(allocator, currentPSO));
    }

    void GraphicsQueue::WaitForCommandListCompletion()
    {
        currentFenceValue++;
        queue->Signal(fence, currentFenceValue);

        if (fence->GetCompletedValue() != currentFenceValue)
        {
            D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
            WaitForSingleObject(fenceEventHandle, INFINITE);
            ResetEvent(fenceEventHandle);
        }
    }

    DeviceDX12::DeviceDX12()
    {
        ID3D12Debug* debugController = nullptr;
        D3D_NOT_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
        debugController->Release();

        D3D_NOT_FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));

        graphicsQueue = std::make_unique<GraphicsQueue>(device);
    }

    DeviceDX12::~DeviceDX12()
    {
        device->Release();
    }

    GraphicsQueue& DeviceDX12::GetQueue() const
    {
        return *graphicsQueue;
    }

    ID3D12Device* DeviceDX12::GetDevice() const
    {
        return device;
    }
}
