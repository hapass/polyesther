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
        desc.NumDescriptors = 1;
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

    void ImguiRenderer::BeginRender()
    {
        ImGui_ImplDX12_NewFrame();
    }

    void ImguiRenderer::EndRender()
    {
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

        swapChain->Present(1, 0);
    }
}