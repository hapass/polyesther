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
* TODO:
* Refactoring.
* Optimization.
*/

using namespace std;

static const uint32_t GameWidth = 800;
static const uint32_t GameHeight = 600;
static const uint32_t FPS = 30;

static int32_t WindowWidth = 800;
static int32_t WindowHeight = 600;
static BITMAPINFO BackBufferInfo;
static uint32_t* BackBuffer;

static float* ZBuffer;

static int32_t TextureWidth;
static int32_t TextureHeight;
static uint8_t* Texture;

static uint32_t CurrentModelIndex = 0;

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

struct Vec
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    float Get(int32_t index) const
    {
        assert(0 <= index && index <= 3);

        switch (index)
        {
        case 0: return x;
        case 1: return y;
        case 2: return z;
        }
        return w;
    }
};

struct Model {
    uint32_t indices_count;
    uint32_t vertices_count;
    uint32_t texture_count;
    uint32_t normal_count;

    Vec position;
    float scale;
};

struct Camera
{
    Vec position;
    float pitch = 0.0f; // around x while at 0
    float yaw = 0.0f; // around z while at 0

    Vec forward;
    Vec left;
};

struct VertIndex
{
    uint32_t vert;
    uint32_t text;
    uint32_t norm;
};

static std::vector<Vec> verticesObj;
static std::vector<Vec> normalsObj;
static std::vector<float> textureX;
static std::vector<float> textureY;
static std::vector<VertIndex> indicesObj;
static std::vector<Vec> colorsObj;

static std::vector<Model> models;

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
    Color(uint8_t r, uint8_t g, uint8_t b) : rgb(0u), rgb_vec { r / 255.0f, g / 255.0f, b / 255.0f, 0.0f }
    {
        rgb |= r << 16;
        rgb |= g << 8;
        rgb |= b << 0;
    }

    const Vec& GetVec() { return rgb_vec; }

    uint32_t rgb;
    Vec rgb_vec;

    static Color Black;
    static Color White;
    static Color Red;
    static Color Green;
    static Color Blue;
};

Color Color::Black = Color(0u, 0u, 0u);
Color Color::White = Color(255u, 255u, 255u);
Color Color::Red = Color(255u, 0u, 0u);
Color Color::Green = Color(0u, 255u, 0u);
Color Color::Blue = Color(0u, 0u, 255u);

struct Light
{
    Vec position;
    Vec position_view;

    float ambientStrength = 0.1f;
    float specularStrength = 0.5f;
    float specularShininess = 32.0f;
    Color color = Color::White;
};

static Light light;

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

Matrix rotateX(float alpha)
{
    Matrix m;

    //col 1
    m.m[0] = 1.0f;
    m.m[4] = 0.0f;
    m.m[8] = 0.0f;
    m.m[12] = 0.0f;

    //col 2
    m.m[1] = 0.0f;
    m.m[5] = cosf(alpha);
    m.m[9] = -sinf(alpha);
    m.m[13] = 0.0f;

    //col 3
    m.m[2] = 0.0f;
    m.m[6] = sinf(alpha);
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
    float halfFieldOfView = 25 * (static_cast<float>(M_PI) / 180);
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
    m.m[11] = -2 * farPlane * nearPlane / (farPlane - nearPlane);
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

Matrix transpose(const Matrix& m)
{
    Matrix r;

    //col 1
    r.m[0] = m.m[0];
    r.m[4] = m.m[1];
    r.m[8] = m.m[2];
    r.m[12] = m.m[3];

    //col 2
    r.m[1] = m.m[4];
    r.m[5] = m.m[5];
    r.m[9] = m.m[6];
    r.m[13] = m.m[7];

    //col 3
    r.m[2] = m.m[8];
    r.m[6] = m.m[9];
    r.m[10] = m.m[10];
    r.m[14] = m.m[11];

    //col 4
    r.m[3] = m.m[12];
    r.m[7] = m.m[13];
    r.m[11] = m.m[14];
    r.m[15] = m.m[15];

    return r;
}

float dot(const Vec& a, const Vec& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vec operator*(const Vec& a, const Vec& b)
{
    return Vec { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w };
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

Vec operator*(const Vec& a, float b)
{
    return Vec { a.x * b, a.y * b, a.z * b, a.w * b };
}

Vec normalize(const Vec& v)
{
    float norm = sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    return Vec { v.x / norm, v.y / norm, v.z / norm, v.w / norm };
}

Vec cross(const Vec& a, const Vec& b)
{
    return Vec
    {
        a.y * b.z - b.y * a.z,
        -a.x * b.z + b.x * a.z,
        a.x * b.y - b.x * a.y,
        0.0f
    };
}

Vec reflect(const Vec& normal, const Vec& vec)
{
    const Vec& j = normalize(normal);
    Vec k = normalize(cross(vec, j));
    Vec i = normalize(cross(j, k));

    float vecI = dot(vec, i);
    float vecJ = dot(vec, j);

    return -i * vecI + j * vecJ;
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
    Vec c2 = Vec{ b.m[2], b.m[6], b.m[10], b.m[14] };
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

Matrix view(const Camera& camera)
{
    Matrix rTranslate = translate(-camera.position.x, -camera.position.y, -camera.position.z);
    Matrix rYaw = transpose(rotateY(camera.yaw));
    Matrix rPitch = transpose(rotateX(camera.pitch));

    return rPitch * rYaw * rTranslate;
}

Matrix modelMat(const Model& model)
{
    return translate(model.position.x, model.position.y, model.position.z) * scale(model.scale);
}

Matrix cam(const Camera& camera)
{
    Matrix rYaw = rotateY(camera.yaw);
    Matrix rPitch = rotateX(camera.pitch);

    return rYaw * rPitch;
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

struct Vector
{
    Vec pos;
    Vec pos_view;
    Vec normal;
    float textureX;
    float textureY;
    float red;
    float green;
    float blue;
};

float Lerp(float begin, float end, float lerpAmount)
{
    return begin + (end - begin) * lerpAmount;
}

Vector Lerp(const Vector& begin, const Vector& end, float lerpAmount)
{
    Vector result;
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

Interpolant GetInterpolant(std::vector<Vector>& vertices, float c1, float c2, float c3)
{
    return Interpolant({ vertices[0].pos.x, vertices[0].pos.y, c1 / vertices[0].pos.w }, { vertices[1].pos.x, vertices[1].pos.y, c2 / vertices[1].pos.w }, { vertices[2].pos.x, vertices[2].pos.y, c3 / vertices[2].pos.w });
}

void DrawRawTriangle(std::vector<Vector>& vertices)
{
    assert(vertices.size() == 3);

    for (Vector& v : vertices)
    {
        v.pos.x /= v.pos.w;
        v.pos.y /= v.pos.w;
        v.pos.z /= v.pos.w;
    }

    // TODO.PAVELZA: We can do backface culling here.

    std::sort(std::begin(vertices), std::end(vertices), [](const Vector& lhs, const Vector& rhs) { return lhs.pos.y < rhs.pos.y; });

    for (Vector& v : vertices)
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

bool IsPointInside(const Vector& point, int32_t axis, int32_t plane)
{    
    return point.pos.Get(axis) * plane <= point.pos.w;
}

void ClipTrianglePlane(std::vector<Vector>& vertices, int32_t axis, int32_t plane)
{
    std::vector<Vector> result;
    size_t previousElement = vertices.size() - 1;

    for (size_t currentElement = 0; currentElement < vertices.size(); currentElement++)
    {
        bool isPreviousInside = IsPointInside(vertices[previousElement], axis, plane);
        bool isCurrentInside = IsPointInside(vertices[currentElement], axis, plane);

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

bool ClipTriangleAxis(std::vector<Vector>& vertices, int32_t axis)
{
    ClipTrianglePlane(vertices, axis, 1);

    if (vertices.size() == 0)
    {
        return false;
    }

    ClipTrianglePlane(vertices, axis, -1);

    return vertices.size() != 0;
}

void DrawTriangle(VertIndex a, VertIndex b, VertIndex c, const Matrix& transform)
{
    std::vector<Vector> vertices 
    {
        Vector 
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
        Vector 
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
        Vector 
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

    for (Vector& v : vertices)
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
            std::vector<Vector> triangle;
            triangle.resize(3);

            triangle[0] = vertices[0];
            triangle[1] = vertices[i - 1];
            triangle[2] = vertices[i];

            DrawRawTriangle(triangle);
        }
    }
}

struct LoadContext
{
    uint32_t vertices_offset = 0;
    uint32_t texture_offset = 0;
    uint32_t normal_offset = 0;
};

static Color tint = Color::White;

Model LoadOBJ(const char* fileName, LoadContext& context)
{
    Model model;
    model.indices_count = 0;
    model.vertices_count = 0;
    model.texture_count = 0;
    model.normal_count = 0;

    fstream file(fileName);
    string line;

    while (getline(file, line))
    {
        stringstream ss(line);
        string primitive_type;
        if (ss >> primitive_type) 
        {
            if (primitive_type == "v" || primitive_type == "vn")
            {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;

                if (ss >> x >> y >> z) {

                    if (primitive_type == "v")
                    {
                        verticesObj.push_back({ x, y, z, 1.0f });
                        colorsObj.push_back(tint.GetVec());

                        // colors and vertices are assumed to have the same count
                        model.vertices_count++;
                    }
                    else
                    {
                        normalsObj.push_back({ x, y, z, 0.0f });
                        model.normal_count++;
                    }
                }
            }
            else if (primitive_type == "vt")
            {
                float u = 0.0f;
                float v = 0.0f;

                if (ss >> u >> v) {
                    textureX.push_back(u);
                    textureY.push_back(v);
                    model.texture_count++;
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

                    vertIndices.push_back({ context.vertices_offset + vert_index - 1, context.texture_offset + texture_index - 1, context.normal_offset + normal_index - 1 });
                }

                if (vertIndices.size() == 3)
                {
                    indicesObj.push_back(vertIndices[0]);
                    indicesObj.push_back(vertIndices[1]);
                    indicesObj.push_back(vertIndices[2]);
                    model.indices_count += 3;
                }
                else if (vertIndices.size() == 4)
                {
                    indicesObj.push_back(vertIndices[0]);
                    indicesObj.push_back(vertIndices[1]);
                    indicesObj.push_back(vertIndices[2]);
                    indicesObj.push_back(vertIndices[0]);
                    indicesObj.push_back(vertIndices[2]);
                    indicesObj.push_back(vertIndices[3]);
                    model.indices_count += 6;
                }
                else
                {
                    assert(false);
                }
            }
        }
    }

    context.texture_offset += model.texture_count;
    context.vertices_offset += model.vertices_count;
    context.normal_offset += model.normal_count;

    return model;
}

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
        NOT_FAILED(Texture = stbi_load("texture.jpg", &TextureWidth, &TextureHeight, &channels, 3), 0);

        auto frameExpectedTime = chrono::milliseconds(1000 / FPS);

        LoadContext context;

        // init model
        models.push_back(LoadOBJ("sphere.obj", context));
        models[0].scale = 50.0f;
        models[0].position = Vec { 0.0f, 0.0f, 0.0f, 1.0f };

        // init light
        models.push_back(LoadOBJ("cube.obj", context));
        models[1].scale = 10.0f;

        Camera camera;
        camera.position.z = 200;
        camera.position.x = 10;
        camera.position.y = 0;

        camera.pitch = 0.0f;
        camera.yaw = 0.0f;

        camera.forward = Vec{ 0.0f, 0.0f, -10.0f, 0.0f };
        camera.left = Vec{ -10.0f, 0.0f, 0.0f, 0.0f };

        light.position = Vec{ 100.0f, 100.0f, 100.0f, 1.0f };

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

            // visualize light at the right position
            models[1].position = light.position;

            // calculate light's position in view space
            light.position_view = view(camera) * modelMat(models[1]) * models[1].position;

            ClearScreen(Color::Black);

            int32_t indices_offset = 0;
            for (size_t i = 0; i < models.size(); i++)
            {
                CurrentModelIndex = i;
                const Model& model = models.at(i);
                for (uint32_t i = 0; i < model.indices_count / 3; i++)
                {
                    DrawTriangle(indicesObj[indices_offset + i * 3 + 0], indicesObj[indices_offset + i * 3 + 1], indicesObj[indices_offset + i * 3 + 2], view(camera) * modelMat(model));
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