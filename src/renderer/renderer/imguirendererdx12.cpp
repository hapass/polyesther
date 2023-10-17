#include <renderer/imguirendererdx12.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>

namespace Renderer
{
    ImguiRenderer::ImguiRenderer(const DeviceDX12& device)
        : deviceDX12(device)
    {
        //ImGui_ImplDX12_Init(device.GetDevice(), 1,
        //    DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
        //    g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        //    g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

        //IMGUI_CHECKVERSION();
        //ImGui::CreateContext();
        //ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    }

    void ImguiRenderer::BeginRender()
    {
        ImGui_ImplDX12_NewFrame();
    }

    void ImguiRenderer::EndRender()
    {
        ImGui::Render();

        //FrameContext* frameCtx = WaitForNextFrameResources();
        //UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
        //frameCtx->CommandAllocator->Reset();

        //D3D12_RESOURCE_BARRIER barrier = {};
        //barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        //barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        //barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
        //barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        //barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        //barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        //g_pd3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
        //g_pd3dCommandList->ResourceBarrier(1, &barrier);

        //// Render Dear ImGui graphics
        //const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        //g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, nullptr);
        //g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);
        //g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
        //ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
        //barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        //barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        //g_pd3dCommandList->ResourceBarrier(1, &barrier);
        //g_pd3dCommandList->Close();

        //g_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_pd3dCommandList);

        //// Update and Render additional Platform Windows
        //if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        //{
        //    ImGui::UpdatePlatformWindows();
        //    ImGui::RenderPlatformWindowsDefault(nullptr, (void*)g_pd3dCommandList);
        //}

        //g_pSwapChain->Present(1, 0);

        //UINT64 fenceValue = g_fenceLastSignaledValue + 1;
        //g_pd3dCommandQueue->Signal(g_fence, fenceValue);
        //g_fenceLastSignaledValue = fenceValue;
        //frameCtx->FenceValue = fenceValue;
    }
}