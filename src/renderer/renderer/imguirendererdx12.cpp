#include <renderer/imguirendererdx12.h>
#include <renderer/utils.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>

namespace Renderer
{
    ImguiRenderer::ImguiRenderer(const DeviceDX12& device, uint32_t gameWidth, uint32_t gameHeight, HWND window)
        : deviceDX12(device)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 2;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        D3D_NOT_FAILED(device.GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rootDescriptorHeap)));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

        ImGui_ImplDX12_Init(device.GetDevice(), 1,
            DXGI_FORMAT_R8G8B8A8_UNORM, rootDescriptorHeap,
            rootDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            rootDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

        DXGI_SWAP_CHAIN_DESC swapChainDescription;

        swapChainDescription.BufferDesc.Width = gameWidth;
        swapChainDescription.BufferDesc.Height = gameHeight;
        swapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDescription.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        swapChainDescription.SampleDesc.Count = 1;
        swapChainDescription.SampleDesc.Quality = 0;

        swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDescription.BufferCount = 2;

        swapChainDescription.OutputWindow = window;
        swapChainDescription.Windowed = true;

        swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDescription.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        IDXGIFactory* factory = nullptr;
        D3D_NOT_FAILED(CreateDXGIFactory(IID_PPV_ARGS(&factory)));

        IDXGISwapChain* tempSwapChain = nullptr;
        D3D_NOT_FAILED(factory->CreateSwapChain(device.GetQueue().GetQueue(), &swapChainDescription, &tempSwapChain));

        tempSwapChain->QueryInterface<IDXGISwapChain3>(&swapChain);

        ID3D12DescriptorHeap* backBufferDescHeap = nullptr;
        D3D12_DESCRIPTOR_HEAP_DESC backBufferDescHeapDesc = {};
        backBufferDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        backBufferDescHeapDesc.NumDescriptors = 2;
        backBufferDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        backBufferDescHeapDesc.NodeMask = 1;
        D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateDescriptorHeap(&backBufferDescHeapDesc, IID_PPV_ARGS(&backBufferDescHeap)));

        SIZE_T rtvDescriptorSize = deviceDX12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = backBufferDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < 2; i++)
        {
            mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }

        for (UINT i = 0; i < 2; i++)
        {
            ID3D12Resource* pBackBuffer = nullptr;
            swapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
            deviceDX12.GetDevice()->CreateRenderTargetView(pBackBuffer, nullptr, mainRenderTargetDescriptor[i]);
            mainRenderTargetResource[i] = pBackBuffer;
        }
    }

    D3D12_GPU_DESCRIPTOR_HANDLE ImguiRenderer::UploadTexturesToGPU(const Texture& texture)
    {
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = texture.GetWidth();
        textureDesc.Height = static_cast<UINT>(texture.GetHeight());
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 totalBytes = 0;
        deviceDX12.GetDevice()->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

        if (!textureUploadBuffer)
        {
            D3D12_RESOURCE_DESC uploadBufferDescription;
            uploadBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            uploadBufferDescription.Alignment = 0;
            uploadBufferDescription.Width = totalBytes;
            uploadBufferDescription.Height = 1;
            uploadBufferDescription.DepthOrArraySize = 1;
            uploadBufferDescription.MipLevels = 1;
            uploadBufferDescription.Format = DXGI_FORMAT_UNKNOWN;
            uploadBufferDescription.SampleDesc.Count = 1;
            uploadBufferDescription.SampleDesc.Quality = 0;
            uploadBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            uploadBufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES uploadProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&uploadProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDescription, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&textureUploadBuffer)));
            textureUploadBuffer->SetName(L"Imgui texture upload buffer.");
            deviceDX12.GetQueue().AddBarrierToList(textureUploadBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);
        }

        if (!textureBuffer)
        {
            D3D12_HEAP_PROPERTIES defaultProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&textureBuffer)));
            textureBuffer->SetName(L"Imgui texture buffer");
            deviceDX12.GetQueue().AddBarrierToList(textureBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        }

        BYTE* mappedData = nullptr;
        textureUploadBuffer->Map(0, nullptr, (void**)&mappedData);

        for (size_t i = 0; i < numRows; i++)
        {
            memcpy(mappedData + footprint.Footprint.RowPitch * i, texture.GetBuffer() + rowSizeInBytes * i, rowSizeInBytes);
        }
        textureUploadBuffer->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION dest;
        dest.pResource = textureBuffer;
        dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest.PlacedFootprint = {};
        dest.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src;
        src.pResource = textureUploadBuffer;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;

        deviceDX12.GetQueue().GetList()->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
        deviceDX12.GetQueue().AddBarrierToList(textureBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

        deviceDX12.GetQueue().Execute();

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE textureCPUDescriptor { SIZE_T(INT64(rootDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(deviceDX12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))) };
        D3D12_GPU_DESCRIPTOR_HANDLE textureGPUDescriptor { UINT64(INT64(rootDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr) + INT64(deviceDX12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))) };
        deviceDX12.GetDevice()->CreateShaderResourceView(textureBuffer, &srvDesc, textureCPUDescriptor);

        return textureGPUDescriptor;
    }

    void ImguiRenderer::Render(const Texture& texture, const std::function<void(ImTextureID)>& func)
    {
        ImGui_ImplDX12_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();

        D3D12_GPU_DESCRIPTOR_HANDLE handle = UploadTexturesToGPU(texture); // todo.pavelza: add GPU handle to Image?

        func((ImTextureID)handle.ptr);

        ImGui::Render();

        UINT backBufferIdx = swapChain->GetCurrentBackBufferIndex();

        deviceDX12.GetQueue().AddBarrierToList(mainRenderTargetResource[backBufferIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // Render Dear ImGui graphics
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        deviceDX12.GetQueue().GetList()->ClearRenderTargetView(mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, nullptr);
        deviceDX12.GetQueue().GetList()->OMSetRenderTargets(1, &mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);
        deviceDX12.GetQueue().GetList()->SetDescriptorHeaps(1, &rootDescriptorHeap);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), deviceDX12.GetQueue().GetList());

        deviceDX12.GetQueue().AddBarrierToList(mainRenderTargetResource[backBufferIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        deviceDX12.GetQueue().Execute();

        // Update and Render additional Platform Windows
        //ImGui::UpdatePlatformWindows();
        //ImGui::RenderPlatformWindowsDefault(nullptr, (void*)deviceDX12.GetQueue().GetList());

        // todo.pavelza: shutdown, clean, etc
        swapChain->Present(1, 0);
    }
}