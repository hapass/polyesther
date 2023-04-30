#define _USE_MATH_DEFINES

#include <Windows.h>

#include <renderer/math.h>
#include <renderer/rendererdx12.h>
#include <renderer/color.h>
#include <renderer/scene.h>
#include <renderer/texture.h>
#include <renderer/utils.h>

static int32_t WindowWidth = 800;
static int32_t WindowHeight = 600;
static BITMAPINFO BackBufferInfo;
static uint32_t* BackBuffer;

static bool KeyDown[256];

static uint8_t WKey = 87;
static uint8_t AKey = 65;
static uint8_t SKey = 83;
static uint8_t DKey = 68;

static uint8_t UpKey = 38;
static uint8_t LeftKey = 37;
static uint8_t DownKey = 40;
static uint8_t RightKey = 39;

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

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    bool isRunning = true;

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

            AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);

            HWND window;
            NOT_FAILED(window = CreateWindowExW(
                0,
                L"MainWindow",
                L"Software Rasterizer",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                clientArea.right - clientArea.left,
                clientArea.bottom - clientArea.top,
                0,
                0,
                hInstance,
                0
            ), 0);

            Renderer::Scene scene;
            Renderer::Load("scene.sce", scene);
            const Renderer::Model& model = scene.models[0];

            scene.camera.position.z = 2;
            scene.camera.position.x = 0;
            scene.camera.position.y = 0;
            scene.camera.pitch = 0.0f;
            scene.camera.yaw = 0.0f;
            scene.camera.forward = Renderer::Vec{ 0.0f, 0.0f, -1.0f, 0.0f };
            scene.camera.left = Renderer::Vec{ -1.0f, 0.0f, 0.0f, 0.0f };
            scene.light.position = Renderer::Vec{ 100.0f, 100.0f, 100.0f, 1.0f };

            Renderer::RendererDX12 renderer;

            while (isRunning)
            {
                MSG message;
                if (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        OutputDebugString(L"Quit message reached loop.");
                        isRunning = false;
                    }
                    else
                    {
                        TranslateMessage(&message);
                        DispatchMessageW(&message);
                    }
                }

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

                // calculate light's position in view space
                scene.light.position_view = Renderer::ViewTransform(scene.camera) * scene.light.position;

                Renderer::Texture result(WindowWidth, WindowHeight);
                renderer.Render(scene, result);
            }
        }
    }
    catch (std::exception&)
    {
        DebugBreak();
    }

    return 0;
}