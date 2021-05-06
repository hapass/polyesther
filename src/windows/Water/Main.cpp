#include <Windows.h>
#include <stdint.h>
#include <cassert>

static RECT ScreenSize;

static int32_t WindowWidth;
static int32_t WindowHeight;
static BITMAPINFO BackBufferInfo;
static void* BackBuffer;

#define NOT_FAILED(call, failureCode) \
    if ((call) == failureCode) \
    { \
         assert(false, "Windows API call failed."); \
         exit(1); \
    } \

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
    MainWindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    MainWindowClass.lpfnWndProc = MainWindowProc;
    MainWindowClass.hInstance = hInstance;
    MainWindowClass.lpszClassName = L"MainWindow";

    if (RegisterClassW(&MainWindowClass))
    {
        HWND window;
        NOT_FAILED(window = CreateWindowExW(
            0,
            L"MainWindow",
            L"Software Rasterizer",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            hInstance,
            0
        ), 0);

        HDC screenContext;
        NOT_FAILED(screenContext = GetDC(window), 0);

        BackBufferInfo.bmiHeader.biSize = sizeof(BackBufferInfo.bmiHeader);
        BackBufferInfo.bmiHeader.biWidth = 1920;
        BackBufferInfo.bmiHeader.biHeight = 1080;
        BackBufferInfo.bmiHeader.biPlanes = 1;
        BackBufferInfo.bmiHeader.biBitCount = sizeof(uint32_t) * CHAR_BIT;
        BackBufferInfo.bmiHeader.biCompression = BI_RGB;
        NOT_FAILED(BackBuffer = VirtualAlloc(0, BackBufferInfo.bmiHeader.biWidth * BackBufferInfo.bmiHeader.biHeight * sizeof(uint32_t), MEM_COMMIT, PAGE_READWRITE), 0);

        uint32_t offset = 0x00U;
        int32_t direction = 1;
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

            uint32_t* color = (uint32_t*)BackBuffer;
            for (uint32_t i = 0; i < BackBufferInfo.bmiHeader.biWidth; i++)
            {
                for (uint32_t j = 0; j < BackBufferInfo.bmiHeader.biHeight; j++)
                {
                    uint32_t red = (static_cast<float>(j) / BackBufferInfo.bmiHeader.biHeight) * 0xFF;
                    uint32_t green = offset;
                    uint32_t blue = (static_cast<float>(i) / BackBufferInfo.bmiHeader.biWidth) * 0xFF;

                    uint32_t finalColor = 0U;
                    finalColor |= red << 16;
                    finalColor |= green << 8;
                    finalColor |= blue << 0;

                    color[j * BackBufferInfo.bmiHeader.biWidth + i] = finalColor;
                }
            }

            StretchDIBits(
                screenContext,
                0,
                0,
                WindowWidth,
                WindowHeight,
                0,
                0,
                BackBufferInfo.bmiHeader.biWidth,
                BackBufferInfo.bmiHeader.biHeight,
                BackBuffer,
                &BackBufferInfo,
                DIB_RGB_COLORS,
                SRCCOPY
            );

            if (offset == 0xFFU)
            {
                direction = -1;
            }

            if (offset == 0x00U)
            {
                direction = 1;
            }

            offset += direction;
        }

        NOT_FAILED(VirtualFree(BackBuffer, 0, MEM_RELEASE), 0);
        NOT_FAILED(ReleaseDC(window, screenContext), 1);
    }

    return 0;
}