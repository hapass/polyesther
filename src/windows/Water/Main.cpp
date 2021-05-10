#include <Windows.h>
#include <stdint.h>
#include <cassert>
#include <thread>
#include <chrono>

using namespace std;

static const uint32_t GameWidth = 800;
static const uint32_t GameHeight = 600;
static const uint32_t FPS = 30;

static int32_t WindowWidth;
static int32_t WindowHeight;
static BITMAPINFO BackBufferInfo;
static uint32_t* BackBuffer;

#define NOT_FAILED(call, failureCode) \
    if ((call) == failureCode) \
    { \
         assert(false); \
         exit(1); \
    } \

struct Color
{
    Color(uint8_t r, uint8_t g, uint8_t b): rgb(0u)
    {
        rgb |= r << 16;
        rgb |= g << 8;
        rgb |= b << 0;
    }

    uint32_t rgb;

    static Color Black;
    static Color White;
};

Color Color::Black = Color(0u, 0u, 0u);
Color Color::White = Color(255u, 255u, 255u);

void DrawPixel(float x, float y, Color color)
{
    assert(GameWidth > 0);
    assert(GameHeight > 0);
    assert(-1 <= x && x <= 1);
    assert(-1 <= y && y <= 1);
    assert(BackBuffer);

    uint32_t i = static_cast<uint32_t>((GameWidth - 1) * ((x + 1) / 2.0f));
    uint32_t j = static_cast<uint32_t>((GameHeight - 1) * ((y + 1) / 2.0f));

    BackBuffer[j * GameWidth + i] = color.rgb;
}

void ClearScreen(Color color)
{
    assert(GameWidth > 0);
    assert(GameHeight > 0);
    assert(BackBuffer);

    for (uint32_t i = 0; i < GameWidth * GameHeight; i++)
    {
        BackBuffer[i] = color.rgb;
    }
}

static int32_t ScanLineBuffer[2 * GameHeight];


void DrawScanLineBuffer()
{
    for (int32_t line = 0; line < GameHeight; line++)
    {
        int32_t columnMin = ScanLineBuffer[line * 2];
        int32_t columnMax = ScanLineBuffer[line * 2 + 1];

        if (columnMin == 0 || columnMax == 0) continue;

        for (int32_t column = columnMin; column < columnMax; column++)
        {
            float x = (2.0f / GameWidth) * column - 1.0f;
            float y = -((2.0f / GameHeight) * line - 1.0f);
            DrawPixel(x, y, Color::White);
        }
    }
}

struct Vert
{
    int32_t x;
    int32_t y;
};

enum class ScanLineBufferSide
{
    Left,
    Right
};

void AddTriangleSideToScanLineBuffer(Vert begin, Vert end, ScanLineBufferSide side)
{
    float stepX = static_cast<float>(end.x - begin.x) / static_cast<float>(end.y - begin.y);
    float x = static_cast<float>(begin.x);

    for (int32_t i = begin.y; i < end.y; i++)
    {
        int32_t bufferOffset = side == ScanLineBufferSide::Left ? 0 : 1;
        ScanLineBuffer[i * 2 + bufferOffset] = static_cast<int32_t>(x);
        x += stepX;
    }
}

void DrawTriangle(Vert min, Vert mid, Vert max)
{
    AddTriangleSideToScanLineBuffer(min, mid, ScanLineBufferSide::Right);
    AddTriangleSideToScanLineBuffer(mid, max, ScanLineBufferSide::Right);
    AddTriangleSideToScanLineBuffer(min, max, ScanLineBufferSide::Left);
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
            GameWidth,
            GameHeight,
            0,
            0,
            hInstance,
            0
        ), 0);

        HDC screenContext;
        NOT_FAILED(screenContext = GetDC(window), 0);

        BackBufferInfo.bmiHeader.biSize = sizeof(BackBufferInfo.bmiHeader);
        BackBufferInfo.bmiHeader.biWidth = GameWidth;
        BackBufferInfo.bmiHeader.biHeight = GameHeight;
        BackBufferInfo.bmiHeader.biPlanes = 1;
        BackBufferInfo.bmiHeader.biBitCount = sizeof(uint32_t) * CHAR_BIT;
        BackBufferInfo.bmiHeader.biCompression = BI_RGB;
        NOT_FAILED(BackBuffer = (uint32_t*)VirtualAlloc(0, GameWidth * GameHeight * sizeof(uint32_t), MEM_COMMIT, PAGE_READWRITE), 0);

        auto frameExpectedTime = chrono::milliseconds(1000 / FPS);

        while (isRunning)
        {
            auto frameStart = chrono::steady_clock::now();

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

            ClearScreen(Color::Black);
            DrawTriangle({ 100, 100 }, { 150, 200 }, { 80, 300 });
            DrawScanLineBuffer();

            //swap buffers
            StretchDIBits(
                screenContext,
                0,
                0,
                WindowWidth,
                WindowHeight,
                0,
                0,
                GameWidth,
                GameHeight,
                BackBuffer,
                &BackBufferInfo,
                DIB_RGB_COLORS,
                SRCCOPY
            );

            auto frameActualTime = frameStart - chrono::steady_clock::now();
            auto timeLeft = frameExpectedTime - frameActualTime;

            if (timeLeft > 0ms) 
            {
                this_thread::sleep_for(timeLeft);
            }
        }

        NOT_FAILED(VirtualFree(BackBuffer, 0, MEM_RELEASE), 0);
        NOT_FAILED(ReleaseDC(window, screenContext), 1);
    }

    return 0;
}