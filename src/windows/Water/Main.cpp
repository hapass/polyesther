#define _USE_MATH_DEFINES

#include <Windows.h>
#include <stdint.h>
#include <cassert>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include <array>
#include <cmath>
#include <wincodec.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/*
* World coordinates.
* Refactoring.
* Do z-buffer.
* Do clipping.
* Fix artifacts.
* Optimization.
*/

using namespace std;

static const uint32_t GameWidth = 800;
static const uint32_t GameHeight = 600;
static const uint32_t FPS = 30;

static int32_t WindowWidth;
static int32_t WindowHeight;
static BITMAPINFO BackBufferInfo;
static uint32_t* BackBuffer;

static float* ZBuffer;

static int32_t TextureWidth;
static int32_t TextureHeight;
static uint8_t* Texture;

// move all vertices 100 pixels away from camera
static float zCoord = 100.f;
static float angle = 0.0f;

struct Vec
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct VertIndex
{
    uint32_t vert;
    uint32_t text;
};

static std::vector<Vec> verticesObj;
static std::vector<float> textureX;
static std::vector<float> textureY;
static std::vector<VertIndex> indicesObj;
static uint32_t triangles_count = 0;

#define NOT_FAILED(call, failureCode) \
    if ((call) == failureCode) \
    { \
         assert(false); \
         exit(1); \
    } \

void DebugOut(const wchar_t* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    wchar_t dbg_out[4096];
    vswprintf_s(dbg_out, fmt, argp);
    va_end(argp);
    OutputDebugString(dbg_out);
}

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
        ZBuffer[i] = 2.0f;
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

Matrix scale(float x)
{
    Matrix m;

    //col 1
    m.m[0] = x;
    m.m[4] = 0.0f;
    m.m[8] = 0.0f;
    m.m[12] = 0.0f;

    //col 2
    m.m[1] = 0.0f;
    m.m[5] = x;
    m.m[9] = 0.0f;
    m.m[13] = 0.0f;

    //col 3
    m.m[2] = 0.0f;
    m.m[6] = 0.0f;
    m.m[10] = x;
    m.m[14] = 0.0f;

    //col 4
    m.m[3] = 0.0f;
    m.m[7] = 0.0f;
    m.m[11] = 0.0f;
    m.m[15] = 1.0f;

    return m;
}

float dot(const Vec& a, const Vec& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vec operator+(const Vec& a, const Vec& b)
{
    return Vec { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

Vec operator-(const Vec& b)
{
    return Vec { -b.x, -b.y, -b.z, -b.w };
}

Vec operator-(const Vec& a, const Vec& b)
{
    return a + (-b);
}

Vec operator*(const Matrix& m, const Vec& v)
{
    float x = dot(Vec { m.m[0],  m.m[1],  m.m[2],  m.m[3] }, v);
    float y = dot(Vec { m.m[4],  m.m[5],  m.m[6],  m.m[7] }, v);
    float z = dot(Vec { m.m[8],  m.m[9],  m.m[10], m.m[11] }, v);
    float w = dot(Vec { m.m[12], m.m[13], m.m[14], m.m[15] }, v);
    return Vec { x, y, z, w };
}

Matrix operator*(const Matrix& a, const Matrix& b)
{
    Matrix res;

    Vec r0 = Vec{ a.m[0],  a.m[1],  a.m[2],  a.m[3] };
    Vec r1 = Vec{ a.m[4],  a.m[5],  a.m[6],  a.m[7] };
    Vec r2 = Vec{ a.m[8],  a.m[9],  a.m[10], a.m[11] };
    Vec r3 = Vec{ a.m[12], a.m[13], a.m[14], a.m[15] };

    Vec c0 = Vec{ b.m[0], b.m[4], b.m[8],  b.m[12] };
    Vec c1 = Vec{ b.m[1], b.m[5], b.m[9],  b.m[13] };
    Vec c2 = Vec{ b.m[2], b.m[6], b.m[9],  b.m[14] };
    Vec c3 = Vec{ b.m[3], b.m[7], b.m[11], b.m[15] };

    res.m[0] = dot(r0, c0);
    res.m[1] = dot(r0, c1);
    res.m[2] = dot(r0, c2);
    res.m[3] = dot(r0, c3);

    res.m[4] = dot(r1, c0);
    res.m[5] = dot(r1, c1);
    res.m[6] = dot(r1, c2);
    res.m[7] = dot(r1, c3);

    res.m[8] = dot(r2, c0);
    res.m[9] = dot(r2, c1);
    res.m[10] = dot(r2, c2);
    res.m[11] = dot(r2, c3);

    res.m[12] = dot(r3, c0);
    res.m[13] = dot(r3, c1);
    res.m[14] = dot(r3, c2);
    res.m[15] = dot(r3, c3);

    return res;
}

struct Vert
{
    float x;
    float y;
    float c;
};

struct Interpolant
{
    Interpolant(Vert a, Vert b, Vert c)
    {
        float dx = (b.x - c.x) * (a.y - c.y) - (a.x - c.x) * (b.y - c.y);
        float dy = -dx;

        stepX = ((b.c - c.c) * (a.y - c.y) - (a.c - c.c) * (b.y - c.y)) / dx;
        stepY = ((b.c - c.c) * (a.x - c.x) - (a.c - c.c) * (b.x - c.x)) / dy;

        minC = a.c;
        midC = b.c;
    }

    float stepX;
    float stepY;
    float currentC;

    float minC;
    float midC;

    void CalculateC(float dx, float dy, bool isMin)
    {
        // can't do isMin? will change colors?
        currentC = (isMin ? minC : midC) + dy * stepY + dx * stepX;
    }
};

struct Edge
{
    Edge(Vec b, Vec e, std::vector<Interpolant>& inter, bool isMin = true): interpolants(inter), begin(b), isBeginMin(isMin)
    {
        pixelYBegin = static_cast<int32_t>(ceil(b.y));
        pixelYEnd = static_cast<int32_t>(ceil(e.y));

        float distanceX = e.x - b.x;
        float distanceY = e.y - b.y;
        stepX = distanceY > 0.0f ? distanceX / distanceY : 0.0f;
    }

    void CalculateX(int32_t y)
    {
        pixelX = static_cast<int32_t>(ceil(begin.x + (y - begin.y) * stepX));
        for (Interpolant& i : interpolants)
        {
            i.CalculateC(pixelX - begin.x, y - begin.y, isBeginMin);
        }
    }

    int32_t pixelYBegin;
    int32_t pixelYEnd;
    int32_t pixelX;

    float stepX;
    bool isBeginMin;
    Vec begin;

    std::vector<Interpolant> interpolants;
};

void DrawTrianglePart(Edge& minMax, Edge& other)
{
    static uint64_t frame = 0;

    frame++;
    if (other.pixelYEnd - other.pixelYBegin > 2)
    {
        DebugOut(L"Can you stop? %d in %u\n", other.pixelYEnd - other.pixelYBegin, frame);
    }

    for (int32_t y = other.pixelYBegin; y < other.pixelYEnd; y++)
    {
        minMax.CalculateX(y);
        other.CalculateX(y);

        int32_t pixelXBegin = minMax.pixelX;
        int32_t pixelXEnd = other.pixelX;

        if (pixelXEnd < pixelXBegin)
        {
            int32_t temp = pixelXBegin;
            pixelXBegin = pixelXEnd;
            pixelXEnd = temp;
        }

        std::vector<float> interpolants_raw;
        interpolants_raw.resize(other.interpolants.size());

        for (int32_t x = pixelXBegin; x < pixelXEnd; x++)
        {
            for (uint32_t i = 0; i < interpolants_raw.size(); i++)
            {
                float beginC = minMax.interpolants[i].currentC;
                float endC = other.interpolants[i].currentC;
                float percent = static_cast<float>(x - pixelXBegin) / static_cast<float>(pixelXEnd - pixelXBegin);
                interpolants_raw[i] = beginC + (endC - beginC) * percent;
            }
            
            //if (interpolants_raw[6] < ZBuffer[y * GameWidth + x])
            //{
            //    ZBuffer[y * GameWidth + x] = interpolants_raw[6];
            //}
            //else
            //{
            //    continue;
            //}

            //colored
            DrawPixel(x, y, Color(
                static_cast<uint8_t>(interpolants_raw[0] * (1.0f / interpolants_raw[5]) * 255.0f),
                static_cast<uint8_t>(interpolants_raw[1] * (1.0f / interpolants_raw[5]) * 255.0f),
                static_cast<uint8_t>(interpolants_raw[2] * (1.0f / interpolants_raw[5]) * 255.0f)
            ));

            //DrawPixel(x, y, Color(
            //    static_cast<uint8_t>(255),
            //    static_cast<uint8_t>(255),
            //    static_cast<uint8_t>(255)
            //));

            //textured
            int32_t textureX = static_cast<int32_t>(interpolants_raw[3] * (1.0f / interpolants_raw[5]) * TextureWidth);
            int32_t textureY = static_cast<int32_t>(interpolants_raw[4] * (1.0f / interpolants_raw[5]) * TextureHeight);

            //textureX = std::clamp(textureX, 0, (TextureWidth - 1));
            //textureY = std::clamp(textureY, 0, (TextureHeight - 1));

            //int32_t texelBase = textureY * TextureWidth * 3 + textureX * 3;
            //DrawPixel(x, y, Color(Texture[texelBase], Texture[texelBase + 1], Texture[texelBase + 2]));
        }
    }
}

Matrix t;

void DrawTriangle(VertIndex a, VertIndex b, VertIndex c)
{
    std::array<Vec, 3> vertices { verticesObj[a.vert], verticesObj[b.vert], verticesObj[c.vert] };

    for (Vec& v : vertices)
    {
        v = t * v;
        v.x /= v.w;
        v.y /= v.w;
        v.z /= v.w;
    }

    std::array<VertIndex, 3> indices { a, b, c };
    std::sort(std::begin(indices), std::end(indices), [](const VertIndex& lhs, const VertIndex& rhs) { return verticesObj[lhs.vert].y < verticesObj[rhs.vert].y; });
    std::sort(std::begin(vertices), std::end(vertices), [](const Vec& lhs, const Vec& rhs) { return lhs.y < rhs.y; });

    for (Vec& v : vertices)
    {
        // TODO.PAVELZA: Need to clip coordinates before scanline buffer phaze.
        assert(-1.0f <= v.x && v.x <= 1.0f);
        assert(-1.0f <= v.y && v.y <= 1.0f);
        assert(-1.0f <= v.z && v.z <= 1.0f);
        v.x = (GameWidth - 1) * ((v.x + 1) / 2.0f);
        v.y = (GameHeight - 1) * ((v.y + 1) / 2.0f);
    }

    Interpolant red      ({ vertices[0].x, vertices[0].y, 1.0f / vertices[0].w }, { vertices[1].x, vertices[1].y, 0.0f / vertices[1].w }, { vertices[2].x, vertices[2].y, 0.0f / vertices[2].w });
    Interpolant green    ({ vertices[0].x, vertices[0].y, 0.0f / vertices[0].w }, { vertices[1].x, vertices[1].y, 1.0f / vertices[1].w }, { vertices[2].x, vertices[2].y, 0.0f / vertices[2].w });
    Interpolant blue     ({ vertices[0].x, vertices[0].y, 0.0f / vertices[0].w }, { vertices[1].x, vertices[1].y, 0.0f / vertices[1].w }, { vertices[2].x, vertices[2].y, 1.0f / vertices[2].w });

    Interpolant xTexture ({ vertices[0].x, vertices[0].y, textureX[indices[0].text] / vertices[0].w }, { vertices[1].x, vertices[1].y, textureX[indices[1].text] / vertices[1].w }, { vertices[2].x, vertices[2].y, textureX[indices[2].text] / vertices[2].w });
    Interpolant yTexture ({ vertices[0].x, vertices[0].y, textureY[indices[0].text] / vertices[0].w }, { vertices[1].x, vertices[1].y, textureY[indices[1].text] / vertices[1].w }, { vertices[2].x, vertices[2].y, textureY[indices[2].text] / vertices[2].w });

    Interpolant oneOverW ({ vertices[0].x, vertices[0].y, 1.0f / vertices[0].w }, { vertices[1].x, vertices[1].y, 1.0f / vertices[1].w }, { vertices[2].x, vertices[2].y, 1.0f / vertices[2].w });
    Interpolant z        ({ vertices[0].x, vertices[0].y, vertices[0].z }, { vertices[1].x, vertices[1].y, vertices[1].z }, { vertices[2].x, vertices[2].y, vertices[2].z });

    std::vector<Interpolant> interpolants { red, green, blue, xTexture, yTexture, oneOverW, z };

    Edge minMax(vertices[0], vertices[2], interpolants);
    Edge minMiddle(vertices[0], vertices[1], interpolants);
    Edge middleMax(vertices[1], vertices[2], interpolants);

    DrawTrianglePart(minMax, minMiddle);
    DrawTrianglePart(minMax, middleMax);
}

void LoadOBJ()
{
    fstream file("cube.obj");
    string line;
    while (getline(file, line))
    {
        stringstream ss(line);
        string primitive_type;
        if (ss >> primitive_type) 
        {
            if (primitive_type == "v") 
            {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;

                if (ss >> x >> y >> z) {
                    //scale by 20
                    verticesObj.push_back({ x * 20, y * 20, z * 20, 1.0f });
                }
            }
            else if (primitive_type == "vt") 
            {
                float u = 0.0f;
                float v = 0.0f;

                if (ss >> u >> v) {
                    textureX.push_back(u);
                    textureY.push_back(v);
                }
            }
            else if (primitive_type == "f") 
            {
                vector<VertIndex> vertIndices;
                string nextStr;
                while (ss >> nextStr)
                {
                    stringstream v(nextStr);

                    uint32_t vert_index;
                    v >> vert_index;

                    v.ignore(1);

                    uint32_t texture_index;
                    v >> texture_index;

                    v.ignore(1);

                    uint32_t normal_index;
                    v >> normal_index;

                    vertIndices.push_back({ vert_index - 1, texture_index - 1 });
                }

                if (vertIndices.size() == 3)
                {
                    indicesObj.push_back(vertIndices[0]);
                    indicesObj.push_back(vertIndices[1]);
                    indicesObj.push_back(vertIndices[2]);
                }
                else if (vertIndices.size() == 4)
                {
                    indicesObj.push_back(vertIndices[0]);
                    indicesObj.push_back(vertIndices[1]);
                    indicesObj.push_back(vertIndices[2]);
                    indicesObj.push_back(vertIndices[0]);
                    indicesObj.push_back(vertIndices[2]);
                    indicesObj.push_back(vertIndices[3]);
                }
                else
                {
                    assert(false);
                }
            }
        }
    }

    triangles_count = indicesObj.size() / 3;
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
        NOT_FAILED(ZBuffer = (float*)VirtualAlloc(0, GameWidth * GameHeight * sizeof(float), MEM_COMMIT, PAGE_READWRITE), 0);

        int32_t channels;
        NOT_FAILED(Texture = stbi_load("test.bmp", &TextureWidth, &TextureHeight, &channels, 3), 0);

        auto frameExpectedTime = chrono::milliseconds(1000 / FPS);

        // init model
        LoadOBJ();

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

            angle += 0.01f;
            if (angle > 2.0f)
            {
                angle = 0.0f;
            }

            t = perspective() * (translate(0.0f, 0.0f, zCoord) * rotateY(static_cast<float>(M_PI) * angle));

            ClearScreen(Color::Black);
            //for (uint32_t i = 0; i < indicesObj.size(); i += 3)
            //{
            //    DrawTriangle(indicesObj[i], indicesObj[i + 1], indicesObj[i + 2]);
            //}

            //DebugOut(L"Angle: %f\n", angle);
            DrawTriangle(indicesObj[0], indicesObj[1], indicesObj[2]);
            //DrawTriangle(indicesObj[3], indicesObj[4], indicesObj[5]);
            //DrawTriangle(indicesObj[6], indicesObj[7], indicesObj[8]);
            //DrawTriangle(indicesObj[9], indicesObj[10], indicesObj[11]);
            //DrawTriangle(indicesObj[12], indicesObj[13], indicesObj[14]);
            //DrawTriangle(indicesObj[15], indicesObj[16], indicesObj[17]);

            //DrawTriangle(indicesObj[18], indicesObj[19], indicesObj[20]);
            //DrawTriangle(indicesObj[21], indicesObj[22], indicesObj[23]);
            //DrawTriangle(indicesObj[24], indicesObj[25], indicesObj[26]);
            //DrawTriangle(indicesObj[27], indicesObj[28], indicesObj[29]);
            //DrawTriangle(indicesObj[30], indicesObj[31], indicesObj[32]);
            //DrawTriangle(indicesObj[33], indicesObj[34], indicesObj[35]);


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

        stbi_image_free(Texture);
        NOT_FAILED(VirtualFree(BackBuffer, 0, MEM_RELEASE), 0);
        NOT_FAILED(ReleaseDC(window, screenContext), 1);
    }

    return 0;
}