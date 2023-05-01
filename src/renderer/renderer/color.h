#pragma once

#include <renderer/math.h>
#include <stdint.h>

namespace Renderer
{
    struct Color
    {
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0) : rgba(0u), rgba_vec{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f }
        {
            rgba |= a << 24;
            rgba |= r << 16;
            rgba |= g << 8;
            rgba |= b << 0;
        }

        Color() : Color(0, 0, 0) {}

        const Vec& GetVec() { return rgba_vec; }

        uint32_t rgba;
        Vec rgba_vec;

        static Color Black;
        static Color White;
        static Color Red;
        static Color Green;
        static Color Blue;
    };
}