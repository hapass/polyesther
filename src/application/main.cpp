#define _USE_MATH_DEFINES

#include <Windows.h>
#include <array>

#include <renderer/math.h>
#include <renderer/scenerendererdx12.h>
#include <renderer/imguirendererdx12.h> // todo.pavelza: move win32 header and cpp from renderer into application
#include <renderer/scenerenderersoftware.h>
#include <renderer/color.h>
#include <renderer/scene.h>
#include <renderer/texture.h>
#include <renderer/utils.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

namespace
{
    constexpr int32_t WindowWidth = 1920;
    constexpr int32_t WindowHeight = 1080;

    constexpr int32_t RenderWidth = 800;
    constexpr int32_t RenderHeight = 600;

    const char* GetCurrentRendererName(Renderer::SceneRendererDX12& hardwareRenderer, Renderer::SceneRenderer* renderer)
    {
        return renderer == &hardwareRenderer ? "Hardware Rasterizer" : "Software Rasterizer";
    }

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
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (LRESULT r = ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return r;

    static std::string SolutionDir = _SOLUTIONDIR;
    static std::string AssetsDir = SolutionDir + "assets\\";

    static Renderer::SceneRendererSoftware softwareRenderer;
    static Renderer::DeviceDX12 device;
    static Renderer::SceneRendererDX12 hardwareRenderer(AssetsDir + "color.hlsl", device);
    static Renderer::ImguiRenderer imguiRenderer(device, WindowWidth, WindowHeight, hWnd);
    static Renderer::SceneRenderer* renderer = &hardwareRenderer;
    static Renderer::Scene scene;

    switch (uMsg)
    {
        case WM_CREATE:
        {
            ImGui_ImplWin32_Init(hWnd);

            NOT_FAILED(Renderer::Load(AssetsDir + "cars\\scene.sce", scene), false);
            return 0;
        }
        case WM_PAINT:
        {
            ImGui_ImplWin32_NewFrame();

            HandleInput(scene);

            Renderer::Texture result(RenderWidth, RenderHeight);
            renderer->Render(scene, result);

            imguiRenderer.Render(result, [&result](ImTextureID id) {
                if (ImGui::BeginMainMenuBar())
                {
                    if (ImGui::BeginMenu("File"))
                    {
                        ImGui::MenuItem("(demo menu)", NULL, false, false);
                        if (ImGui::MenuItem("New")) {}
                        if (ImGui::MenuItem("Open", "Ctrl+O")) {}
                        if (ImGui::BeginMenu("Open Recent"))
                        {
                            ImGui::MenuItem("fish_hat.c");
                            ImGui::MenuItem("fish_hat.inl");
                            ImGui::MenuItem("fish_hat.h");
                            ImGui::EndMenu();
                        }
                        if (ImGui::MenuItem("Save", "Ctrl+S")) {}
                        if (ImGui::MenuItem("Save As..")) {}

                        ImGui::Separator();
                        if (ImGui::BeginMenu("Options"))
                        {
                            static bool enabled = true;
                            ImGui::MenuItem("Enabled", "", &enabled);
                            ImGui::BeginChild("child", ImVec2(0, 60), true);
                            for (int i = 0; i < 10; i++)
                                ImGui::Text("Scrolling Text %d", i);
                            ImGui::EndChild();
                            static float f = 0.5f;
                            static int n = 0;
                            ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
                            ImGui::InputFloat("Input", &f, 0.1f);
                            ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
                            ImGui::EndMenu();
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMainMenuBar();
                }

                ImGui::Begin("Scene");

                if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_R))
                {
                    if (renderer == &hardwareRenderer)
                    {
                        renderer = &softwareRenderer;
                    }
                    else
                    {
                        renderer = &hardwareRenderer;
                    }
                }

                ImGui::Text(GetCurrentRendererName(hardwareRenderer, renderer));

                ImGui::Image(id, ImVec2((float)result.GetWidth(), (float)result.GetHeight()));

                ImGui::End();
            });

            return 0;
        }
        case WM_DESTROY:
        {
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

        AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);

        CreateWindowExW(
            0,
            MainWindowClass.lpszClassName,
            L"Polyesther",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
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