#pragma once

#include <renderer/texture.h>
#include <renderer/color.h>

#define STB_IMAGE_IMPLEMENTATION
#include <renderer/thirdparty/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <renderer/thirdparty/stb_image_write.h>

namespace Renderer
{
    Texture::Texture(size_t width, size_t height)
        : width(width)
        , height(height)
        , data(width * height * BytesPerColor)
    {
    }

    Texture::Texture()
    {
    }

    uint8_t* Texture::GetBuffer()
    {
        return data.data();
    }

    const uint8_t* Texture::GetBuffer() const
    {
        return data.data();
    }

    size_t Texture::GetByteSize() const
    {
        return data.size();
    }

    size_t Texture::GetWidth() const
    {
        return width;
    }

    size_t Texture::GetHeight() const
    {
        return height;
    }

    Color Texture::GetColorAt(size_t index) const
    {
        return Color(data.at(index * BytesPerColor), data.at(index * BytesPerColor + 1), data.at(index * BytesPerColor + 2));
    }

    size_t Texture::GetSize() const
    {
        return width * height;
    }

    bool Load(const std::string& path, Texture& texture)
    {
        int32_t width, height, channels;
        if (uint8_t* result = stbi_load(path.c_str(), &width, &height, &channels, Texture::BytesPerColor))
        {
            Texture temp(width, height);
            std::memcpy(temp.GetBuffer(), result, temp.GetByteSize());
            stbi_image_free(result);
            texture = std::move(temp);
            return true;
        }

        return false;
    };

    bool Save(const std::string& path, const Texture& texture)
    {
        return static_cast<bool>(stbi_write_jpg(path.c_str(), static_cast<int>(texture.GetWidth()), static_cast<int>(texture.GetHeight()), Texture::BytesPerColor, texture.GetBuffer(), 100));
    }
}