#pragma once

#include <renderer/math.h>
#include <renderer/color.h>
#include <vector>

namespace Renderer
{
    struct Camera
    {
        Vec position;
        float pitch = 0.0f; // around x while at 0
        float yaw = 0.0f; // around z while at 0

        Vec forward;
        Vec left;
    };

    struct Vertex
    {
        uint32_t materialId;
        Vec position;
        Vec normal;
        Vec textureCoord;
        Color color;
    };

    struct Material
    {
        uint32_t id;
        std::string textureName;
    };

    struct Model
    {
        Vec position;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct Light
    {
        Vec position;
        Vec position_view;

        float ambientStrength = 0.1f;
        float specularStrength = 0.5f;
        float specularShininess = 32.0f;
        Color color = Color::White;
    };

    struct Scene
    {
        std::vector<Model> models;
    };

    bool Load(const char* fileName, Scene& scene);
}