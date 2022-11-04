#pragma once

#include <renderer/math.h>
#include <stdint.h>

namespace Renderer
{
    struct Color
    {
        Color(uint8_t r, uint8_t g, uint8_t b) : rgb(0u), rgb_vec{ r / 255.0f, g / 255.0f, b / 255.0f, 0.0f }
        {
            rgb |= r << 16;
            rgb |= g << 8;
            rgb |= b << 0;
        }

        Color() : Color(0, 0, 0) {}

        const Vec& GetVec() { return rgb_vec; }

        uint32_t rgb;
        Vec rgb_vec;

        static Color Black;
        static Color White;
        static Color Red;
        static Color Green;
        static Color Blue;
    };
}