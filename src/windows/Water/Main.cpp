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
    OS_CALL(BackBuffer = VirtualAlloc(0, BackBufferInfo.bmiHeader.biWidth * BackBufferInfo.bmiHeader.biHeight * sizeof(uint32_t), MEM_COMMIT, PAGE_READWRITE));
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

        BackBufferInfo.bmiHeader.biSize = sizeof(BackBufferInfo.bmiHeader);
        BackBufferInfo.bmiHeader.biWidth = 1920;
        BackBufferInfo.bmiHeader.biHeight = 1080;
        BackBufferInfo.bmiHeader.biPlanes = 1;
        BackBufferInfo.bmiHeader.biBitCount = 32;
        BackBufferInfo.bmiHeader.biCompression = BI_RGB;
        CreateBackBuffer();

        //TODO.PAVELZA: cannot move this inside the loop. Bad window handle in case of closed? Should recalculate in window handler?
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

            uint32_t* color = (uint32_t*)BackBuffer;
            for (uint32_t i = 0; i < BackBufferInfo.bmiHeader.biWidth; i++)
            {
                for (uint32_t j = 0; j < BackBufferInfo.bmiHeader.biHeight; j++)
                {
                    //xxGGRRBB
                    color[j * BackBufferInfo.bmiHeader.biWidth + i] = 0x000000FFU;
                }
            }

            StretchDIBits(
                screenContext,
                drawingArea.left,
                drawingArea.top,
                drawingArea.right - drawingArea.left,
                drawingArea.bottom - drawingArea.top,
                0,
                0,
                BackBufferInfo.bmiHeader.biWidth,
                BackBufferInfo.bmiHeader.biHeight,
                BackBuffer,
                &BackBufferInfo,
                DIB_RGB_COLORS,
                SRCCOPY
            );
        }

        ReleaseDC(window, screenContext);
        DestroyBackBuffer();
    }

    return 0;
}