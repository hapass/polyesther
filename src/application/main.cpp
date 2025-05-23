#include <Windows.h>

#include <renderer/math.h>
#include <renderer/scenerendererdx12.h>
#include <renderer/imguirendererdx12.h>
#include <renderer/scenerenderersoftware.h>
#include <renderer/color.h>
#include <renderer/scene.h>
#include <renderer/texture.h>

#include <utils.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <filesystem>

constexpr int32_t WindowWidth = 1280;
constexpr int32_t WindowHeight = 720;

constexpr int32_t RenderWidth = 800;
constexpr int32_t RenderHeight = 600;

void CorrectRotation(float& angle)
{
    if (angle < 0)
    {
        angle += static_cast<float>(M_PI) * 2;
    }
    else if (angle > static_cast<float>(M_PI) * 2)
    {
        angle -= static_cast<float>(M_PI) * 2;
    }
}

void HandleInput(Renderer::Scene& scene)
{
    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_UpArrow))
    {
        scene.camera.pitch -= 0.01f;
    }

    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_DownArrow))
    {
        scene.camera.pitch += 0.01f;
    }

    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_RightArrow))
    {
        scene.camera.yaw -= 0.01f;
    }

    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_LeftArrow))
    {
        scene.camera.yaw += 0.01f;
    }

    CorrectRotation(scene.camera.pitch);
    CorrectRotation(scene.camera.yaw);

    Renderer::Vec forward = Renderer::CameraTransform(scene.camera) * scene.camera.forward;
    Renderer::Vec left = Renderer::CameraTransform(scene.camera) * scene.camera.left;

    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_W))
    {
        scene.camera.position = scene.camera.position + forward * 0.1f;
    }

    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_A))
    {
        scene.camera.position = scene.camera.position + left * 0.1f;
    }

    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_S))
    {
        scene.camera.position = scene.camera.position - forward * 0.1f;
    }

    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_D))
    {
        scene.camera.position = scene.camera.position - left * 0.1f;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct WindowContext
{
    WindowContext(HWND hWnd)
        : assetsDir(std::filesystem::current_path().string() + "\\assets\\")
        , hardwareRenderer(assetsDir, device)
        , imguiRenderer(device, WindowWidth, WindowHeight, hWnd)
    {
        NOT_FAILED(Renderer::Load(assetsDir + "cars\\scene.sce", scene), false);
        renderer = &hardwareRenderer;
    }

    std::string assetsDir;

    Renderer::SceneRendererSoftware softwareRenderer;
    Renderer::DeviceDX12 device;
    Renderer::SceneRendererDX12 hardwareRenderer;
    Renderer::ImguiRenderer imguiRenderer;
    Renderer::Scene scene;

    Renderer::SceneRenderer* renderer = nullptr;
};

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (LRESULT r = ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return r;

    static std::unique_ptr<WindowContext> windowContext;

    switch (uMsg)
    {
        case WM_CREATE:
        {
            windowContext = std::make_unique<WindowContext>(hWnd);
            ImGui_ImplWin32_Init(hWnd);
            return 0;
        }
        case WM_PAINT:
        {
            ImGui_ImplWin32_NewFrame();

            HandleInput(windowContext->scene);

            Renderer::Texture result(RenderWidth, RenderHeight);

            Utils::FrameCounter::GetInstance().Start("Frame time");
            windowContext->renderer->Render(windowContext->scene, result);
            Utils::FrameCounter::GetInstance().End();

            std::stringstream ss;
            Utils::FrameCounter::GetInstance().GetPerformanceString(ss);

            windowContext->imguiRenderer.Render(result, [&result, &ss](ImTextureID id) {
                if (ImGui::BeginMainMenuBar())
                {
                    if (ImGui::BeginMenu("File"))
                    {
                        if (ImGui::MenuItem("Open", "Ctrl+O")) {}
                        if (ImGui::MenuItem("Save", "Ctrl+S")) {}
                        ImGui::EndMenu();
                    }
                    ImGui::EndMainMenuBar();
                }

                ImGui::Begin("Scene");
                ImVec2 imageSize = ImVec2((float)result.GetWidth(), (float)result.GetHeight());
                ImVec2 cursorPosition = ImVec2((ImGui::GetWindowSize()[0] - imageSize[0]) * 0.5f, (ImGui::GetWindowSize()[1] - imageSize[1]) * 0.5f);
                ImGui::SetCursorPos(cursorPosition);
                ImGui::Image(id, imageSize);
                ImGui::End();

                ImGui::Begin("Properties");

                ImGui::Text("Current rasterizer: ");
                ImGui::SameLine();
                ImGui::Text(windowContext->renderer == &windowContext->hardwareRenderer ? "Hardware Rasterizer" : "Software Rasterizer");
                ImGui::Separator();
                ImGui::Text("Help:");
                ImGui::Text("Press R to switch renderer.");
                ImGui::Text("Use arrow keys to turn the camera.");
                ImGui::Text("Use wasd keys to move the camera.");
                ImGui::Separator();

                ImGui::Text(ss.str().c_str());
                ImGui::Separator();

                ImGui::Text("Camera:");
                ImGui::Text("x: %f; y: %f; z: %f",
                    windowContext->scene.camera.position.x,
                    windowContext->scene.camera.position.y,
                    windowContext->scene.camera.position.z
                );
                ImGui::Text("pitch: %f; yaw: %f",
                    windowContext->scene.camera.pitch,
                    windowContext->scene.camera.yaw
                );
                ImGui::Separator();

                ImGui::Text("Show: ");

                if (ImGui::SmallButton("Final image"))
                {
                    windowContext->scene.debugContext.DisplayedGBufferTextureIndex = -1;
                }

                if (ImGui::SmallButton("View space positions"))
                {
                    windowContext->scene.debugContext.DisplayedGBufferTextureIndex = 0;
                }

                if (ImGui::SmallButton("View space normals"))
                {
                    windowContext->scene.debugContext.DisplayedGBufferTextureIndex = 1;
                }

                if (ImGui::SmallButton("Diffuse"))
                {
                    windowContext->scene.debugContext.DisplayedGBufferTextureIndex = 2;
                }

                if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_R))
                {
                    windowContext->renderer = windowContext->renderer == &windowContext->hardwareRenderer ? 
                        (Renderer::SceneRenderer*)&windowContext->softwareRenderer :
                        (Renderer::SceneRenderer*)&windowContext->hardwareRenderer;
                }

                ImGui::End();
            });

            return 0;
        }
        case WM_DESTROY:
        {
            windowContext.reset();
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    Utils::DebugUtils::GetInstance().AddOutput([](const std::string& message) {
        OutputDebugStringA(message.c_str());
    });

    WNDCLASS MainWindowClass = {};
    MainWindowClass.style = CS_HREDRAW | CS_VREDRAW;
    MainWindowClass.lpfnWndProc = MainWindowProc;
    MainWindowClass.hInstance = hInstance;
    MainWindowClass.lpszClassName = L"MainWindow";

    if (RegisterClassW(&MainWindowClass))
    {
        RECT clientArea;
        clientArea.left = 0;
        clientArea.top = 0;
        clientArea.right = WindowWidth;
        clientArea.bottom = WindowHeight;

        AdjustWindowRect(&clientArea, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 0);

        CreateWindowExW(
            0,
            MainWindowClass.lpszClassName,
            L"Polyesther",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            clientArea.right - clientArea.left,
            clientArea.bottom - clientArea.top,
            0,
            0,
            hInstance,
            0
        );

        MSG message {};
        while (message.message != WM_QUIT)
        {
            if (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    }

    return 0;
}