#include <renderer/renderersoftware.h>

#define _USE_MATH_DEFINES

#include <stdint.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cassert>
#include <iostream>
#include "utils.h"

namespace Renderer
{
    namespace
    {
        int32_t OutputWidth;
        int32_t OutputHeight;

        std::vector<uint32_t> BackBuffer;
        std::vector<float> ZBuffer;
        std::vector<Texture> Textures;
        uint32_t CurrentTexture = 0;
    }

    struct LightS
    {
        Light light;
        Vec position_view;
    };

    namespace
    {
        LightS light;
    }

    struct VertexS
    {
        Vertex v;
        Vec pos_view;

        // todo: need to get rid of these, just needed for easier interpolation
        float red;
        float green;
        float blue;
    };

    void DrawPixel(int32_t x, int32_t y, Color color)
    {
        assert(OutputWidth > 0);
        assert(OutputHeight > 0);
        assert(0 <= x && x < OutputWidth);
        assert(0 <= y && y < OutputHeight);
        assert(BackBuffer);
        BackBuffer[y * OutputWidth + x] = color.rgba;
    }

    void ClearScreen(Color color)
    {
        assert(OutputWidth > 0);
        assert(OutputHeight > 0);
        assert(BackBuffer);

        for (uint32_t i = 0; i < OutputWidth * OutputHeight; i++)
        {
            BackBuffer[i] = color.rgba;
            ZBuffer[i] = 2.0f;
        }
    }

    struct InterpolationPoint
    {
        float x;
        float y;
        float c;
    };

    struct Interpolant
    {
        Interpolant(const InterpolationPoint& a, const InterpolationPoint& b, const InterpolationPoint& c)
        {
            float dx = (b.x - c.x) * (a.y - c.y) - (a.x - c.x) * (b.y - c.y);
            float dy = -dx;

            stepX = ((b.c - c.c) * (a.y - c.y) - (a.c - c.c) * (b.y - c.y)) / dx;
            stepY = ((b.c - c.c) * (a.x - c.x) - (a.c - c.c) * (b.x - c.x)) / dy;

            minC = a.c;
            midC = b.c;
        }

        float stepX;
        float stepY;
        float currentC = 0.0f;

        float minC;
        float midC;

        void CalculateC(float dx, float dy, bool isMin)
        {
            currentC = (isMin ? minC : midC) + dy * stepY + dx * stepX;
        }
    };

    struct Edge
    {
        Edge(Vec b, Vec e, std::vector<Interpolant>& inter, bool isMin = true) : interpolants(inter), begin(b), isBeginMin(isMin)
        {
            pixelYBegin = static_cast<int32_t>(ceil(b.y));
            pixelYEnd = static_cast<int32_t>(ceil(e.y));

            float distanceX = e.x - b.x;
            float distanceY = e.y - b.y;
            stepX = distanceY > 0.0f ? distanceX / distanceY : 0.0f;
        }

        void CalculateX(int32_t y)
        {
            pixelX = static_cast<int32_t>(ceil(begin.x + (y - begin.y) * stepX));
            for (Interpolant& i : interpolants)
            {
                i.CalculateC(pixelX - begin.x, y - begin.y, isBeginMin);
            }
        }

        int32_t pixelYBegin;
        int32_t pixelYEnd;
        int32_t pixelX;

        float stepX;
        bool isBeginMin;
        Vec begin;

        std::vector<Interpolant> interpolants;
    };

    float Lerp(float begin, float end, float lerpAmount)
    {
        return begin + (end - begin) * lerpAmount;
    }

    void DrawTrianglePart(Edge& minMax, Edge& other)
    {
        for (int32_t y = other.pixelYBegin; y < other.pixelYEnd; y++)
        {
            Edge* left = &minMax;
            Edge* right = &other;

            left->CalculateX(y);
            right->CalculateX(y);

            if (left->pixelX > right->pixelX)
            {
                Edge* temp = left;
                left = right;
                right = temp;
            }

            std::vector<float> interpolants_raw;
            interpolants_raw.resize(left->interpolants.size());

            for (int32_t x = left->pixelX; x < right->pixelX; x++)
            {
                float percent = static_cast<float>(x - left->pixelX) / static_cast<float>(right->pixelX - left->pixelX);

                interpolants_raw[6] = Lerp(left->interpolants[6].currentC, right->interpolants[6].currentC, percent);
                if (interpolants_raw[6] < ZBuffer[y * OutputWidth + x])
                {
                    ZBuffer[y * OutputWidth + x] = interpolants_raw[6];
                }
                else
                {
                    continue;
                }

                for (uint32_t i = 0; i < interpolants_raw.size(); i++)
                {
                    interpolants_raw[i] = Lerp(left->interpolants[i].currentC, right->interpolants[i].currentC, percent);
                }

                float tintRed = interpolants_raw[0] / interpolants_raw[5];
                float tintGreen = interpolants_raw[1] / interpolants_raw[5];
                float tintBlue = interpolants_raw[2] / interpolants_raw[5];

                float normalX = interpolants_raw[7] / interpolants_raw[5];
                float normalY = interpolants_raw[8] / interpolants_raw[5];
                float normalZ = interpolants_raw[9] / interpolants_raw[5];

                float viewX = interpolants_raw[10] / interpolants_raw[5];
                float viewY = interpolants_raw[11] / interpolants_raw[5];
                float viewZ = interpolants_raw[12] / interpolants_raw[5];


                Vec pos_view{ viewX, viewY, viewZ, 1.0f };
                Vec normal_vec = normalize({ normalX, normalY, normalZ, 0.0f });
                Vec light_vec = normalize(light.position_view - pos_view);

                Vec diffuse = light.light.color.GetVec() * static_cast<float>(std::max(dot(normal_vec, light_vec), 0.0f));
                Vec ambient = light.light.color.GetVec() * light.light.ambientStrength;

                float specAmount = static_cast<float>(std::max(dot(normalize(pos_view), reflect(normal_vec, light_vec * -1.0f)), 0.0f));
                Vec specular = light.light.color.GetVec() * pow(specAmount, light.light.specularShininess) * light.light.specularStrength;

                uint32_t materialId = CurrentTexture;

                // From 0 to TextureWidth - 1 (TextureWidth pixels in total)
                int32_t textureX = static_cast<int32_t>((interpolants_raw[3] / interpolants_raw[5]) * (Textures[materialId].GetWidth() - 1));
                // From 0 to TextureHeight - 1 (TextureHeight pixels in total)
                int32_t textureY = static_cast<int32_t>((interpolants_raw[4] / interpolants_raw[5]) * (Textures[materialId].GetHeight() - 1));

                textureY = (Textures[materialId].GetHeight() - 1) - textureY; // invert texture coords

                assert(textureY < Textures[materialId].GetHeight() && textureX < Textures[materialId].GetWidth());
                int32_t texelBase = textureY * Textures[materialId].GetWidth() + textureX;

                Color texture_color = Textures[materialId].GetColor(texelBase);

                Vec frag_color = texture_color.GetVec();
                Vec final_color = (diffuse + ambient + specular) * frag_color;
                final_color.x = std::clamp(final_color.x, 0.0f, 1.0f);
                final_color.y = std::clamp(final_color.y, 0.0f, 1.0f);
                final_color.z = std::clamp(final_color.z, 0.0f, 1.0f);
                final_color = final_color * 255.0f;

                DrawPixel(x, y, Color(
                    static_cast<uint8_t>(final_color.x),
                    static_cast<uint8_t>(final_color.y),
                    static_cast<uint8_t>(final_color.z)
                ));
            }
        }
    }

    VertexS Lerp(const VertexS& begin, const VertexS& end, float lerpAmount)
    {
        VertexS result;
        result.v.position.x = Lerp(begin.v.position.x, end.v.position.x, lerpAmount);
        result.v.position.y = Lerp(begin.v.position.y, end.v.position.y, lerpAmount);
        result.v.position.z = Lerp(begin.v.position.z, end.v.position.z, lerpAmount);
        result.v.position.w = Lerp(begin.v.position.w, end.v.position.w, lerpAmount);
        result.pos_view.x = Lerp(begin.pos_view.x, end.pos_view.x, lerpAmount);
        result.pos_view.y = Lerp(begin.pos_view.y, end.pos_view.y, lerpAmount);
        result.pos_view.z = Lerp(begin.pos_view.z, end.pos_view.z, lerpAmount);
        result.pos_view.w = Lerp(begin.pos_view.w, end.pos_view.w, lerpAmount);
        result.v.textureCoord.x = Lerp(begin.v.textureCoord.x, end.v.textureCoord.x, lerpAmount);
        result.v.textureCoord.y = Lerp(begin.v.textureCoord.y, end.v.textureCoord.y, lerpAmount);
        result.red = Lerp(begin.red, end.red, lerpAmount);
        result.green = Lerp(begin.green, end.green, lerpAmount);
        result.blue = Lerp(begin.blue, end.blue, lerpAmount);
        result.v.normal.x = Lerp(begin.v.normal.x, end.v.normal.x, lerpAmount);
        result.v.normal.y = Lerp(begin.v.normal.y, end.v.normal.y, lerpAmount);
        result.v.normal.z = Lerp(begin.v.normal.z, end.v.normal.z, lerpAmount);
        result.v.normal.w = Lerp(begin.v.normal.w, end.v.normal.w, lerpAmount);
        return result;
    }

    Interpolant GetInterpolant(std::vector<VertexS>& vertices, float c1, float c2, float c3)
    {
        return Interpolant(
            { vertices[0].v.position.x, vertices[0].v.position.y, c1 / vertices[0].v.position.w },
            { vertices[1].v.position.x, vertices[1].v.position.y, c2 / vertices[1].v.position.w },
            { vertices[2].v.position.x, vertices[2].v.position.y, c3 / vertices[2].v.position.w }
        );
    }

    void DrawRawTriangle(std::vector<VertexS>& vertices)
    {
        assert(vertices.size() == 3);

        for (VertexS& v : vertices)
        {
            v.v.position.x /= v.v.position.w;
            v.v.position.y /= v.v.position.w;
            v.v.position.z /= v.v.position.w;
        }

        // todo: backface culling
        //if (cross(vertices[1].pos_view - vertices[0].pos_view, vertices[1].pos_view - vertices[2].pos_view).z >= 0)
        //{
        //    return;
        //}

        std::sort(std::begin(vertices), std::end(vertices), [](const VertexS& lhs, const VertexS& rhs) { return lhs.v.position.y < rhs.v.position.y; });

        for (VertexS& v : vertices)
        {
            // TODO.PAVELZA: Clipping might result in some vertices being slightly outside of -1 to 1 range, so we clamp. Will need to think how to avoid this.
            v.v.position.x = std::clamp(v.v.position.x, -1.0f, 1.0f);
            v.v.position.y = std::clamp(v.v.position.y, -1.0f, 1.0f);
            v.v.position.z = std::clamp(v.v.position.z, -1.0f, 1.0f);

            v.v.position.x = (OutputWidth - 1) * ((v.v.position.x + 1) / 2.0f);
            v.v.position.y = (OutputHeight - 1) * ((v.v.position.y + 1) / 2.0f);
        }

        // No division by w, because it is already divided by w.
        Interpolant z({ vertices[0].v.position.x, vertices[0].v.position.y, vertices[0].v.position.z }, { vertices[1].v.position.x, vertices[1].v.position.y, vertices[1].v.position.z }, { vertices[2].v.position.x, vertices[2].v.position.y, vertices[2].v.position.z });

        Interpolant red = GetInterpolant(vertices, vertices[0].red, vertices[1].red, vertices[2].red);
        Interpolant green = GetInterpolant(vertices, vertices[0].green, vertices[1].green, vertices[2].green);
        Interpolant blue = GetInterpolant(vertices, vertices[0].blue, vertices[1].blue, vertices[2].blue);

        Interpolant xTexture = GetInterpolant(vertices, vertices[0].v.textureCoord.x, vertices[1].v.textureCoord.x, vertices[2].v.textureCoord.x);
        Interpolant yTexture = GetInterpolant(vertices, vertices[0].v.textureCoord.y, vertices[1].v.textureCoord.y, vertices[2].v.textureCoord.y);

        Interpolant oneOverW = GetInterpolant(vertices, 1.0f, 1.0f, 1.0f);

        Interpolant xNormal = GetInterpolant(vertices, vertices[0].v.normal.x, vertices[1].v.normal.x, vertices[2].v.normal.x);
        Interpolant yNormal = GetInterpolant(vertices, vertices[0].v.normal.y, vertices[1].v.normal.y, vertices[2].v.normal.y);
        Interpolant zNormal = GetInterpolant(vertices, vertices[0].v.normal.z, vertices[1].v.normal.z, vertices[2].v.normal.z);

        Interpolant xView = GetInterpolant(vertices, vertices[0].pos_view.x, vertices[1].pos_view.x, vertices[2].pos_view.x);
        Interpolant yView = GetInterpolant(vertices, vertices[0].pos_view.y, vertices[1].pos_view.y, vertices[2].pos_view.y);
        Interpolant zView = GetInterpolant(vertices, vertices[0].pos_view.z, vertices[1].pos_view.z, vertices[2].pos_view.z);

        std::vector<Interpolant> interpolants{ red, green, blue, xTexture, yTexture, oneOverW, z, xNormal, yNormal, zNormal, xView, yView, zView };

        Edge minMax(vertices[0].v.position, vertices[2].v.position, interpolants, true);
        Edge minMiddle(vertices[0].v.position, vertices[1].v.position, interpolants, true);
        Edge middleMax(vertices[1].v.position, vertices[2].v.position, interpolants, false);

        DrawTrianglePart(minMax, minMiddle);
        DrawTrianglePart(minMax, middleMax);
    }

    bool IsVertexInside(const VertexS& point, int32_t axis, int32_t plane)
    {
        return point.v.position.Get(axis) * plane <= point.v.position.w;
    }

    void ClipTrianglePlane(std::vector<VertexS>& vertices, int32_t axis, int32_t plane)
    {
        std::vector<VertexS> result;
        size_t previousElement = vertices.size() - 1;

        for (size_t currentElement = 0; currentElement < vertices.size(); currentElement++)
        {
            bool isPreviousInside = IsVertexInside(vertices[previousElement], axis, plane);
            bool isCurrentInside = IsVertexInside(vertices[currentElement], axis, plane);

            if (isPreviousInside != isCurrentInside)
            {
                float k = (vertices[previousElement].v.position.w - vertices[previousElement].v.position.Get(axis) * plane);
                float lerpAmount = k / (k - vertices[currentElement].v.position.w + vertices[currentElement].v.position.Get(axis) * plane);
                result.push_back(Lerp(vertices[previousElement], vertices[currentElement], lerpAmount));
            }

            if (isCurrentInside)
            {
                result.push_back(vertices[currentElement]);
            }

            previousElement = currentElement;
        }

        swap(vertices, result);
    }

    bool ClipTriangleAxis(std::vector<VertexS>& vertices, int32_t axis)
    {
        ClipTrianglePlane(vertices, axis, 1);

        if (vertices.size() == 0)
        {
            return false;
        }

        ClipTrianglePlane(vertices, axis, -1);

        return vertices.size() != 0;
    }

    void DrawTriangle(const Vertex& a, const Vertex& b, const Vertex& c, const Matrix& transform)
    {
        std::vector<VertexS> vertices
        {
            VertexS
            {
                a,
                a.position,
                a.color.GetVec().x,
                a.color.GetVec().y,
                a.color.GetVec().z
            },
            VertexS
            {
                b,
                b.position,
                b.color.GetVec().x,
                b.color.GetVec().y,
                b.color.GetVec().z
            },
            VertexS
            {
                c,
                c.position,
                c.color.GetVec().x,
                c.color.GetVec().y,
                c.color.GetVec().z
            }
        };

        for (VertexS& v : vertices)
        {
            v.v.position = PerspectiveTransform(static_cast<float>(OutputWidth), static_cast<float>(OutputHeight)) * transform * v.v.position;
            v.pos_view = transform * v.pos_view;
            // This is possible because we do not do non-uniform scale in transform. If we are about to do non-uniform scale, we should calculate the normal matrix.
            v.v.normal = transform * v.v.normal;
        }

        // TODO.PAVELZA: If all the points are outside of the view frustum - we can cull immediately.
        // TODO.PAVELZA: If all points are inside - we can draw immediately.

        if (ClipTriangleAxis(vertices, 0) && ClipTriangleAxis(vertices, 1) && ClipTriangleAxis(vertices, 2))
        {
            assert(vertices.size() >= 3);

            for (size_t i = 2; i < vertices.size(); i++)
            {
                std::vector<VertexS> triangle;
                triangle.resize(3);

                triangle[0] = vertices[0];
                triangle[1] = vertices[i - 1];
                triangle[2] = vertices[i];

                DrawRawTriangle(triangle);
            }
        }
    }

    bool RendererSoftware::Render(const Scene& scene, Texture& texture)
    {
        OutputWidth = texture.GetWidth();
        OutputHeight = texture.GetHeight();

        BackBuffer.resize(OutputWidth * OutputHeight);
        ZBuffer.resize(OutputWidth * OutputHeight);
        ClearScreen(Color::Black);

        // calculate light's position in view space
        light.position_view = ViewTransform(scene.camera) * scene.light.position;
        light.light = scene.light;

        if (Textures.size() == 0)
        {
            Textures.resize(2);
            Load(scene.models[0].materials[0].textureName, Textures[0]);
            Load(scene.models[0].materials[1].textureName, Textures[1]);
        }

        const Model& model = scene.models[0];
        for (uint32_t i = 0; i < model.indices.size() / 3; i++)
        {
            CurrentTexture = model.vertices[model.indices[i * 3 + 0]].materialId;
            DrawTriangle(
                model.vertices[model.indices[i * 3 + 0]],
                model.vertices[model.indices[i * 3 + 1]],
                model.vertices[model.indices[i * 3 + 2]],
                ViewTransform(scene.camera)
            );
        }

        for (size_t i = 0; i < BackBuffer.size(); i++)
        {
            texture.SetColor(OutputWidth * (OutputHeight - 1 - (i / OutputWidth)) + i % OutputWidth, Color(BackBuffer[i]));
        }

        return true;
    }
}