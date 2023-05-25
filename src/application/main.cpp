#define _USE_MATH_DEFINES

#include <Windows.h>

#include <renderer/math.h>
#include <renderer/rendererdx12.h>
#include <renderer/renderersoftware.h>
#include <renderer/color.h>
#include <renderer/scene.h>
#include <renderer/texture.h>
#include <renderer/utils.h>

namespace
{
    int32_t WindowWidth = 800;
    int32_t WindowHeight = 600;

    bool KeyDown[256];

    uint8_t WKey = 87;
    uint8_t AKey = 65;
    uint8_t SKey = 83;
    uint8_t DKey = 68;

    uint8_t UpKey = 38;
    uint8_t LeftKey = 37;
    uint8_t DownKey = 40;
    uint8_t RightKey = 39;

    std::string SolutionDir = _SOLUTIONDIR;
    std::string AssetsDir = SolutionDir + "assets\\";
}

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SIZE:
    {
        RECT clientRect;
        NOT_FAILED(GetClientRect(hWnd, &clientRect), 0);
        WindowWidth = clientRect.right - clientRect.left;
        WindowHeight = clientRect.bottom - clientRect.top;
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    case WM_KEYDOWN:
    {
        uint8_t code = static_cast<uint8_t>(wParam);
        KeyDown[code] = true;
        break;
    }
    case WM_KEYUP:
    {
        uint8_t code = static_cast<uint8_t>(wParam);
        KeyDown[code] = false;
        break;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void HandleInput(Renderer::Scene& scene)
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

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    bool isRunning = true;

    Utils::DebugUtils::GetInstance().AddOutput([](const std::string& message) {
        OutputDebugStringA(message.c_str());
    });

    WNDCLASS MainWindowClass = {};
    MainWindowClass.lpfnWndProc = MainWindowProc;
    MainWindowClass.hInstance = hInstance;
    MainWindowClass.lpszClassName = L"MainWindow";

    try
    {
        if (RegisterClassW(&MainWindowClass))
        {
            RECT clientArea;
            clientArea.left = 0;
            clientArea.top = 0;
            clientArea.right = WindowWidth;
            clientArea.bottom = WindowHeight;

            AdjustWindowRect(&clientArea, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 0);

            HWND window;
            NOT_FAILED(window = CreateWindowExW(
                0,
                L"MainWindow",
                L"Software Rasterizer",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                clientArea.right - clientArea.left,
                clientArea.bottom - clientArea.top,
                0,
                0,
                hInstance,
                0
            ), 0);

            HDC screenContext;
            NOT_FAILED(screenContext = GetDC(window), 0);

            BITMAPINFO BackBufferInfo;
            BackBufferInfo.bmiHeader.biSize = sizeof(BackBufferInfo.bmiHeader);
            BackBufferInfo.bmiHeader.biWidth = WindowWidth;
            BackBufferInfo.bmiHeader.biHeight = WindowHeight;
            BackBufferInfo.bmiHeader.biPlanes = 1;
            BackBufferInfo.bmiHeader.biBitCount = sizeof(uint32_t) * CHAR_BIT;
            BackBufferInfo.bmiHeader.biCompression = BI_RGB;

            uint32_t* BackBuffer = nullptr;
            NOT_FAILED(BackBuffer = (uint32_t*)VirtualAlloc(0, WindowWidth * WindowHeight * sizeof(uint32_t), MEM_COMMIT, PAGE_READWRITE), 0);

            Renderer::Scene scene;
            Renderer::Load(AssetsDir + "cars\\scene.sce", scene);

            scene.camera.position.z = 2;
            scene.camera.position.x = 0;
            scene.camera.position.y = 0;
            scene.camera.pitch = 0.0f;
            scene.camera.yaw = 0.0f;
            scene.camera.forward = Renderer::Vec{ 0.0f, 0.0f, -1.0f, 0.0f };
            scene.camera.left = Renderer::Vec{ -1.0f, 0.0f, 0.0f, 0.0f };
            scene.light.position = Renderer::Vec{ 100.0f, 100.0f, 100.0f, 1.0f };

            Renderer::RendererSoftware renderer;
            //Renderer::RendererDX12 renderer(AssetsDir + "color.hlsl");

            while (isRunning)
            {
                MSG message;
                if (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        LOG("Quit message reached loop.");
                        isRunning = false;
                    }
                    else
                    {
                        TranslateMessage(&message);
                        DispatchMessageW(&message);
                    }
                }

                HandleInput(scene);

                Renderer::Texture result(WindowWidth, WindowHeight);
                renderer.Render(scene, result);

                for (size_t i = 0; i < result.GetSize(); i++)
                {
                    // invert texture along y axis and shift alpha outside
                    BackBuffer[result.GetWidth() * (result.GetHeight() - 1 - (i / result.GetWidth())) + i % result.GetWidth()] = (result.GetColor(i).rgba >> 8);
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
                    BackBuffer,
                    &BackBufferInfo,
                    DIB_RGB_COLORS,
                    SRCCOPY
                );
            }

            NOT_FAILED(VirtualFree(BackBuffer, 0, MEM_RELEASE), 0);
            NOT_FAILED(ReleaseDC(window, screenContext), 1);
        }
    }
    catch (std::exception&)
    {
        DebugBreak();
    }

    return 0;
}