#define _USE_MATH_DEFINES

#include <Windows.h>
#include <dxgi.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3dcompiler.h>

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
#include "../common/stb_image.h"
#include "../common/profile.h"

/*
* TODO:
* Refactoring.
* Optimization.
* Render to image.
* One interface for rendering to image.
* Write DX12 renderer that renders the same image.
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

struct Index
{
    uint32_t vert;
    uint32_t text;
    uint32_t norm;
};

static std::vector<Vec> verticesObj;
static std::vector<Vec> normalsObj;
static std::vector<float> textureX;
static std::vector<float> textureY;
static std::vector<Index> indicesObj;
static std::vector<Vec> colorsObj;

static std::vector<Model> models;

void DebugOut(const wchar_t* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    wchar_t dbg_out[4096];
    vswprintf_s(dbg_out, fmt, argp);
    va_end(argp);
    OutputDebugString(dbg_out);
}

void PrintError(HRESULT result)
{
    DebugOut(L"D3D Result: 0x%08X \n", result);
}

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a ## b

#define NOT_FAILED(call, failureCode) \
    if ((call) == failureCode) \
    { \
         assert(false); \
         exit(1); \
    } \

#define D3D_NOT_FAILED(call) \
    HRESULT CONCAT(result, __LINE__) = (call); \
    if (CONCAT(result, __LINE__) != S_OK) \
    { \
        PrintError(CONCAT(result, __LINE__)); \
        assert(false); \
        exit(1); \
    } \

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

struct Vertex
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
                vector<Index> vertIndices;
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

struct Constants
{
    DirectX::XMFLOAT4X4 worldViewProj = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};

struct XVertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

template<typename T>
uint64_t AlignBytes(uint64_t alignment = 256)
{
    const uint64_t upperBound = sizeof(T) + alignment - 1;
    return upperBound - upperBound % alignment;
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

        int32_t channels;
        NOT_FAILED(Texture = stbi_load("../../assets/texture.jpg", &TextureWidth, &TextureHeight, &channels, 3), 0);

        auto frameExpectedTime = chrono::milliseconds(1000 / FPS);

        LoadContext context;

        // init model
        models.push_back(LoadOBJ("../../assets/monkey.obj", context));
        models[0].scale = 50.0f;
        models[0].position = Vec { 0.0f, 0.0f, 0.0f, 1.0f };

        // init light
        models.push_back(LoadOBJ("../../assets/cube.obj", context));
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

        ID3D12Debug* debugController = nullptr;
        D3D_NOT_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
        debugController->Release();

        IDXGIFactory* factory = nullptr;
        D3D_NOT_FAILED(CreateDXGIFactory(IID_PPV_ARGS(&factory)));

        IDXGIAdapter* adapter = nullptr;
        factory->EnumAdapters(0, &adapter);

        ID3D12Device* device = nullptr;
        D3D_NOT_FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

        ID3D12CommandQueue* queue = nullptr;
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        D3D_NOT_FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));

        DXGI_SWAP_CHAIN_DESC swapChainDescription;

        swapChainDescription.BufferDesc.Width = GameWidth;
        swapChainDescription.BufferDesc.Height = GameHeight;
        swapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDescription.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        swapChainDescription.SampleDesc.Count = 1;
        swapChainDescription.SampleDesc.Quality = 0;

        swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDescription.BufferCount = 2;

        swapChainDescription.OutputWindow = window;
        swapChainDescription.Windowed = true;

        swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDescription.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        IDXGISwapChain* swapChain = nullptr;
        D3D_NOT_FAILED(factory->CreateSwapChain(queue, &swapChainDescription, &swapChain));

        D3D12_DESCRIPTOR_HEAP_DESC renderTargetViewHeapDescription;
        renderTargetViewHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        renderTargetViewHeapDescription.NumDescriptors = 2;
        renderTargetViewHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        renderTargetViewHeapDescription.NodeMask = 0;

        ID3D12DescriptorHeap* renderTargetViewHeap = nullptr;
        D3D_NOT_FAILED(device->CreateDescriptorHeap(&renderTargetViewHeapDescription, IID_PPV_ARGS(&renderTargetViewHeap)));

        ID3D12Resource* zeroBuffer = nullptr;
        D3D_NOT_FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&zeroBuffer)));

        D3D12_CPU_DESCRIPTOR_HANDLE zeroBufferHandle{ SIZE_T(INT64(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart().ptr)) };
        device->CreateRenderTargetView(zeroBuffer, nullptr, zeroBufferHandle);

        ID3D12Resource* firstBuffer = nullptr;
        D3D_NOT_FAILED(swapChain->GetBuffer(1, IID_PPV_ARGS(&firstBuffer)));

        D3D12_CPU_DESCRIPTOR_HANDLE firstBufferHandle{ SIZE_T(INT64(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV))) };
        device->CreateRenderTargetView(firstBuffer, nullptr, firstBufferHandle);

        D3D12_RESOURCE_DESC depthBufferDescription;
        depthBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthBufferDescription.Alignment = 0;
        depthBufferDescription.Width = GameWidth;
        depthBufferDescription.Height = GameHeight;
        depthBufferDescription.DepthOrArraySize = 1;
        depthBufferDescription.MipLevels = 1;
        depthBufferDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

        depthBufferDescription.SampleDesc.Count = 1;
        depthBufferDescription.SampleDesc.Quality = 0;

        depthBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthBufferDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        D3D12_HEAP_PROPERTIES defaultHeapProperties;
        defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProperties.VisibleNodeMask = 1;
        defaultHeapProperties.CreationNodeMask = 1;

        ID3D12Resource* depthStencilBuffer = nullptr;
        D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &depthBufferDescription, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&depthStencilBuffer)));

        D3D12_DESCRIPTOR_HEAP_DESC depthStencilViewHeapDescription;
        depthStencilViewHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        depthStencilViewHeapDescription.NumDescriptors = 1;
        depthStencilViewHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        depthStencilViewHeapDescription.NodeMask = 0;

        ID3D12DescriptorHeap* depthStencilViewHeap = nullptr;
        D3D_NOT_FAILED(device->CreateDescriptorHeap(&depthStencilViewHeapDescription, IID_PPV_ARGS(&depthStencilViewHeap)));

        D3D12_CPU_DESCRIPTOR_HANDLE depthBufferHandle(depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart());
        device->CreateDepthStencilView(depthStencilBuffer, nullptr, depthBufferHandle);

        ID3D12CommandAllocator* allocator = nullptr;
        D3D_NOT_FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));

        ID3D12GraphicsCommandList* commandList = nullptr;
        D3D_NOT_FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&commandList)));

        D3D12_RESOURCE_BARRIER depthBufferWriteTransition;
        depthBufferWriteTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBufferWriteTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

        depthBufferWriteTransition.Transition.pResource = depthStencilBuffer;
        depthBufferWriteTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        depthBufferWriteTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBufferWriteTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        commandList->ResourceBarrier(1, &depthBufferWriteTransition);

        UINT64 currentFenceValue = 0;

        ID3D12Fence* fence = nullptr;
        D3D_NOT_FAILED(device->CreateFence(currentFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HANDLE fenceEventHandle = CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
        assert(fenceEventHandle);

        bool isZeroBufferInTheBack = true;

        // rendering

        // Constant buffer
        D3D12_HEAP_PROPERTIES uploadHeapProperties;
        uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeapProperties.VisibleNodeMask = 1;
        uploadHeapProperties.CreationNodeMask = 1;

        D3D12_RESOURCE_DESC uploadBufferDescription;
        uploadBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadBufferDescription.Alignment = 0;
        uploadBufferDescription.Width = AlignBytes<Constants>();
        uploadBufferDescription.Height = 1;
        uploadBufferDescription.DepthOrArraySize = 1;
        uploadBufferDescription.MipLevels = 1;
        uploadBufferDescription.Format = DXGI_FORMAT_UNKNOWN;

        uploadBufferDescription.SampleDesc.Count = 1;
        uploadBufferDescription.SampleDesc.Quality = 0;

        uploadBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uploadBufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

        ID3D12Resource* uploadBuffer = nullptr;
        D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)));

        D3D12_DESCRIPTOR_HEAP_DESC constantBufferDescriptorHeapDescription;
        constantBufferDescriptorHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        constantBufferDescriptorHeapDescription.NumDescriptors = 1;
        constantBufferDescriptorHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        constantBufferDescriptorHeapDescription.NodeMask = 0;

        ID3D12DescriptorHeap* constantBufferDescriptorHeap = nullptr;
        D3D_NOT_FAILED(device->CreateDescriptorHeap(&constantBufferDescriptorHeapDescription, IID_PPV_ARGS(&constantBufferDescriptorHeap)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDescription;
        constantBufferViewDescription.BufferLocation = uploadBuffer->GetGPUVirtualAddress();
        constantBufferViewDescription.SizeInBytes = static_cast<UINT>(uploadBufferDescription.Width);

        device->CreateConstantBufferView(&constantBufferViewDescription, constantBufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        // root signature
        D3D12_DESCRIPTOR_RANGE constantBufferDescriptorRange;
        constantBufferDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        constantBufferDescriptorRange.NumDescriptors = 1;
        constantBufferDescriptorRange.BaseShaderRegister = 0;
        constantBufferDescriptorRange.RegisterSpace = 0;
        constantBufferDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER constantBufferParameter;
        constantBufferParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        constantBufferParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        constantBufferParameter.DescriptorTable.NumDescriptorRanges = 1;
        constantBufferParameter.DescriptorTable.pDescriptorRanges = &constantBufferDescriptorRange;

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.NumParameters = 1;
        rootSignatureDescription.pParameters = &constantBufferParameter;
        rootSignatureDescription.NumStaticSamplers = 0;
        rootSignatureDescription.pStaticSamplers = nullptr;
        rootSignatureDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* rootSignatureData = nullptr;
        D3D_NOT_FAILED(D3D12SerializeRootSignature(&rootSignatureDescription, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureData, nullptr));

        ID3D12RootSignature* rootSignature = nullptr;
        D3D_NOT_FAILED(device->CreateRootSignature(0, rootSignatureData->GetBufferPointer(), rootSignatureData->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

        // pso
        ID3DBlob* vertexShaderBlob = nullptr;
        D3D_NOT_FAILED(D3DCompileFromFile(L"color.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_0", 0, 0, &vertexShaderBlob, nullptr));

        ID3DBlob* pixelShaderBlob = nullptr;
        D3D_NOT_FAILED(D3DCompileFromFile(L"color.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_0", 0, 0, &pixelShaderBlob, nullptr));

        constexpr size_t vertexFieldsCount = 2;
        D3D12_INPUT_ELEMENT_DESC inputElementDescription[vertexFieldsCount];

        inputElementDescription[0].SemanticName = "POSITION";
        inputElementDescription[0].SemanticIndex = 0;
        inputElementDescription[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        inputElementDescription[0].InputSlot = 0;
        inputElementDescription[0].AlignedByteOffset = 0;
        inputElementDescription[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        inputElementDescription[0].InstanceDataStepRate = 0;

        inputElementDescription[1].SemanticName = "COLOR";
        inputElementDescription[1].SemanticIndex = 0;
        inputElementDescription[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        inputElementDescription[1].InputSlot = 0;
        inputElementDescription[1].AlignedByteOffset = 12;
        inputElementDescription[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        inputElementDescription[1].InstanceDataStepRate = 0;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateObjectDescription = {};
        pipelineStateObjectDescription.InputLayout = { inputElementDescription, vertexFieldsCount };
        pipelineStateObjectDescription.pRootSignature = rootSignature;
        pipelineStateObjectDescription.VS = { (BYTE*)vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
        pipelineStateObjectDescription.PS = { (BYTE*)pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

        pipelineStateObjectDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pipelineStateObjectDescription.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        pipelineStateObjectDescription.RasterizerState.FrontCounterClockwise = false;
        pipelineStateObjectDescription.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        pipelineStateObjectDescription.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        pipelineStateObjectDescription.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        pipelineStateObjectDescription.RasterizerState.DepthClipEnable = true;
        pipelineStateObjectDescription.RasterizerState.MultisampleEnable = false;
        pipelineStateObjectDescription.RasterizerState.AntialiasedLineEnable = false;
        pipelineStateObjectDescription.RasterizerState.ForcedSampleCount = 0;
        pipelineStateObjectDescription.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        pipelineStateObjectDescription.BlendState.AlphaToCoverageEnable = false;
        pipelineStateObjectDescription.BlendState.IndependentBlendEnable = false;
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            pipelineStateObjectDescription.BlendState.RenderTarget[i].BlendEnable = false;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].LogicOpEnable = false;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        pipelineStateObjectDescription.DepthStencilState.DepthEnable = true;
        pipelineStateObjectDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pipelineStateObjectDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pipelineStateObjectDescription.DepthStencilState.StencilEnable = false;
        pipelineStateObjectDescription.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        pipelineStateObjectDescription.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

        pipelineStateObjectDescription.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        pipelineStateObjectDescription.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        pipelineStateObjectDescription.SampleMask = UINT_MAX;
        pipelineStateObjectDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateObjectDescription.NumRenderTargets = 1;
        pipelineStateObjectDescription.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipelineStateObjectDescription.SampleDesc.Count = 1;
        pipelineStateObjectDescription.SampleDesc.Quality = 0;
        pipelineStateObjectDescription.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        ID3D12PipelineState* pso = nullptr;
        D3D_NOT_FAILED(device->CreateGraphicsPipelineState(&pipelineStateObjectDescription, IID_PPV_ARGS(&pso)));

        // upload static geometry

        std::array<std::uint16_t, 36> cubeIndices =
        {
            // front face
            0, 1, 2,
            0, 2, 3,

            // back face
            4, 6, 5,
            4, 7, 6,

            // left face
            4, 5, 1,
            4, 1, 0,

            // right face
            3, 2, 6,
            3, 6, 7,

            // top face
            1, 5, 6,
            1, 6, 2,

            // bottom face
            4, 0, 3,
            4, 3, 7
        };

        SIZE_T indexBufferBytes = static_cast<SIZE_T>(cubeIndices.size()) * sizeof(std::uint16_t);

        D3D12_RESOURCE_DESC dataBufferDescription;
        dataBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        dataBufferDescription.Alignment = 0;
        dataBufferDescription.Height = 1;
        dataBufferDescription.DepthOrArraySize = 1;
        dataBufferDescription.MipLevels = 1;
        dataBufferDescription.Format = DXGI_FORMAT_UNKNOWN;

        dataBufferDescription.SampleDesc.Count = 1;
        dataBufferDescription.SampleDesc.Quality = 0;

        dataBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        dataBufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_RESOURCE_BARRIER copyDestBufferTransition;
        copyDestBufferTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        copyDestBufferTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        copyDestBufferTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        copyDestBufferTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        copyDestBufferTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        D3D12_RESOURCE_BARRIER genericReadBufferTransition;
        copyDestBufferTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        copyDestBufferTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        copyDestBufferTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        copyDestBufferTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
        copyDestBufferTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        {
            std::array<XVertex, 8> data =
            {
                XVertex({ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::White) }),
                XVertex({ DirectX::XMFLOAT3(-1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Black) }),
                XVertex({ DirectX::XMFLOAT3(+1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Red) }),
                XVertex({ DirectX::XMFLOAT3(+1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Green) }),
                XVertex({ DirectX::XMFLOAT3(-1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Blue) }),
                XVertex({ DirectX::XMFLOAT3(-1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Yellow) }),
                XVertex({ DirectX::XMFLOAT3(+1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Cyan) }),
                XVertex({ DirectX::XMFLOAT3(+1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Magenta) })
            };

            ID3DBlob* cpuBuffer = nullptr;
            D3D_NOT_FAILED(D3DCreateBlob(static_cast<SIZE_T>(data.size()) * sizeof(XVertex), &cpuBuffer));
            CopyMemory(cpuBuffer->GetBufferPointer(), data.data(), data.size());

            dataBufferDescription.Width = cpuBuffer->GetBufferSize();

            ID3D12Resource* dataUploadBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));
        
            ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&dataBuffer)));
            
            copyDestBufferTransition.Transition.pResource = dataBuffer;
            genericReadBufferTransition.Transition.pResource = dataBuffer;

            commandList->ResourceBarrier(1, &copyDestBufferTransition);

            BYTE* mappedData = nullptr;
            dataUploadBuffer->Map(0, nullptr, (void**)&mappedData);
            memcpy(mappedData, cpuBuffer->GetBufferPointer(), cpuBuffer->GetBufferSize());
            dataUploadBuffer->Unmap(0, nullptr);
            commandList->CopyBufferRegion(dataBuffer, 0, dataUploadBuffer, 0, cpuBuffer->GetBufferSize());

            commandList->Close();

            ID3D12CommandList* cmdsLists[1] = { commandList };
            queue->ExecuteCommandLists(1, cmdsLists);

            currentFenceValue++;
            queue->Signal(fence, currentFenceValue);

            if (fence->GetCompletedValue() != currentFenceValue)
            {
                D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
                WaitForSingleObject(fenceEventHandle, INFINITE);
                ResetEvent(fenceEventHandle);
            }

            D3D_NOT_FAILED(allocator->Reset());
            D3D_NOT_FAILED(commandList->Reset(allocator, nullptr));
        }

        {
            std::array<std::uint16_t, 36> data =
            {
                // front face
                0, 1, 2,
                0, 2, 3,

                // back face
                4, 6, 5,
                4, 7, 6,

                // left face
                4, 5, 1,
                4, 1, 0,

                // right face
                3, 2, 6,
                3, 6, 7,

                // top face
                1, 5, 6,
                1, 6, 2,

                // bottom face
                4, 0, 3,
                4, 3, 7
            };

            ID3DBlob* cpuBuffer = nullptr;
            D3D_NOT_FAILED(D3DCreateBlob(static_cast<SIZE_T>(data.size()) * sizeof(std::uint16_t), &cpuBuffer));
            CopyMemory(cpuBuffer->GetBufferPointer(), data.data(), data.size());

            dataBufferDescription.Width = cpuBuffer->GetBufferSize();

            static ID3D12Resource* dataUploadBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));

            static ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&dataBuffer)));

            copyDestBufferTransition.Transition.pResource = dataBuffer;
            genericReadBufferTransition.Transition.pResource = dataBuffer;

            commandList->ResourceBarrier(1, &copyDestBufferTransition);

            BYTE* mappedData = nullptr;
            dataUploadBuffer->Map(0, nullptr, (void**)&mappedData);
            memcpy(mappedData, cpuBuffer->GetBufferPointer(), cpuBuffer->GetBufferSize());
            dataUploadBuffer->Unmap(0, nullptr);
            commandList->CopyBufferRegion(dataBuffer, 0, dataUploadBuffer, 0, cpuBuffer->GetBufferSize());

            commandList->Close();

            ID3D12CommandList* cmdsLists[1] = { commandList };
            queue->ExecuteCommandLists(1, cmdsLists);

            currentFenceValue++;
            queue->Signal(fence, currentFenceValue);

            if (fence->GetCompletedValue() != currentFenceValue)
            {
                D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
                WaitForSingleObject(fenceEventHandle, INFINITE);
                ResetEvent(fenceEventHandle);
            }

            D3D_NOT_FAILED(allocator->Reset());
            D3D_NOT_FAILED(commandList->Reset(allocator, nullptr));
        }

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

            int32_t indices_offset = 0;
            for (size_t i = 0; i < models.size(); i++)
            {
                CurrentModelIndex = static_cast<uint32_t>(i);
                const Model& model = models.at(i);
                for (uint32_t i = 0; i < model.indices_count / 3; i++)
                {
                    // draw triangle - not implemented
                }
                indices_offset += model.indices_count;
            }

            ID3D12Resource* currentBuffer = isZeroBufferInTheBack ? zeroBuffer : firstBuffer;
            D3D12_CPU_DESCRIPTOR_HANDLE* currentBufferHandle = isZeroBufferInTheBack ? &zeroBufferHandle : &firstBufferHandle;

            D3D12_RESOURCE_BARRIER renderTargetBufferTransition;
            renderTargetBufferTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            renderTargetBufferTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            renderTargetBufferTransition.Transition.pResource = currentBuffer;
            renderTargetBufferTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            renderTargetBufferTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            renderTargetBufferTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &renderTargetBufferTransition);

            D3D12_VIEWPORT viewport;
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = GameWidth;
            viewport.Height = GameHeight;
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 0.0f;

            commandList->RSSetViewports(1, &viewport);

            D3D12_RECT scissor;
            scissor.left = 0;
            scissor.top = 0;
            scissor.bottom = GameHeight;
            scissor.right = GameWidth;

            commandList->RSSetScissorRects(1, &scissor);
            commandList->OMSetRenderTargets(1, currentBufferHandle, true, &depthBufferHandle);

            FLOAT clearColor[4] = { 1.0f, 0.f, 0.f, 1.000000000f };
            commandList->ClearRenderTargetView(*currentBufferHandle, clearColor, 0, nullptr);
            commandList->ClearDepthStencilView(depthBufferHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

            D3D12_RESOURCE_BARRIER presentBufferTransition;
            presentBufferTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            presentBufferTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            presentBufferTransition.Transition.pResource = currentBuffer;
            presentBufferTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            presentBufferTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            presentBufferTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &presentBufferTransition);
            commandList->Close();

            ID3D12CommandList* cmdsLists[1] = { commandList };
            queue->ExecuteCommandLists(1, cmdsLists);

            D3D_NOT_FAILED(swapChain->Present(0, 0));
            isZeroBufferInTheBack = !isZeroBufferInTheBack;

            currentFenceValue++;
            queue->Signal(fence, currentFenceValue);

            if (fence->GetCompletedValue() != currentFenceValue)
            {
                D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
                WaitForSingleObject(fenceEventHandle, INFINITE);
                ResetEvent(fenceEventHandle);
            }

            // clean stuff
            D3D_NOT_FAILED(allocator->Reset());
            D3D_NOT_FAILED(commandList->Reset(allocator, nullptr));

            auto frameActualTime = frameStart - chrono::steady_clock::now();
            auto timeLeft = frameExpectedTime - frameActualTime;

            if (timeLeft > 0ms) 
            {
                this_thread::sleep_for(timeLeft);
            }
        }

        // release hepas and render targets
        CloseHandle(fenceEventHandle);
        swapChain->Release();
        commandList->Release();
        allocator->Release();
        queue->Release();
        device->Release();
        stbi_image_free(Texture);
    }

    return 0;
}