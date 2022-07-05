#define _USE_MATH_DEFINES

#include <Windows.h>
#include <stdint.h>
#include <cassert>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <array>
#include <cmath>
#include <wincodec.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "../common/stb_image.h"
#include "../common/profile.h"
#include "../common/common.h"

static uint32_t CurrentModelIndex = 0;

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

struct InterpolationPoint
{
    float x;
    float y;
    float c;
};

struct Interpolant
{
    Interpolant(const InterpolationPoint& a, const InterpolationPoint& b, const InterpolationPoint& c)
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
    float currentC = 0.0f;

    float minC;
    float midC;

    void CalculateC(float dx, float dy, bool isMin)
    {
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

void DrawTrianglePart(Edge& minMax, Edge& other, bool isSecondPart = false)
{
    for (int32_t y = other.pixelYBegin; y < other.pixelYEnd; y++)
    {
        Edge* left = &minMax;
        Edge* right = &other;

        left->CalculateX(y);
        right->CalculateX(y);

        if (left->pixelX > right->pixelX)
        {
            Edge* temp = left;
            left = right;
            right = temp;
        }

        std::vector<float> interpolants_raw;
        interpolants_raw.resize(left->interpolants.size());

        for (int32_t x = left->pixelX; x < right->pixelX; x++)
        {
            for (uint32_t i = 0; i < interpolants_raw.size(); i++)
            {
                float beginC = left->interpolants[i].currentC;
                float endC = right->interpolants[i].currentC;
                float percent = static_cast<float>(x - left->pixelX) / static_cast<float>(right->pixelX - left->pixelX);
                interpolants_raw[i] = beginC + (endC - beginC) * percent;
            }

            if (interpolants_raw[6] < ZBuffer[y * GameWidth + x])
            {
                ZBuffer[y * GameWidth + x] = interpolants_raw[6];
            }
            else
            {
                continue;
            }

            float tintRed = interpolants_raw[0] / interpolants_raw[5];
            float tintGreen = interpolants_raw[1] / interpolants_raw[5];
            float tintBlue = interpolants_raw[2] / interpolants_raw[5];

            float normalX = interpolants_raw[7] / interpolants_raw[5];
            float normalY = interpolants_raw[8] / interpolants_raw[5];
            float normalZ = interpolants_raw[9] / interpolants_raw[5];

            float viewX = interpolants_raw[10] / interpolants_raw[5];
            float viewY = interpolants_raw[11] / interpolants_raw[5];
            float viewZ = interpolants_raw[12] / interpolants_raw[5];

            Vec pos_view { viewX, viewY, viewZ, 1.0f };
            Vec normal_vec = normalize({ normalX, normalY, normalZ, 0.0f });
            Vec light_vec = normalize(light.position_view - pos_view);

            Vec diffuse = light.color.GetVec() * static_cast<float>(max(dot(normal_vec, light_vec), 0.0));
            Vec ambient = light.color.GetVec() * light.ambientStrength;

            float specAmount = static_cast<float>(max(dot(normalize(pos_view), reflect(normal_vec, light_vec * -1.0f)), 0.0f));
            Vec specular = light.color.GetVec() * pow(specAmount, light.specularShininess) * light.specularStrength;

            // From 0 to TextureWidth - 1 (TextureWidth pixels in total)
            int32_t textureX = static_cast<int32_t>((interpolants_raw[3] / interpolants_raw[5]) * (TextureWidth - 1));
            // From 0 to TextureHeight - 1 (TextureHeight pixels in total)
            int32_t textureY = static_cast<int32_t>((interpolants_raw[4] / interpolants_raw[5]) * (TextureHeight - 1));

            assert(textureY < TextureHeight&& textureX < TextureWidth);
            int32_t texelBase = textureY * TextureWidth * 3 + textureX * 3;

            Color texture_color(Texture[texelBase], Texture[texelBase + 1], Texture[texelBase + 2]);
            
            // Assuming 1 is model for light.
            Vec frag_color = CurrentModelIndex == 1 ? Vec{ tintRed, tintGreen, tintBlue } : texture_color.GetVec();
            Vec final_color = CurrentModelIndex == 1 ? frag_color : (diffuse + ambient + specular) * frag_color;
            final_color = final_color * 255.0f;

            DrawPixel(x, y, Color(
                static_cast<uint8_t>(min(final_color.x, 255.0f)),
                static_cast<uint8_t>(min(final_color.y, 255.0f)),
                static_cast<uint8_t>(min(final_color.z, 255.0f))
            ));
        }
    }
}

float Lerp(float begin, float end, float lerpAmount)
{
    return begin + (end - begin) * lerpAmount;
}

Vertex Lerp(const Vertex& begin, const Vertex& end, float lerpAmount)
{
    Vertex result;
    result.pos.x = Lerp(begin.pos.x, end.pos.x, lerpAmount);
    result.pos.y = Lerp(begin.pos.y, end.pos.y, lerpAmount);
    result.pos.z = Lerp(begin.pos.z, end.pos.z, lerpAmount);
    result.pos.w = Lerp(begin.pos.w, end.pos.w, lerpAmount);
    result.pos_view.x = Lerp(begin.pos_view.x, end.pos_view.x, lerpAmount);
    result.pos_view.y = Lerp(begin.pos_view.y, end.pos_view.y, lerpAmount);
    result.pos_view.z = Lerp(begin.pos_view.z, end.pos_view.z, lerpAmount);
    result.pos_view.w = Lerp(begin.pos_view.w, end.pos_view.w, lerpAmount);
    result.textureX = Lerp(begin.textureX, end.textureX, lerpAmount);
    result.textureY = Lerp(begin.textureY, end.textureY, lerpAmount);
    result.red = Lerp(begin.red, end.red, lerpAmount);
    result.green = Lerp(begin.green, end.green, lerpAmount);
    result.blue = Lerp(begin.blue, end.blue, lerpAmount);
    result.normal.x = Lerp(begin.normal.x, end.normal.x, lerpAmount);
    result.normal.y = Lerp(begin.normal.y, end.normal.y, lerpAmount);
    result.normal.z = Lerp(begin.normal.z, end.normal.z, lerpAmount);
    result.normal.w = Lerp(begin.normal.w, end.normal.w, lerpAmount);
    return result;
}

Interpolant GetInterpolant(std::vector<Vertex>& vertices, float c1, float c2, float c3)
{
    return Interpolant({ vertices[0].pos.x, vertices[0].pos.y, c1 / vertices[0].pos.w }, { vertices[1].pos.x, vertices[1].pos.y, c2 / vertices[1].pos.w }, { vertices[2].pos.x, vertices[2].pos.y, c3 / vertices[2].pos.w });
}

void DrawRawTriangle(std::vector<Vertex>& vertices)
{
    assert(vertices.size() == 3);

    for (Vertex& v : vertices)
    {
        v.pos.x /= v.pos.w;
        v.pos.y /= v.pos.w;
        v.pos.z /= v.pos.w;
    }

    // TODO.PAVELZA: We can do backface culling here.

    std::sort(std::begin(vertices), std::end(vertices), [](const Vertex& lhs, const Vertex& rhs) { return lhs.pos.y < rhs.pos.y; });

    for (Vertex& v : vertices)
    {
        // TODO.PAVELZA: Clipping might result in some vertices being slightly outside of -1 to 1 range, so we clamp. Will need to think how to avoid this.
        v.pos.x = clamp(v.pos.x, -1.0f, 1.0f);
        v.pos.y = clamp(v.pos.y, -1.0f, 1.0f);
        v.pos.z = clamp(v.pos.z, -1.0f, 1.0f);

        v.pos.x = (GameWidth - 1) * ((v.pos.x + 1) / 2.0f);
        v.pos.y = (GameHeight - 1) * ((v.pos.y + 1) / 2.0f);
    }

    // No division by w, because it is already divided by w.
    Interpolant z({ vertices[0].pos.x, vertices[0].pos.y, vertices[0].pos.z }, { vertices[1].pos.x, vertices[1].pos.y, vertices[1].pos.z }, { vertices[2].pos.x, vertices[2].pos.y, vertices[2].pos.z });

    Interpolant red = GetInterpolant(vertices, vertices[0].red, vertices[1].red, vertices[2].red);
    Interpolant green = GetInterpolant(vertices, vertices[0].green, vertices[1].green, vertices[2].green);
    Interpolant blue = GetInterpolant(vertices, vertices[0].blue, vertices[1].blue, vertices[2].blue);

    Interpolant xTexture = GetInterpolant(vertices, vertices[0].textureX, vertices[1].textureX, vertices[2].textureX);
    Interpolant yTexture = GetInterpolant(vertices, vertices[0].textureY, vertices[1].textureY, vertices[2].textureY);

    Interpolant oneOverW = GetInterpolant(vertices, 1.0f, 1.0f, 1.0f);

    Interpolant xNormal = GetInterpolant(vertices, vertices[0].normal.x, vertices[1].normal.x, vertices[2].normal.x);
    Interpolant yNormal = GetInterpolant(vertices, vertices[0].normal.y, vertices[1].normal.y, vertices[2].normal.y);
    Interpolant zNormal = GetInterpolant(vertices, vertices[0].normal.z, vertices[1].normal.z, vertices[2].normal.z);

    Interpolant xView = GetInterpolant(vertices, vertices[0].pos_view.x, vertices[1].pos_view.x, vertices[2].pos_view.x);
    Interpolant yView = GetInterpolant(vertices, vertices[0].pos_view.y, vertices[1].pos_view.y, vertices[2].pos_view.y);
    Interpolant zView = GetInterpolant(vertices, vertices[0].pos_view.z, vertices[1].pos_view.z, vertices[2].pos_view.z);

    std::vector<Interpolant> interpolants { red, green, blue, xTexture, yTexture, oneOverW, z, xNormal, yNormal, zNormal, xView, yView, zView };

    Edge minMax(vertices[0].pos, vertices[2].pos, interpolants, true);
    Edge minMiddle(vertices[0].pos, vertices[1].pos, interpolants, true);
    Edge middleMax(vertices[1].pos, vertices[2].pos, interpolants, false);

    DrawTrianglePart(minMax, minMiddle);
    DrawTrianglePart(minMax, middleMax, true);
}

bool IsVertexInside(const Vertex& point, int32_t axis, int32_t plane)
{    
    return point.pos.Get(axis) * plane <= point.pos.w;
}

void ClipTrianglePlane(std::vector<Vertex>& vertices, int32_t axis, int32_t plane)
{
    std::vector<Vertex> result;
    size_t previousElement = vertices.size() - 1;

    for (size_t currentElement = 0; currentElement < vertices.size(); currentElement++)
    {
        bool isPreviousInside = IsVertexInside(vertices[previousElement], axis, plane);
        bool isCurrentInside = IsVertexInside(vertices[currentElement], axis, plane);

        if (isPreviousInside != isCurrentInside)
        {
            float k = (vertices[previousElement].pos.w - vertices[previousElement].pos.Get(axis) * plane);
            float lerpAmount = k / (k - vertices[currentElement].pos.w + vertices[currentElement].pos.Get(axis) * plane);
            result.push_back(Lerp(vertices[previousElement], vertices[currentElement], lerpAmount));
        }

        if (isCurrentInside)
        {
            result.push_back(vertices[currentElement]);
        }
        
        previousElement = currentElement;
    }

    swap(vertices, result);
}

bool ClipTriangleAxis(std::vector<Vertex>& vertices, int32_t axis)
{
    ClipTrianglePlane(vertices, axis, 1);

    if (vertices.size() == 0)
    {
        return false;
    }

    ClipTrianglePlane(vertices, axis, -1);

    return vertices.size() != 0;
}

void DrawTriangle(Index a, Index b, Index c, const Matrix& transform)
{
    std::vector<Vertex> vertices 
    {
        Vertex 
        {
            Vec { verticesObj[a.vert].x, verticesObj[a.vert].y, verticesObj[a.vert].z, verticesObj[a.vert].w },
            Vec { verticesObj[a.vert].x, verticesObj[a.vert].y, verticesObj[a.vert].z, verticesObj[a.vert].w },
            Vec {  normalsObj[a.norm].x,  normalsObj[a.norm].y,  normalsObj[a.norm].z,  normalsObj[a.norm].w },
            textureX[a.text],
            textureY[a.text],
            colorsObj[a.vert].x,
            colorsObj[a.vert].y,
            colorsObj[a.vert].z 
        },
        Vertex 
        {
            Vec { verticesObj[b.vert].x, verticesObj[b.vert].y, verticesObj[b.vert].z, verticesObj[b.vert].w },
            Vec { verticesObj[b.vert].x, verticesObj[b.vert].y, verticesObj[b.vert].z, verticesObj[b.vert].w },
            Vec {  normalsObj[b.norm].x,  normalsObj[b.norm].y,  normalsObj[b.norm].z,  normalsObj[b.norm].w },
            textureX[b.text],
            textureY[b.text],
            colorsObj[b.vert].x,
            colorsObj[b.vert].y,
            colorsObj[b.vert].z
        },
        Vertex 
        {
            Vec { verticesObj[c.vert].x, verticesObj[c.vert].y, verticesObj[c.vert].z, verticesObj[c.vert].w },
            Vec { verticesObj[c.vert].x, verticesObj[c.vert].y, verticesObj[c.vert].z, verticesObj[c.vert].w },
            Vec {  normalsObj[c.norm].x,  normalsObj[c.norm].y,  normalsObj[c.norm].z,  normalsObj[c.norm].w },
            textureX[c.text],
            textureY[c.text],
            colorsObj[c.vert].x,
            colorsObj[c.vert].y,
            colorsObj[c.vert].z
        }
    };

    for (Vertex& v : vertices)
    {
        v.pos = perspective() * transform * v.pos;
        v.pos_view = transform * v.pos_view;
        // This is possible because we do not do non-uniform scale in transform. If we are about to do non-uniform scale, we should calculate the normal matrix.
        v.normal = transform * v.normal;
    }

    // TODO.PAVELZA: If all the points are outside of the view frustum - we can cull immediately.
    // TODO.PAVELZA: If all points are inside - we can draw immediately.

    if (ClipTriangleAxis(vertices, 0) && ClipTriangleAxis(vertices, 1) && ClipTriangleAxis(vertices, 2))
    {
        assert(vertices.size() >= 3);

        for (size_t i = 2; i < vertices.size(); i++)
        {
            std::vector<Vertex> triangle;
            triangle.resize(3);

            triangle[0] = vertices[0];
            triangle[1] = vertices[i - 1];
            triangle[2] = vertices[i];

            DrawRawTriangle(triangle);
        }
    }
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
            WindowWidth,
            WindowHeight,
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
        NOT_FAILED(Texture = stbi_load("../../assets/texture.jpg", &TextureWidth, &TextureHeight, &channels, 3), 0);

        auto frameExpectedTime = chrono::milliseconds(1000 / FPS);

        LoadContext context;

        // init model
        models.push_back(LoadOBJ("../../assets/monkey.obj", 50.0f, Vec{ 0.0f, 0.0f, 0.0f, 1.0f }, context));

        // init light
        models.push_back(LoadOBJ("../../assets/cube.obj", 10.0f, Vec{ 100.0f, 100.0f, 100.0f, 1.0f }, context));

        Camera camera;
        camera.position.z = 200;
        camera.position.x = 10;
        camera.position.y = 0;

        camera.pitch = 0.0f;
        camera.yaw = 0.0f;

        camera.forward = Vec{ 0.0f, 0.0f, -10.0f, 0.0f };
        camera.left = Vec{ -10.0f, 0.0f, 0.0f, 0.0f };

        light.position = models[1].position;

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

            Vec forward = cam(camera) * camera.forward;
            Vec left = cam(camera) * camera.left;

            if (KeyDown[UpKey])
            {
                camera.pitch -= 0.1f;
                if (camera.pitch < 0)
                {
                    camera.pitch += static_cast<float>(M_PI) * 2;
                }
            }

            if (KeyDown[DownKey])
            {
                camera.pitch += 0.1f;
                if (camera.pitch > static_cast<float>(M_PI) * 2)
                {
                    camera.pitch -= static_cast<float>(M_PI) * 2;
                }
            }

            if (KeyDown[RightKey])
            {
                camera.yaw -= 0.1f;
                if (camera.yaw < 0)
                {
                    camera.yaw += static_cast<float>(M_PI) * 2;
                }
            }

            if (KeyDown[LeftKey])
            {
                camera.yaw += 0.1f;
                if (camera.yaw > static_cast<float>(M_PI) * 2)
                {
                    camera.yaw -= static_cast<float>(M_PI) * 2;
                }
            }

            if (KeyDown[WKey]) 
            {
                camera.position = camera.position + forward;
            }

            if (KeyDown[AKey])
            {
                camera.position = camera.position + left;
            }

            if (KeyDown[SKey])
            {
                camera.position = camera.position - forward;
            }

            if (KeyDown[DKey])
            {
                camera.position = camera.position - left;
            }

            // calculate light's position in view space
            light.position_view = view(camera) * light.position;

            ClearScreen(Color::Black);

            uint32_t indices_offset = 0;
            for (size_t i = 0; i < models.size(); i++)
            {
                CurrentModelIndex = static_cast<uint32_t>(i);
                const Model& model = models.at(i);
                for (uint32_t i = 0; i < model.indices_count / 3; i++)
                {
                    DrawTriangle(indicesObj[indices_offset + i * 3 + 0], indicesObj[indices_offset + i * 3 + 1], indicesObj[indices_offset + i * 3 + 2], view(camera));
                }
                indices_offset += model.indices_count;
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