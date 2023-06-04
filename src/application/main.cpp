#define _USE_MATH_DEFINES

#include <Windows.h>

#include <array>

#include <renderer/math.h>
#include <renderer/rendererdx12.h>
#include <renderer/renderersoftware.h>
#include <renderer/color.h>
#include <renderer/scene.h>
#include <renderer/texture.h>
#include <renderer/utils.h>

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
}

void HandleInput(const std::array<bool, 256>& KeyPressed, const std::array<bool, 256>& KeyDown, Renderer::RendererDX12& hardwareRenderer, Renderer::RendererSoftware& softwareRenderer, Renderer::Scene& scene, Renderer::Renderer*& renderer)
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

    if (KeyPressed[RKey])
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
}

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static std::string SolutionDir = _SOLUTIONDIR;
    static std::string AssetsDir = SolutionDir + "assets\\";

    std::array<bool, 256> KeyPressed = {};
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
    static Renderer::RendererSoftware softwareRenderer;
    static Renderer::RendererDX12 hardwareRenderer(AssetsDir + "color.hlsl");
    static Renderer::Renderer* renderer = &hardwareRenderer;
    static Renderer::Scene scene;
    static std::vector<uint32_t> backBuffer(WindowWidth * WindowHeight);

    switch (uMsg)
    {
        case WM_CREATE:
        {
            NOT_FAILED(screenContext = GetDC(hWnd), 0);
            NOT_FAILED(Renderer::Load(AssetsDir + "cars\\scene.sce", scene), false);
            return 0;
        }
        case WM_KEYDOWN:
        {
            uint8_t code = static_cast<uint8_t>(wParam);
            KeyDown[code] = true;

            HandleInput(KeyPressed, KeyDown, hardwareRenderer, softwareRenderer, scene, renderer);
            SetWindowText(hWnd, renderer == &hardwareRenderer ? L"Hardware Rasterizer" : L"Software Rasterizer");

            return 0;
        }
        case WM_KEYUP:
        {
            uint8_t code = static_cast<uint8_t>(wParam);
            if (KeyDown[code])
            {
                KeyDown[code] = false;
                KeyPressed[code] = true;
            }

            HandleInput(KeyPressed, KeyDown, hardwareRenderer, softwareRenderer, scene, renderer);
            SetWindowText(hWnd, renderer == &hardwareRenderer ? L"Hardware Rasterizer" : L"Software Rasterizer");

            return 0;
        }
        case WM_PAINT:
        {
            Renderer::Texture result(WindowWidth, WindowHeight);
            renderer->Render(scene, result);

            for (size_t i = 0; i < result.GetSize(); i++)
            {
                // invert texture along y axis and shift alpha outside
                backBuffer[result.GetWidth() * (result.GetHeight() - 1 - (i / result.GetWidth())) + i % result.GetWidth()] = (result.GetColor(i).rgba >> 8);
            }

            // swap buffers
            StretchDIBits(
                screenContext,
                0,
                0,
                WindowWidth,
                WindowHeight,
                0,
                0,
                WindowWidth,
                WindowHeight,
                backBuffer.data(),
                &backBufferInfo,
                DIB_RGB_COLORS,
                SRCCOPY
            );

            return 0;
        }
        case WM_DESTROY:
        {
            NOT_FAILED(ReleaseDC(hWnd, screenContext), 1);
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

        AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW, 0);

        HWND window = CreateWindowExW(
            0,
            MainWindowClass.lpszClassName,
            nullptr,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            clientArea.right - clientArea.left,
            clientArea.bottom - clientArea.top,
            0,
            0,
            hInstance,
            0
        );

        ShowWindow(window, nShowCmd);

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