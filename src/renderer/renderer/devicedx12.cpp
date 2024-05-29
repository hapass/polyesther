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
        if (pso != currentPSO)
        {
            currentPSO = pso;
            commandList->SetPipelineState(pso);
        }
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

    RenderTarget::RenderTarget(const DeviceDX12& device, ID3D12DescriptorHeap* srvDescriptorHeap, size_t width, size_t height, Type type)
        : deviceDX12(device)
        , bufferType(type)
        , targetWidth(width)
        , targetHeight(height)
        , rootDescriptorHeap(srvDescriptorHeap)
    {
        size_t numBuffers = bufferType == Type::GBuffer ? 3 : 1;
        renderTargets.resize(numBuffers);
        rtvDescriptorHeaps.resize(numBuffers);
        rtvHandles.resize(numBuffers);
        srvHandles.resize(numBuffers);

        for (size_t i = 0; i < numBuffers; i++)
        {
            CreateBuffer(i, width, height);
        }
        CreateDepthBuffer(width, height);
    }

    RenderTarget::~RenderTarget()
    {
        depthStencilBuffer->Release();
        depthStencilViewDescriptorHeap->Release();

        for (ID3D12Resource* resource : renderTargets)
        {
            resource->Release();
        }
        for (ID3D12DescriptorHeap* heap : rtvDescriptorHeaps)
        {
            heap->Release();
        }
    }

    void RenderTarget::ClearAndSetRenderTargets(GraphicsQueue& queue)
    {
        for (size_t i = 0; i < rtvHandles.size(); i++)
        {
            queue.GetList()->ClearRenderTargetView(rtvHandles[i], clearColor, 0, nullptr);
        }
        queue.GetList()->ClearDepthStencilView(depthBufferHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        queue.GetList()->OMSetRenderTargets((UINT)rtvHandles.size(), rtvHandles.data(), false, &depthBufferHandle);
    }

    ID3D12Resource* RenderTarget::GetBuffer(size_t i)
    {
        return renderTargets[i];
    }

    void RenderTarget::CreateBuffer(size_t i, size_t width, size_t height)
    {
        ASSERT(rtvDescriptorHeaps.size() >= i);
        ASSERT(renderTargets.size() >= 0);
        ASSERT(rtvHandles.size() >= 0);

        D3D12_DESCRIPTOR_HEAP_DESC heapDescription;
        heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDescription.NumDescriptors = 1;
        heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDescription.NodeMask = 0;

        D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&rtvDescriptorHeaps[i])));

        D3D12_RESOURCE_DESC textureDesc = CreateTextureDescription(DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        D3D12_CLEAR_VALUE clearValue = CreateClearValue(textureDesc);
        D3D12_HEAP_PROPERTIES defaultProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);

        D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&renderTargets[i])));
        renderTargets[i]->SetName(L"Render target.");

        rtvHandles[i] = { SIZE_T(INT64(rtvDescriptorHeaps[i]->GetCPUDescriptorHandleForHeapStart().ptr)) };
        deviceDX12.GetDevice()->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandles[i]);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        srvHandles[i] = { SIZE_T(INT64(rootDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(deviceDX12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * (bufferType == FinalImage ? (i + 1) : (i + 2)))) };
        deviceDX12.GetDevice()->CreateShaderResourceView(renderTargets[i], &srvDesc, srvHandles[i]);
    }

    void RenderTarget::CreateDepthBuffer(size_t width, size_t height)
    {
        D3D12_RESOURCE_DESC depthBufferDescription = CreateTextureDescription(DXGI_FORMAT_D24_UNORM_S8_UINT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        D3D12_CLEAR_VALUE clearValue = CreateClearValue(depthBufferDescription);
        D3D12_HEAP_PROPERTIES defaultHeapProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);

        D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &depthBufferDescription, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&depthStencilBuffer)));
        depthStencilBuffer->SetName(L"Depth stencil buffer.");

        D3D12_DESCRIPTOR_HEAP_DESC heapDescription;
        heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heapDescription.NumDescriptors = 1;
        heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDescription.NodeMask = 0;

        D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&depthStencilViewDescriptorHeap)));

        depthBufferHandle = D3D12_CPU_DESCRIPTOR_HANDLE(depthStencilViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        deviceDX12.GetDevice()->CreateDepthStencilView(depthStencilBuffer, nullptr, depthBufferHandle);

        deviceDX12.GetQueue().AddBarrierToList(depthStencilBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        deviceDX12.GetQueue().Execute();
    }

    D3D12_CLEAR_VALUE RenderTarget::CreateClearValue(D3D12_RESOURCE_DESC textureDescription)
    {
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = textureDescription.Format;

        if (textureDescription.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        {
            clearValue.DepthStencil.Depth = 1.0f;
            clearValue.DepthStencil.Stencil = 0;
        }
        else
        {
            std::copy(clearColor, clearColor + 4, clearValue.Color);
        }

        return clearValue;
    }

    D3D12_RESOURCE_DESC RenderTarget::CreateTextureDescription(DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
    {
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.Format = format;
        textureDesc.Flags = flags;

        textureDesc.Alignment = 0;
        textureDesc.MipLevels = 1;
        textureDesc.Width = static_cast<UINT64>(targetWidth);
        textureDesc.Height = static_cast<UINT>(targetHeight);
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        return textureDesc;
    }
}
