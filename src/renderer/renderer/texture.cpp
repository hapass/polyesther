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

    void Texture::SetColor(size_t index, Color color)
    {
        data[index * BytesPerColor] = color.GetVal(0);
        data[index * BytesPerColor + 1] = color.GetVal(1);
        data[index * BytesPerColor + 2] = color.GetVal(2);
        data[index * BytesPerColor + 3] = color.GetVal(3);
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