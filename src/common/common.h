#pragma once

/*
* TODO:
* Refactoring.
* Optimization.
* Render to image.
* One interface for rendering to image.
* Write DX12 renderer that renders the same image.
*/

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

struct Model {
    uint32_t indices_count;
    uint32_t vertices_count;
    uint32_t texture_count;
    uint32_t normal_count;

    Vec position;
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
    Color(uint8_t r, uint8_t g, uint8_t b) : rgb(0u), rgb_vec{ r / 255.0f, g / 255.0f, b / 255.0f, 0.0f }
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

const Matrix& perspective(float width, float height)
{
    static float Far = 400.0f;
    static float Near = 1.0f;

    float farPlane = Far;
    float nearPlane = Near;
    float halfFieldOfView = 25 * (static_cast<float>(M_PI) / 180);
    float aspect = width / height;

    static Matrix m;

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

Matrix view(const Camera& camera)
{
    Matrix rTranslate = translate(-camera.position.x, -camera.position.y, -camera.position.z);
    Matrix rYaw = transpose(rotateY(camera.yaw));
    Matrix rPitch = transpose(rotateX(camera.pitch));

    return rPitch * rYaw * rTranslate;
}

Matrix modelMat(const Model& model)
{
    return translate(model.position.x, model.position.y, model.position.z);
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

Model LoadOBJ(const char* fileName, float modelScale, const Vec& position, LoadContext& context)
{
    Model model;
    model.indices_count = 0;
    model.vertices_count = 0;
    model.texture_count = 0;
    model.normal_count = 0;
    model.position = position;

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
                        verticesObj.push_back(translate(model.position.x, model.position.y, model.position.z) * scale(modelScale) * Vec { x, y, z, 1.0f });
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

class Scene
{
    Camera camera;
    std::vector<Model> models;
    std::vector<Light> lights;
};

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