#pragma once

#include <renderer/math.h>
#include <stdint.h>

namespace Renderer
{
    struct Color
    {
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : rgba(0u), rgba_vec{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f }
        {
            rgba |= r << 24;
            rgba |= g << 16;
            rgba |= b << 8;
            rgba |= a << 0;
        }

        Color(uint32_t rgba) : rgba(rgba)
        {
            rgba_vec = { GetVal(0) / 255.0f, GetVal(1) / 255.0f, GetVal(2) / 255.0f, GetVal(3) / 255.0f };
        }

        Color(const Vec& rgba_vec) : rgba(0u), rgba_vec(rgba_vec)
        {
            rgba |= uint8_t(rgba_vec.x * 255) << 24;
            rgba |= uint8_t(rgba_vec.y * 255) << 16;
            rgba |= uint8_t(rgba_vec.z * 255) << 8;
            rgba |= uint8_t(rgba_vec.w * 255) << 0;
        }

        Color() : Color(0, 0, 0) {}

        const Vec& GetVec() const 
        { 
            return rgba_vec;
        }

        const uint8_t GetVal(size_t index) const
        {
            switch (index)
            {
                case 0: return static_cast<uint8_t>((rgba & 0xFF000000) >> 24);
                case 1: return static_cast<uint8_t>((rgba & 0x00FF0000) >> 16);
                case 2: return static_cast<uint8_t>((rgba & 0x0000FF00) >> 8);
                default: return static_cast<uint8_t>((rgba & 0x000000FF) >> 0);
            }
        }

        uint32_t rgba;
        Vec rgba_vec;

        static Color Black;
        static Color White;
        static Color Red;
        static Color Green;
        static Color Blue;
        static Color Pink;
    };
}