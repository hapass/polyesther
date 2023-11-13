#pragma once

#include <stdint.h>
#include <vector>
#include <string>

namespace Renderer
{
    struct Color;

    struct Texture
    {
        constexpr static size_t BytesPerColor = 4;

        explicit Texture(size_t width, size_t height);
        explicit Texture();

        uint8_t* GetBuffer();
        const uint8_t* GetBuffer() const;
        size_t GetByteSize() const;
        size_t GetWidth() const;
        size_t GetHeight() const;

        Color GetColor(size_t index) const;
        void SetColor(size_t index, Color color);

        size_t GetSize() const;

        bool operator==(const Texture& rhs) const;

    private:
        size_t width = 0;
        size_t height = 0;
        std::vector<uint8_t> data;
    };

    bool Load(const std::string& path, Texture& texture);

    bool Save(const std::string& path, const Texture& texture);

    bool Diff(const Texture& lhs, const Texture& rhs, Texture& result, uint32_t& differentPixelsCount);
}