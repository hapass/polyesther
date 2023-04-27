#pragma once

#include <stdint.h>
#include <vector>
#include <string>

namespace Renderer
{
    struct Texture
    {
        constexpr static size_t BytesPerColor = 4;

        Texture(size_t width, size_t height);
        Texture();

        uint8_t* GetBuffer();
        const uint8_t* GetBuffer() const;
        size_t GetByteSize() const;
        size_t GetWidth() const;
        size_t GetHeight() const;
        size_t GetSize() const;

    private:
        size_t width = 0;
        size_t height = 0;
        std::vector<uint8_t> data;
    };

    bool Load(const std::string& path, Texture& texture);

    bool Save(const std::string& path, const Texture& texture);
}