#include <Windows.h>
#include <stdint.h>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>

using namespace std;

static uint32_t GameWidth = 800;
static uint32_t GameHeight = 600;

static int32_t WindowWidth;
static int32_t WindowHeight;
static BITMAPINFO BackBufferInfo;
static void* BackBuffer;

#define NOT_FAILED(call, failureCode) \
    if ((call) == failureCode) \
    { \
         assert(false); \
         exit(1); \
    } \

struct Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

void DrawPixel(float x, float y, Color color)
{
    assert(GameWidth > 0);
    assert(GameHeight > 0);
    assert(-1 <= x && x <= 1);
    assert(-1 <= y && y <= 1);
    assert(0x00 <= color.r && color.r <= 0xFF);
    assert(0x00 <= color.g && color.g <= 0xFF);
    assert(0x00 <= color.b && color.b <= 0xFF);
    assert(BackBuffer);

    uint32_t* backBuffer = (uint32_t*)BackBuffer;

    uint32_t i = static_cast<uint32_t>((GameWidth - 1) * ((x + 1) / 2.0f));
    uint32_t j = static_cast<uint32_t>((GameHeight - 1) * ((y + 1) / 2.0f));

    uint32_t red = color.r;
    uint32_t green = color.g;
    uint32_t blue = color.b;

    uint32_t finalColor = 0U;
    finalColor |= red << 16;
    finalColor |= green << 8;
    finalColor |= blue << 0;

    backBuffer[j * GameWidth + i] = finalColor;
}

float StarSpeed = 0.01f;
int StarsCount = 500;
int FPS = 30;

vector<float> StarX(StarsCount);
vector<float> StarY(StarsCount);
vector<float> StarZ(StarsCount);

float Random() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

void CreateStar(int i) {
    StarX[i] = Random() * 2 - 1;
    StarY[i] = Random() * 2 - 1;
    StarZ[i] = Random() + 0.1f;
}

bool IsWithinScreen(float x, float y) {
    return (-1 <= x && x <= 1) && (-1 <= y && y <= 1);
}

bool IsTooCloseToCamera(float z) {
    return z <= 0;
}

void Clear(Color color)
{
    for (uint32_t i = 0; i <= GameWidth; i++)
    {
        for (uint32_t j = 0; j <= GameHeight; j++)
        {
            DrawPixel(2.0f / GameWidth * i - 1.0f, 2.0f / GameHeight * j - 1.0f, color);
        }
    }
}

void RenderStars(chrono::milliseconds delta) 
{
    for (int i = 0; i < StarsCount; i++) {
        StarZ[i] -= StarSpeed;

        float z = StarZ[i];
        if (IsTooCloseToCamera(z)) {
            CreateStar(i);
            continue;
        }

        float x = StarX[i] / z;
        float y = StarY[i] / z;

        if (!IsWithinScreen(x, y)) {
            CreateStar(i);
            continue;
        }

        DrawPixel(x, y, { 255, 255, 255 });
    }
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
        NOT_FAILED(BackBuffer = VirtualAlloc(0, GameWidth * GameHeight * sizeof(uint32_t), MEM_COMMIT, PAGE_READWRITE), 0);

        for (int i = 0; i < StarsCount; i++) {
            CreateStar(i);
        }

        auto frameTime = chrono::milliseconds(1000 / FPS);

        while (isRunning)
        {
            auto start = chrono::steady_clock::now();

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

            Clear({ 0, 0, 0 });
            RenderStars(frameTime);

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

            auto finish = chrono::steady_clock::now();
            auto timeLeft = frameTime - (finish - start);

            if (timeLeft > 0ms) {
                this_thread::sleep_for(timeLeft);
            }
        }

        NOT_FAILED(VirtualFree(BackBuffer, 0, MEM_RELEASE), 0);
        NOT_FAILED(ReleaseDC(window, screenContext), 1);
    }

    return 0;
}