#define _USE_MATH_DEFINES

#include <Windows.h>
#include <array>

#include <renderer/math.h>
#include <renderer/scenerendererdx12.h>
#include <renderer/imguirendererdx12.h>
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
    constexpr uint8_t WKey = 87;
    constexpr uint8_t AKey = 65;
    constexpr uint8_t SKey = 83;
    constexpr uint8_t DKey = 68;

    constexpr uint8_t RKey = 82;

    constexpr uint8_t UpKey = 38;
    constexpr uint8_t LeftKey = 37;
    constexpr uint8_t DownKey = 40;
    constexpr uint8_t RightKey = 39;

    constexpr int32_t WindowWidth = 800;
    constexpr int32_t WindowHeight = 600;

    const char* GetCurrentRendererName(Renderer::SceneRendererDX12& hardwareRenderer, Renderer::SceneRenderer* renderer)
    {
        return renderer == &hardwareRenderer ? "Hardware Rasterizer" : "Software Rasterizer";
    }

    void HandleKeyDown(const std::array<bool, 256>& KeyDown, Renderer::Scene& scene)
    {
        Renderer::Vec forward = Renderer::CameraTransform(scene.camera) * scene.camera.forward;
        Renderer::Vec left = Renderer::CameraTransform(scene.camera) * scene.camera.left;

        if (KeyDown[UpKey])
        {
            scene.camera.pitch -= 0.01f;
            if (scene.camera.pitch < 0)
            {
                scene.camera.pitch += static_cast<float>(M_PI) * 2;
            }
        }

        if (KeyDown[DownKey])
        {
            scene.camera.pitch += 0.01f;
            if (scene.camera.pitch > static_cast<float>(M_PI) * 2)
            {
                scene.camera.pitch -= static_cast<float>(M_PI) * 2;
            }
        }

        if (KeyDown[RightKey])
        {
            scene.camera.yaw -= 0.01f;
            if (scene.camera.yaw < 0)
            {
                scene.camera.yaw += static_cast<float>(M_PI) * 2;
            }
        }

        if (KeyDown[LeftKey])
        {
            scene.camera.yaw += 0.01f;
            if (scene.camera.yaw > static_cast<float>(M_PI) * 2)
            {
                scene.camera.yaw -= static_cast<float>(M_PI) * 2;
            }
        }

        if (KeyDown[WKey])
        {
            scene.camera.position = scene.camera.position + forward * 0.1f;
        }

        if (KeyDown[AKey])
        {
            scene.camera.position = scene.camera.position + left * 0.1f;
        }

        if (KeyDown[SKey])
        {
            scene.camera.position = scene.camera.position - forward * 0.1f;
        }

        if (KeyDown[DKey])
        {
            scene.camera.position = scene.camera.position - left * 0.1f;
        }
    }

    void HandleKeyUp(uint8_t code, HWND hWnd, Renderer::SceneRendererDX12& hardwareRenderer, Renderer::SceneRendererSoftware& softwareRenderer, Renderer::SceneRenderer*& renderer)
    {
        if (code == RKey)
        {
            if (renderer == &hardwareRenderer)
            {
                renderer = &softwareRenderer;
            }
            else
            {
                renderer = &hardwareRenderer;
            }

            SetWindowTextA(hWnd, GetCurrentRendererName(hardwareRenderer, renderer));
        }
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    static std::string SolutionDir = _SOLUTIONDIR;
    static std::string AssetsDir = SolutionDir + "assets\\";

    static std::array<bool, 256> KeyDown = {};

    static HDC screenContext = 0;
    static BITMAPINFO backBufferInfo = {
        .bmiHeader = {
            .biSize = sizeof(backBufferInfo.bmiHeader),
            .biWidth = WindowWidth,
            .biHeight = WindowHeight,
            .biPlanes = 1,
            .biBitCount = sizeof(uint32_t) * CHAR_BIT,
            .biCompression = BI_RGB
        }
    };

    static Renderer::SceneRendererSoftware softwareRenderer;
    static Renderer::DeviceDX12 device;
    static Renderer::SceneRendererDX12 hardwareRenderer(AssetsDir + "color.hlsl", device);
    static Renderer::ImguiRenderer imguiRenderer(device, WindowWidth, WindowHeight, hWnd);
    static Renderer::SceneRenderer* renderer = &hardwareRenderer;
    static Renderer::Scene scene;
    static std::vector<uint32_t> backBuffer(WindowWidth * WindowHeight);

    switch (uMsg)
    {
        case WM_CREATE:
        {
            ImGui_ImplWin32_Init(hWnd);
            NOT_FAILED(screenContext = GetDC(hWnd), 0);
            NOT_FAILED(Renderer::Load(AssetsDir + "cars\\scene.sce", scene), false);
            SetWindowTextA(hWnd, GetCurrentRendererName(hardwareRenderer, renderer));
            return 0;
        }
        case WM_KEYDOWN:
        {
            uint8_t code = static_cast<uint8_t>(wParam);
            KeyDown[code] = true;

            return 0;
        }
        case WM_KEYUP:
        {
            uint8_t code = static_cast<uint8_t>(wParam);
            KeyDown[code] = false;

            HandleKeyUp(code, hWnd, hardwareRenderer, softwareRenderer, renderer);
            return 0;
        }
        case WM_PAINT:
        {
            HandleKeyDown(KeyDown, scene);

            ImGui_ImplWin32_NewFrame();
            imguiRenderer.BeginRender();

            ImGui::NewFrame();

            ImGui::ShowDemoWindow();

            {
                static float f = 0.0f;
                static int counter = 0;

                ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

                if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    counter++;

                ImGui::SameLine();
                ImGui::Text("counter = %d", counter);

                ImGui::End();
            }


            Renderer::Texture result(WindowWidth, WindowHeight);
            renderer->Render(scene, result);

            for (size_t i = 0; i < result.GetSize(); i++)
            {
                // invert texture along y axis and shift alpha outside
                backBuffer[result.GetWidth() * (result.GetHeight() - 1 - (i / result.GetWidth())) + i % result.GetWidth()] = (result.GetColor(i).rgba >> 8);
            }

            imguiRenderer.EndRender();

            // swap buffers
            //StretchDIBits(
            //    screenContext,
            //    0,
            //    0,
            //    WindowWidth,
            //    WindowHeight,
            //    0,
            //    0,
            //    WindowWidth,
            //    WindowHeight,
            //    backBuffer.data(),
            //    &backBufferInfo,
            //    DIB_RGB_COLORS,
            //    SRCCOPY
            //);

            return 0;
        }
        case WM_DESTROY:
        {
            ReleaseDC(hWnd, screenContext); // todo.pavelza: why is HRESULT == 1? (S_FALSE|Operation successful but returned no results)
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
            nullptr,
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