#pragma once

#include <renderer/math.h>
#include <renderer/color.h>
#include <string>
#include <vector>

namespace Renderer
{
    struct Vertex
    {
        int32_t materialId = -1;
        Vec position;
        Vec normal;
        Vec textureCoord;
        Color color;
    };

    bool operator<(const Vertex& lhs, const Vertex& rhs);
    bool operator==(const Vertex& lhs, const Vertex& rhs);

    struct Material
    {
        std::string name;
        std::string textureName;
    };

    struct Model
    {
        Vec position;
        std::vector<Material> materials;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        bool backfaceCulling = true;
    };

    struct Light
    {
        Vec position;

        float ambientStrength = 0.0f; // todo.pavelza: these are picked up from scene description on software, but not on hardware!
        float specularStrength = 0.0f;
        float specularShininess = 0.0f;
        Color color = Color::White;
    };

    struct Camera
    {
        Vec position;
        float pitch = 0.0f; // around x while at 0 position
        float yaw = 0.0f; // around z while at 0 position

        const Vec forward { 0.0f, 0.0f, -1.0f, 0.0f }; // initial forward when pitch and yaw are 0
        const Vec left { -1.0f, 0.0f, 0.0f, 0.0f };
    };

    struct Scene
    {
        Light light;
        Camera camera;
        std::vector<Model> models;
    };

    bool Load(const std::string& fullFileName, Scene& scene);

    // In view space we are at 0 looking down the negative z axis.
    // Near plane of the camera frustum is at -Near, far plane of the camera frustum is at -Far.
    // As DirectX clip space z axis ranges from 0 to 1, we map -Near to 0 and -Far to 1.
    // The coordinate system is right handed.
    Matrix PerspectiveTransform(float width, float height);

    Matrix ViewTransform(const Camera& camera);

    Matrix ModelTransform(const Model& model);

    Matrix CameraTransform(const Camera& camera);
}