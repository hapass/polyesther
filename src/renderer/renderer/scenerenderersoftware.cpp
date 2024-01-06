#include <renderer/scenerenderersoftware.h>

#define _USE_MATH_DEFINES

#include <stdint.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cassert>
#include <iostream>
#include <array>

#include "utils.h"

namespace Renderer
{
    struct InterpolationPoint
    {
        float x;
        float y;
        float c;
    };

    struct Interpolant
    {
        Interpolant() {}

        Interpolant(const InterpolationPoint& a, const InterpolationPoint& b, const InterpolationPoint& c)
        {
            float dx = (b.x - c.x) * (a.y - c.y) - (a.x - c.x) * (b.y - c.y);
            float dy = -dx;

            stepX = ((b.c - c.c) * (a.y - c.y) - (a.c - c.c) * (b.y - c.y)) / dx;
            stepY = ((b.c - c.c) * (a.x - c.x) - (a.c - c.c) * (b.x - c.x)) / dy;

            minC = a.c;
            midC = b.c;
        }

        float stepX = 0.0f;
        float stepY = 0.0f;

        float minC = 0.0f;
        float midC = 0.0f;

        float CalculateC(float dx, float dy, bool isMin)
        {
            return (isMin ? minC : midC) + dy * stepY + dx * stepX;
        }
    };

    namespace
    {
        size_t OutputWidth;
        size_t OutputHeight;

        std::vector<uint32_t> BackBuffer;
        std::vector<float> ZBuffer;
        std::vector<Texture> Textures;
        uint32_t CurrentTexture = 0;

        static constexpr uint32_t InterpolantsSize = 13;
        std::array<Interpolant, InterpolantsSize> Interpolants;
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

        // todo.pavelza: need to get rid of these, just needed for easier interpolation
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
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

    struct Edge
    {
        Edge(Vec b, Vec e, bool isMin = true) : begin(b), isBeginMin(isMin), pixelX(0)
        {
            pixelYBegin = static_cast<int32_t>(ceil(b.y));
            pixelYEnd = static_cast<int32_t>(ceil(e.y));

            float distanceX = e.x - b.x;
            float distanceY = e.y - b.y;
            stepX = distanceY > 0.0f ? distanceX / distanceY : 0.0f;
        }

        void CalculateC(int32_t y)
        {
            pixelX = static_cast<int32_t>(ceil(begin.x + (y - begin.y) * stepX));
            for (size_t i = 0; i < InterpolantsSize; i++)
            {
                currentC[i] = Interpolants[i].CalculateC(pixelX - begin.x, y - begin.y, isBeginMin);
            }
        }

        int32_t pixelYBegin;
        int32_t pixelYEnd;
        int32_t pixelX;

        float stepX;
        bool isBeginMin;
        Vec begin;

        // todo.pavelza: C along the edge. Need to rename the C into interpolated value or something like this.
        std::array<float, InterpolantsSize> currentC;
    };

    float Lerp(float begin, float end, float lerpAmount)
    {
        return begin + (end - begin) * lerpAmount;
    }

    void DrawTrianglePart(Edge& minMax, Edge& other)
    {
        // todo.pavelza: Same as currentC in the edge, but interpolated between edges, need to rename those two.
        std::array<float, InterpolantsSize> interpolants_raw;
        assert(interpolants_raw.size() == minMax.interpolants.size());

        for (int32_t y = other.pixelYBegin; y < other.pixelYEnd; y++)
        {
            Edge* left = &minMax;
            Edge* right = &other;

            left->CalculateC(y);
            right->CalculateC(y);

            if (left->pixelX > right->pixelX)
            {
                Edge* temp = left;
                left = right;
                right = temp;
            }

            for (int32_t x = left->pixelX; x < right->pixelX; x++)
            {
                float percent = static_cast<float>(x - left->pixelX) / static_cast<float>(right->pixelX - left->pixelX);

                interpolants_raw[6] = Lerp(left->currentC[6], right->currentC[6], percent);
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
                    interpolants_raw[i] = Lerp(left->currentC[i], right->currentC[i], percent);
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

                Vec pos_view { viewX, viewY, viewZ, 1.0f };
                Vec normal_vec = normalize({ normalX, normalY, normalZ, 0.0f });
                Vec light_vec = normalize(light.position_view - pos_view);

                Vec diffuse = light.light.color.GetVec() * static_cast<float>(std::max<float>(dot(normal_vec, light_vec), 0.0f));
                Vec ambient = light.light.color.GetVec() * light.light.ambientStrength;

                float specAmount = static_cast<float>(std::max<float>(dot(normalize(pos_view), reflect(normal_vec, light_vec * -1.0f)), 0.0f));
                Vec specular = light.light.color.GetVec() * pow(specAmount, light.light.specularShininess) * light.light.specularStrength;

                Vec final_color { tintRed, tintGreen, tintBlue, 1.0f };
                if (Textures.size() > 0)
                {
                    uint32_t materialId = CurrentTexture;

                    // From 0 to TextureWidth - 1 (TextureWidth pixels in total)
                    size_t textureX = static_cast<size_t>((interpolants_raw[3] / interpolants_raw[5]) * (Textures[materialId].GetWidth() - 1));
                    // From 0 to TextureHeight - 1 (TextureHeight pixels in total)
                    size_t textureY = static_cast<size_t>((interpolants_raw[4] / interpolants_raw[5]) * (Textures[materialId].GetHeight() - 1));

                    // todo.pavelza: 0 height texture undefined behavior
                    textureY = (Textures[materialId].GetHeight() - 1) - textureY; // invert texture coords

                    assert(textureY < Textures[materialId].GetHeight() && textureX < Textures[materialId].GetWidth());
                    size_t texelBase = textureY * Textures[materialId].GetWidth() + textureX;

                    final_color = Textures[materialId].GetColor(texelBase).GetVec();
                }

                final_color = (diffuse + ambient + specular) * final_color;
                final_color.x = std::clamp(final_color.x, 0.0f, 1.0f);
                final_color.y = std::clamp(final_color.y, 0.0f, 1.0f);
                final_color.z = std::clamp(final_color.z, 0.0f, 1.0f);
                final_color.w = 1.0f; // fix the alpha being affected by light, noticable only in tests, because in application we correct alpha manually when copying to backbuffer

                DrawPixel(x, y, Color(final_color));
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

        std::sort(std::begin(vertices), std::end(vertices), [](const VertexS& lhs, const VertexS& rhs) { return lhs.v.position.y < rhs.v.position.y; });

        for (VertexS& v : vertices)
        {
            // todo.pavelza: Clipping might result in some vertices being slightly outside of -1 to 1 range, so we clamp. Will need to think how to avoid this.
            v.v.position.x = std::clamp(v.v.position.x, -1.0f, 1.0f);
            v.v.position.y = std::clamp(v.v.position.y, -1.0f, 1.0f);
            v.v.position.z = std::clamp(v.v.position.z, -1.0f, 1.0f);

            v.v.position.x = (OutputWidth - 1) * ((v.v.position.x + 1) / 2.0f);
            v.v.position.y = (OutputHeight - 1) * ((v.v.position.y + 1) / 2.0f);
        }

        Interpolants[0] = GetInterpolant(vertices, vertices[0].red, vertices[1].red, vertices[2].red);
        Interpolants[1] = GetInterpolant(vertices, vertices[0].green, vertices[1].green, vertices[2].green);
        Interpolants[2] = GetInterpolant(vertices, vertices[0].blue, vertices[1].blue, vertices[2].blue);

        Interpolants[3] = GetInterpolant(vertices, vertices[0].v.textureCoord.x, vertices[1].v.textureCoord.x, vertices[2].v.textureCoord.x);
        Interpolants[4] = GetInterpolant(vertices, vertices[0].v.textureCoord.y, vertices[1].v.textureCoord.y, vertices[2].v.textureCoord.y);

        Interpolants[5] = GetInterpolant(vertices, 1.0f, 1.0f, 1.0f);

        // No division by w, because it is already divided by w.
        Interpolants[6] = Interpolant({ vertices[0].v.position.x, vertices[0].v.position.y, vertices[0].v.position.z }, { vertices[1].v.position.x, vertices[1].v.position.y, vertices[1].v.position.z }, { vertices[2].v.position.x, vertices[2].v.position.y, vertices[2].v.position.z });

        Interpolants[7] = GetInterpolant(vertices, vertices[0].v.normal.x, vertices[1].v.normal.x, vertices[2].v.normal.x);
        Interpolants[8] = GetInterpolant(vertices, vertices[0].v.normal.y, vertices[1].v.normal.y, vertices[2].v.normal.y);
        Interpolants[9] = GetInterpolant(vertices, vertices[0].v.normal.z, vertices[1].v.normal.z, vertices[2].v.normal.z);

        Interpolants[10] = GetInterpolant(vertices, vertices[0].pos_view.x, vertices[1].pos_view.x, vertices[2].pos_view.x);
        Interpolants[11] = GetInterpolant(vertices, vertices[0].pos_view.y, vertices[1].pos_view.y, vertices[2].pos_view.y);
        Interpolants[12] = GetInterpolant(vertices, vertices[0].pos_view.z, vertices[1].pos_view.z, vertices[2].pos_view.z);

        Edge minMax(vertices[0].v.position, vertices[2].v.position, true);
        Edge minMiddle(vertices[0].v.position, vertices[1].v.position, true);
        Edge middleMax(vertices[1].v.position, vertices[2].v.position, false);

        DrawTrianglePart(minMax, minMiddle);
        DrawTrianglePart(minMax, middleMax);
    }

    bool IsVertexInside(const VertexS& point, int32_t axis, int32_t plane)
    {
        return point.v.position.Get(axis) * plane <= point.v.position.w;
    }

    // todo.pavelza: There is an issue somewhere - we render black 1px line on the border sometimes when the triangle is outside of frustum (compared to dx12 renderer).
    // And after introducing the backface culling we can sometimes see pixels from some triangles on the back in the first left 1px line.
    // Looks like front triangle is clipped not precisely by frustum.
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
        static std::vector<VertexS> vertices;
        vertices.clear();
        vertices.resize(3);

        vertices[0] = VertexS {
            a,
            a.position,
            a.color.GetVec().x,
            a.color.GetVec().y,
            a.color.GetVec().z
        };

        vertices[1] = VertexS {
            b,
            b.position,
            b.color.GetVec().x,
            b.color.GetVec().y,
            b.color.GetVec().z
        };

        vertices[2] = VertexS {
            c,
            c.position,
            c.color.GetVec().x,
            c.color.GetVec().y,
            c.color.GetVec().z
        };

        for (VertexS& v : vertices)
        {
            v.v.position = PerspectiveTransform(static_cast<float>(OutputWidth), static_cast<float>(OutputHeight)) * transform * v.v.position;
            v.pos_view = transform * v.pos_view;
            // This is possible because we do not do non-uniform scale in transform. If we are about to do non-uniform scale, we should calculate the normal matrix.
            v.v.normal = transform * v.v.normal;
        }

        // Backface culling produces very rough results in view space (maybe need to figure out why some day). So we do it in clip space. And it seems to be the right (identical to hardware) way.
        // Doing clipspace culling before we split triangles that penetrate camera frustum gives us additional ~10ms gain for a frame in reference scene.
        // Front is counter clockwise.
        Vec v0 { vertices[0].v.position.x / vertices[0].v.position.w, vertices[0].v.position.y / vertices[0].v.position.w, vertices[0].v.position.z / vertices[0].v.position.w, 1.0f };
        Vec v1 { vertices[1].v.position.x / vertices[1].v.position.w, vertices[1].v.position.y / vertices[1].v.position.w, vertices[1].v.position.z / vertices[1].v.position.w, 1.0f };
        Vec v2 { vertices[2].v.position.x / vertices[2].v.position.w, vertices[2].v.position.y / vertices[2].v.position.w, vertices[2].v.position.z / vertices[2].v.position.w, 1.0f };
        if (cross(v2 - v0, v1 - v0).z > 0)
        {
            return;
        }

        // We must check that all triangle lies on outside of one of the planes,
        // since if we check that some vertices lie on the outside of one plane and others on outside of the other,
        // then part of the triangle might still be visible.
        std::array<bool, 6> planesToOutsideness { false };
        for (const VertexS& v : vertices)
        {
            planesToOutsideness[0] |= IsVertexInside(v, 0, 1);
            planesToOutsideness[1] |= IsVertexInside(v, 1, 1);
            planesToOutsideness[2] |= IsVertexInside(v, 2, 1);
            planesToOutsideness[3] |= IsVertexInside(v, 0, -1);
            planesToOutsideness[4] |= IsVertexInside(v, 1, -1);
            planesToOutsideness[5] |= IsVertexInside(v, 2, -1);
        }

        // This check doesn't give us a big performance boost in a general case,
        // but it doesn't seem to hit performance in general case too much to remove it either.
        if (std::any_of(planesToOutsideness.begin(), planesToOutsideness.end(), [](bool val) { return !val; }))
        {
            return;
        }

        if (std::all_of(vertices.begin(), vertices.end(), [](const VertexS& v){ return IsVertexInside(v, 0, 1) && IsVertexInside(v, 1, 1) && IsVertexInside(v, 2, 1) && IsVertexInside(v, 0, -1) && IsVertexInside(v, 1, -1) && IsVertexInside(v, 2, -1); }))
        {
            DrawRawTriangle(vertices);
            return;
        }

        if (ClipTriangleAxis(vertices, 0) && ClipTriangleAxis(vertices, 1) && ClipTriangleAxis(vertices, 2))
        {
            assert(vertices.size() >= 3);

            for (size_t i = 2; i < vertices.size(); i++)
            {
                static std::vector<VertexS> triangle;
                triangle.clear();
                triangle.resize(3);

                triangle[0] = vertices[0];
                triangle[1] = vertices[i - 1];
                triangle[2] = vertices[i];

                DrawRawTriangle(triangle);
            }
        }
    }

    bool SceneRendererSoftware::Render(const Scene& scene, Texture& texture)
    {
        OutputWidth = texture.GetWidth();
        OutputHeight = texture.GetHeight();

        BackBuffer.resize(OutputWidth * OutputHeight);
        ZBuffer.resize(OutputWidth * OutputHeight);

        std::fill(BackBuffer.begin(), BackBuffer.end(), Color::Black.rgba);
        std::fill(ZBuffer.begin(), ZBuffer.end(), 2.0f);

        // calculate light's position in view space
        light.position_view = ViewTransform(scene.camera) * scene.light.position;
        light.light = scene.light;

        if (Textures.size() == 0)
        {
            Textures.resize(scene.models[0].materials.size());
            for (size_t i = 0; i < Textures.size(); i++)
            {
                Load(scene.models[0].materials[i].textureName, Textures[i]);
            }
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