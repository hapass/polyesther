#include <renderer/math.h>
#include <cassert>
#include <tuple>

using namespace std;

namespace Renderer
{
    Matrix::Matrix()
    {
        for (uint32_t i = 0; i < 16U; i++)
        {
            m[i] = 0;
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

    float dot(const Vec& a, const Vec& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    float Vec::Get(int32_t index) const
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

    void Vec::Set(int32_t index, float val)
    {
        assert(0 <= index && index <= 3);

        switch (index)
        {
        case 0: x = val; return;
        case 1: y = val; return;
        case 2: z = val; return;
        }
        w = val;
    }

    bool operator<(const Vec& lhs, const Vec& rhs)
    {
        return std::tie(lhs.x, lhs.y, lhs.z, lhs.w) <
            std::tie(rhs.x, rhs.y, rhs.z, rhs.w);
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

    bool operator==(const Vec& a, const Vec& b)
    {
        return !(a < b) && !(b < a);
    }

    Vec normalize(const Vec& v)
    {
        float norm = 1.0f / sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
        return Vec{ v.x * norm, v.y * norm, v.z * norm, v.w * norm };
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
        return (vec - (normal * (dot(vec, normal) / dot(normal, normal))) * 2) * Vec(-1.0f, -1.0f, -1.0f, 0.0f);
    }
}