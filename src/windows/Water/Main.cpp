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
         assert(false, "Windows API call failed."); \
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
    assert(GameWidth > 0, "GameWidth should be greater than 0.");
    assert(GameHeight > 0, "GameHeight should be greater than 0.");
    assert(-1 <= x && x <= 1, "X should be in clip space.");
    assert(-1 <= y && y <= 1, "Y should be in clip space.");
    assert(0x00 <= color.r && color.r <= 0xFF, "Red should be between 0x00 and 0xFF.");
    assert(0x00 <= color.g && color.g <= 0xFF, "Green should be between 0x00 and 0xFF.");
    assert(0x00 <= color.b && color.b <= 0xFF, "Blue should be between 0x00 and 0xFF.");
    assert(BackBuffer, "BackBuffer should exist before drawing.");

    uint32_t* backBuffer = (uint32_t*)BackBuffer;

    uint32_t i = (GameWidth - 1) * ((x + 1) / 2.0f);
    uint32_t j = (GameHeight - 1) * ((y + 1) / 2.0f);

    uint32_t red = color.r;
    uint32_t green = color.g;
    uint32_t blue = color.b;

    uint32_t finalColor = 0U;
    finalColor |= red << 16;
    finalColor |= green << 8;
    finalColor |= blue << 0;

    backBuffer[j * GameWidth + i] = finalColor;
}

float star_speed = 0.01;
int stars_count = 200;
int fps = 25;

vector<float> star_x(stars_count);
vector<float> star_y(stars_count);
vector<float> star_z(stars_count);

float generate_random() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

void create_star(int i) {
    star_x[i] = generate_random() * 2 - 1;
    star_y[i] = generate_random() * 2 - 1;
    star_z[i] = generate_random() + 0.1;
}

bool within_screen(float x, float y) {
    return (-1 <= x && x <= 1) &&
        (-1 <= y && y <= 1);
}

bool too_close(float z) {
    return z <= 0;
}

void draw_stars(chrono::milliseconds delta) 
{
    for (uint32_t i = 0; i <= GameWidth; i++)
        for (uint32_t j = 0; j <= GameHeight; j++)
            DrawPixel(
                2.0f / GameWidth * i - 1.0f,
                2.0f / GameHeight * j - 1.0f,
                { 0, 0, 0 }
            );

    auto delta_seconds = chrono::duration_cast<chrono::seconds>(delta);
    for (int i = 0; i < stars_count; i++) {
        star_z[i] -= star_speed;

        float z = star_z[i];
        if (too_close(z)) {
            create_star(i);
            continue;
        }

        float x = (star_x[i] / z);
        float y = (star_y[i] / z);

        if (!within_screen(x, y)) {
            create_star(i);
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

        for (int i = 0; i < stars_count; i++) {
            create_star(i);
        }

        auto frame_time = chrono::milliseconds(1000 / fps);

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

            draw_stars(frame_time);

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
            auto time_left = frame_time - (finish - start);

            if (time_left > 0ms) {
                this_thread::sleep_for(time_left);
            }
        }

        NOT_FAILED(VirtualFree(BackBuffer, 0, MEM_RELEASE), 0);
        NOT_FAILED(ReleaseDC(window, screenContext), 1);
    }

    return 0;
}