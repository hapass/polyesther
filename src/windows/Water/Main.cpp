#define _USE_MATH_DEFINES

#include <Windows.h>
#include <stdint.h>
#include <cassert>
#include <thread>
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>

/*
* TODO: Remove scanline buffer.
*/

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

void DrawPixel(int32_t x, int32_t y, Color color)
{
    assert(GameWidth > 0);
    assert(GameHeight > 0);
    assert(0 <= x && x < GameWidth);
    assert(0 <= y && y < GameHeight);
    assert(BackBuffer);
    BackBuffer[y * GameWidth + x] = color.rgb;
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

struct Matrix
{
    Matrix()
    {
        for (uint32_t i = 0; i < 16U; i++)
        {
            m[i] = 0;
        }
    }

    float m[16U];
};

Matrix rotateZ(float alpha)
{
    Matrix m;

    //col 1
    m.m[0] = cosf(alpha);
    m.m[4] = sinf(alpha);
    m.m[8] = 0.0f;
    m.m[12] = 0.0f;

    //col 2
    m.m[1] = -sinf(alpha);
    m.m[5] = cosf(alpha);
    m.m[9] = 0.0f;
    m.m[13] = 0.0f;

    //col 3
    m.m[2] = 0.0f;
    m.m[6] = 0.0f;
    m.m[10] = 1.0f;
    m.m[14] = 0.0f;

    //col 4
    m.m[3] = 0.0f;
    m.m[7] = 0.0f;
    m.m[11] = 0.0f;
    m.m[15] = 1.0f;

    return m;
}

Matrix rotateY(float alpha)
{
    Matrix m;

    //col 1
    m.m[0] = cosf(alpha);
    m.m[4] = 0.0f;
    m.m[8] = -sinf(alpha);
    m.m[12] = 0.0f;

    //col 2
    m.m[1] = 0.0f;
    m.m[5] = 1.0f;
    m.m[9] = 0.0f;
    m.m[13] = 0.0f;

    //col 3
    m.m[2] = sinf(alpha);
    m.m[6] = 0.0f;
    m.m[10] = cosf(alpha);
    m.m[14] = 0.0f;

    //col 4
    m.m[3] = 0.0f;
    m.m[7] = 0.0f;
    m.m[11] = 0.0f;
    m.m[15] = 1.0f;

    return m;
}

Matrix translate(float x, float y, float z)
{
    Matrix m;

    //col 1
    m.m[0] = 1.0f;
    m.m[4] = 0.0f;
    m.m[8] = 0.0f;
    m.m[12] = 0.0f;

    //col 2
    m.m[1] = 0.0f;
    m.m[5] = 1.0f;
    m.m[9] = 0.0f;
    m.m[13] = 0.0f;

    //col 3
    m.m[2] = 0.0f;
    m.m[6] = 0.0f;
    m.m[10] = 1.0f;
    m.m[14] = 0.0f;

    //col 4
    m.m[3] = x;
    m.m[7] = y;
    m.m[11] = z;
    m.m[15] = 1.0f;

    return m;
}

float Far = 400.0f;
float Near = 1.0f;

Matrix perspective()
{
    float farPlane = Far;
    float nearPlane = Near;
    float halfFieldOfView = 45 * (static_cast<float>(M_PI) / 180);
    float aspect = static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight);

    Matrix m;

    //col 1
    m.m[0] = 1 / (tan(halfFieldOfView) * aspect);
    m.m[4] = 0.0f;
    m.m[8] = 0.0f;
    m.m[12] = 0.0f;

    //col 2
    m.m[1] = 0.0f;
    m.m[5] = 1 / tan(halfFieldOfView);
    m.m[9] = 0.0f;
    m.m[13] = 0.0f;

    //col 3
    m.m[2] = 0.0f;
    m.m[6] = 0.0f;
    m.m[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    m.m[14] = -1.0f;

    //col 4
    m.m[3] = 0.0f;
    m.m[7] = 0.0f;
    m.m[11] = 2 * farPlane * nearPlane / (farPlane - nearPlane);
    m.m[15] = 0.0f;

    return m;
}

struct Vec
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

float dot(Vec a, Vec b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vec operator+(Vec a, Vec b)
{
    return Vec { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

Vec operator-(Vec b)
{
    return Vec { -b.x, -b.y, -b.z, -b.w };
}

Vec operator-(Vec a, Vec b)
{
    return a + (-b);
}

Vec operator*(Matrix m, Vec v)
{
    float x = dot(Vec { m.m[0],  m.m[1],  m.m[2],  m.m[3] }, v);
    float y = dot(Vec { m.m[4],  m.m[5],  m.m[6],  m.m[7] }, v);
    float z = dot(Vec { m.m[8],  m.m[9],  m.m[10], m.m[11] }, v);
    float w = dot(Vec { m.m[12], m.m[13], m.m[14], m.m[15] }, v);
    return Vec { x, y, z, w };
}

struct Edge
{
    Edge(Vec b, Vec e): begin(b), end(e)
    {
        pixelYBegin = static_cast<int32_t>(ceil(begin.y));
        pixelYEnd = static_cast<int32_t>(ceil(end.y));

        Vec distance = GetDistance();
        stepX = distance.y > 0.0f ? distance.x / distance.y : 0.0f;
        currentX = begin.x + (ceil(begin.y) - begin.y) * stepX;
    }

    Vec GetDistance() const
    {
        return end - begin;
    }

    int32_t Step()
    {
        currentX += stepX;
        return static_cast<int32_t>(ceil(currentX));
    }

    int32_t pixelYBegin;
    int32_t pixelYEnd;

    float stepX;
    float currentX;

    Vec begin;
    Vec end;
};

void DrawScanLine(Edge& minMax, Edge& other)
{
    for (int32_t y = other.pixelYBegin; y < other.pixelYEnd; y++)
    {
        int32_t pixelXBegin = minMax.Step();
        int32_t pixelXEnd = other.Step();

        if (pixelXBegin > pixelXEnd)
        {
            int32_t temp = pixelXBegin;
            pixelXBegin = pixelXEnd;
            pixelXEnd = temp;
        }

        for (int32_t x = pixelXBegin; x < pixelXEnd; x++)
        {
            DrawPixel(x, y, Color::White);
        }
    }
}

void DrawTriangle(Vec a, Vec b, Vec c)
{
    std::array<Vec, 3> vertices { a, b, c };

    for (Vec& v : vertices)
    {
        // TODO.PAVELZA: Need to clip coordinates before scanline buffer phaze.
        assert(-1.0f <= v.x && v.x <= 1.0f);
        assert(-1.0f <= v.y && v.y <= 1.0f);
        assert(-1.0f <= v.z && v.z <= 1.0f);
        v.x = (GameWidth - 1) * ((v.x + 1) / 2.0f);
        v.y = (GameHeight - 1) * ((v.y + 1) / 2.0f);
    }

    std::sort(std::begin(vertices), std::end(vertices), [](const Vec& lhs, const Vec& rhs) { return lhs.y < rhs.y; });

    Edge minMax(vertices[0], vertices[2]);
    Edge minMiddle(vertices[0], vertices[1]);
    Edge middleMax(vertices[1], vertices[2]);

    DrawScanLine(minMax, minMiddle);
    DrawScanLine(minMax, middleMax); 
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

        float multiplier = 0.0f;

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

            float zCoord = 100.f;
            std::array<Vec, 3> vertices{ Vec{ -20, 0, zCoord, 1 }, Vec{ 20, 10, zCoord, 1 }, Vec{ 0, -50, zCoord, 1 } };
            multiplier += 0.01f;
            if (multiplier > 2.0f)
            {
                multiplier = 0.0f;
            }
            for (Vec& v : vertices)
            {
                v = translate(0.0f, 0.0f, -zCoord) * v;
                v = rotateY(static_cast<float>(M_PI) * multiplier) * v;
                v = translate(0.0f, 0.0f, zCoord) * v;
                v = perspective() * v;
                v.x /= v.w;
                v.y /= v.w;
                v.z /= v.w;
                v.w = 1.0f;
            }

            ClearScreen(Color::Black);
            DrawTriangle(vertices[0], vertices[1], vertices[2]);

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