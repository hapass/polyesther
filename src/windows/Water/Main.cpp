#include <Windows.h>
#include <stdint.h>
#include <cassert>

static BITMAPINFO BackBufferInfo;
static void* BackBuffer;

#define OS_CALL(call) \
    if (!(call)) \
    { \
         assert(false, "Windows API call failed."); \
         exit(1); \
    } \

void CreateBackBuffer()
{
    OS_CALL(BackBuffer = VirtualAlloc(0, BackBufferInfo.bmiHeader.biWidth * BackBufferInfo.bmiHeader.biHeight * (BackBufferInfo.bmiHeader.biBitCount / CHAR_BIT), MEM_COMMIT, PAGE_READWRITE));
}

void DestroyBackBuffer()
{
    if (BackBuffer)
    {
        OS_CALL(VirtualFree(BackBuffer, 0, MEM_RELEASE));
    }
}

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SIZE:
    {
        DestroyBackBuffer();
        CreateBackBuffer();
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    case WM_PAINT:
    {
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
        OS_CALL(window = CreateWindowExW(
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
        ));

        HDC screenContext;
        OS_CALL(screenContext = GetDC(window));

        RECT drawingArea;
        OS_CALL(GetClientRect(window, &drawingArea));

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

            //StretchDIBits(
            //    screenContext,
            //    drawingArea.left,
            //    drawingArea.top,
            //    drawingArea.right - drawingArea.left,
            //    drawingArea.bottom - drawingArea.top,

            //    )
        }

        ReleaseDC(window, screenContext);
    }

    return 0;
}