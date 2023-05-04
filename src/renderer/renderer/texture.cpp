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

    Color Texture::GetColor(size_t index) const
    {
        return Color(data.at(index * BytesPerColor), data.at(index * BytesPerColor + 1), data.at(index * BytesPerColor + 2));
    }

    void Texture::SetColor(size_t index, uint32_t rgba)
    {
        uint8_t r = static_cast<uint8_t>(rgba & 0x000000FF);
        uint8_t g = static_cast<uint8_t>(rgba & 0x0000FF00 >> 8);
        uint8_t b = static_cast<uint8_t>(rgba & 0x00FF0000 >> 16);
        uint8_t a = static_cast<uint8_t>(rgba & 0xFF000000 >> 24);

        data[index * BytesPerColor] = r;
        data[index * BytesPerColor + 1] = g;
        data[index * BytesPerColor + 2] = b;
        data[index * BytesPerColor + 3] = a;
    }

    size_t Texture::GetSize() const
    {
        return width * height;
    }

    bool Texture::operator==(const Texture& other)
    {
        return this->width == other.width && this->height == other.height && this->data == other.data;
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
        return static_cast<bool>(stbi_write_bmp(path.c_str(), static_cast<int>(texture.GetWidth()), static_cast<int>(texture.GetHeight()), Texture::BytesPerColor, texture.GetBuffer()));
    }
}