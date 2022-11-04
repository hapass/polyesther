#pragma once

#include <stdint.h>

namespace Renderer
{
    struct Matrix
    {
        Matrix();
        float m[16U];
    };

    const Matrix& rotateZ(float alpha);
    const Matrix& rotateY(float alpha);
    const Matrix& rotateX(float alpha);
    const Matrix& translate(float x, float y, float z);
    const Matrix& scale(float x);
    const Matrix& transpose(const Matrix& m);

    Matrix operator*(const Matrix& a, const Matrix& b);

    struct Vec
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        float Get(int32_t index) const;
        void Set(int32_t index, float val);
    };

    bool operator<(const Vec& lhs, const Vec& rhs);

    float dot(const Vec& a, const Vec& b);

    Vec operator*(const Vec& a, const Vec& b);

    Vec operator+(const Vec& a, const Vec& b);

    Vec operator-(const Vec& b);

    Vec operator-(const Vec& a, const Vec& b);

    Vec operator*(const Matrix& m, const Vec& v);

    Vec operator*(const Vec& a, float b);

    Vec normalize(const Vec& v);

    Vec cross(const Vec& a, const Vec& b);

    Vec reflect(const Vec& normal, const Vec& vec);
}