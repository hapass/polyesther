#pragma once

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
    return Vec{ a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w };
}

Vec operator+(const Vec& a, const Vec& b)
{
    return Vec{ a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

Vec operator-(const Vec& b)
{
    return Vec{ -b.x, -b.y, -b.z, -b.w };
}

Vec operator-(const Vec& a, const Vec& b)
{
    return a + (-b);
}

Vec operator*(const Matrix& m, const Vec& v)
{
    float x = dot(Vec{ m.m[0],  m.m[1],  m.m[2],  m.m[3] }, v);
    float y = dot(Vec{ m.m[4],  m.m[5],  m.m[6],  m.m[7] }, v);
    float z = dot(Vec{ m.m[8],  m.m[9],  m.m[10], m.m[11] }, v);
    float w = dot(Vec{ m.m[12], m.m[13], m.m[14], m.m[15] }, v);
    return Vec{ x, y, z, w };
}

Vec operator*(const Vec& a, float b)
{
    return Vec{ a.x * b, a.y * b, a.z * b, a.w * b };
}

Vec normalize(const Vec& v)
{
    float norm = sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    return Vec{ v.x / norm, v.y / norm, v.z / norm, v.w / norm };
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