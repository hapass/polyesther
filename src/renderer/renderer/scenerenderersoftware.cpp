#include <renderer/scenerenderersoftware.h>

#define _USE_MATH_DEFINES

#include <stdint.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cassert>
#include <iostream>
#include <array>
#include <execution>

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

    struct LightS
    {
        Light light;
        Vec position_view;
    };

    namespace
    {
        size_t OutputWidth;
        size_t OutputHeight;

        std::vector<uint32_t> BackBuffer;
        std::vector<float> ZBuffer;
        std::vector<Texture> Textures;
        LightS light;

        static constexpr uint32_t InterpolantsSize = 13;

        std::vector<std::array<float, InterpolantsSize>> IBuffer;
        std::vector<uint32_t> TBuffer;
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

    struct Edge
    {
        Edge(Vec b, Vec e, bool isMin = true) : begin(b), isBeginMin(isMin)
        {
            pixelYBegin = static_cast<int32_t>(ceil(b.y));
            pixelYEnd = static_cast<int32_t>(ceil(e.y));

            float distanceX = e.x - b.x;
            float distanceY = e.y - b.y;
            stepX = distanceY > 0.0f ? distanceX / distanceY : 0.0f;
        }

        void CalculateCandX(std::array<Interpolant, InterpolantsSize>& interpolants, int32_t y, std::array<float, InterpolantsSize>& outC, int32_t& outPixelX) const
        {
            outPixelX = static_cast<int32_t>(ceil(begin.x + (y - begin.y) * stepX));
            for (size_t i = 0; i < InterpolantsSize; i++)
            {
                outC[i] = interpolants[i].CalculateC(outPixelX - begin.x, y - begin.y, isBeginMin);
            }
        }

        int32_t pixelYBegin;
        int32_t pixelYEnd;

        float stepX;
        bool isBeginMin;
        Vec begin;
    };

    struct Triangle
    {
        struct InterpolantData
        {
            std::array<float, InterpolantsSize> left;
            std::array<float, InterpolantsSize> right;
        };

        struct LerpData
        {
            int32_t x;
            int32_t y;
            float percent;
            int32_t idIndex;
        };

        uint32_t texture = 0;

        std::array<Interpolant, InterpolantsSize> interpolants; // todo.pavelza: can we actually throw these out immediately and only leave lerpDatas?
        std::vector<VertexS> vertices;
        std::vector<LerpData> lerpDatas;
        std::vector<InterpolantData> interpolantDatas;
    };

    float Lerp(float begin, float end, float lerpAmount)
    {
        return begin + (end - begin) * lerpAmount;
    }

    void FillZBuffer(const Triangle& tr)
    {
        for (const Triangle::LerpData& ld : tr.lerpDatas)
        {
            float z = Lerp(tr.interpolantDatas[ld.idIndex].left[6], tr.interpolantDatas[ld.idIndex].right[6], ld.percent);
            if (z < ZBuffer[ld.y * OutputWidth + ld.x])
            {
                ZBuffer[ld.y * OutputWidth + ld.x] = z;
            }
        }
    }

    void FillIBuffer(const Triangle& tr)
    {
        std::for_each(std::execution::par, tr.lerpDatas.begin(), tr.lerpDatas.end(), [&tr](const Triangle::LerpData& ld) {
            float z = Lerp(tr.interpolantDatas[ld.idIndex].left[6], tr.interpolantDatas[ld.idIndex].right[6], ld.percent);
            if (z == ZBuffer[ld.y * OutputWidth + ld.x])
            {
                for (uint32_t i = 0; i < InterpolantsSize; i++)
                {
                    assert(IBuffer[y * OutputWidth + x][i] == 0.0f);
                    IBuffer[ld.y * OutputWidth + ld.x][i] = Lerp(tr.interpolantDatas[ld.idIndex].left[i], tr.interpolantDatas[ld.idIndex].right[i], ld.percent);
                }

                assert(TBuffer[y * OutputWidth + x][i] == 0);
                TBuffer[ld.y * OutputWidth + ld.x] = tr.texture;
            }
        });
    }

    void ShadeTriangle(const Triangle& tr)
    {
        std::for_each(std::execution::par, tr.lerpDatas.begin(), tr.lerpDatas.end(), [&tr](const Triangle::LerpData& ld) {
            std::array<float, InterpolantsSize>& interpolants_raw = IBuffer[ld.y * OutputWidth + ld.x];

            if (interpolants_raw[6] == ZBuffer[ld.y * OutputWidth + ld.x])
            {
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

                Vec diffuse = light.light.color.GetVec() * static_cast<float>(std::max<float>(dot(normal_vec, light_vec), 0.0f));
                Vec ambient = light.light.color.GetVec() * light.light.ambientStrength;

                float specAmount = static_cast<float>(std::max<float>(dot(normalize(pos_view), reflect(normal_vec, light_vec * -1.0f)), 0.0f));
                Vec specular = light.light.color.GetVec() * pow(specAmount, light.light.specularShininess) * light.light.specularStrength;

                Vec final_color{ tintRed, tintGreen, tintBlue, 1.0f };
                if (Textures.size() > 0)
                {
                    uint32_t materialId = TBuffer[ld.y * OutputWidth + ld.x];

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

                assert(OutputWidth > 0);
                assert(OutputHeight > 0);
                assert(0 <= x && x < OutputWidth);
                assert(0 <= y && y < OutputHeight);
                assert(BackBuffer);
                BackBuffer[ld.y * OutputWidth + ld.x] = Color(final_color).rgba;
            }
        });
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
        result.v.materialId = begin.v.materialId;
        return result;
    }

    Interpolant GetInterpolant(std::vector<VertexS>& vertices, const std::function<float(const VertexS& v)>& c)
    {
        return Interpolant(
            { vertices[0].v.position.x, vertices[0].v.position.y, c(vertices[0]) / vertices[0].v.position.w },
            { vertices[1].v.position.x, vertices[1].v.position.y, c(vertices[1]) / vertices[1].v.position.w },
            { vertices[2].v.position.x, vertices[2].v.position.y, c(vertices[2]) / vertices[2].v.position.w }
        );
    }

    void AddRawTriangle(Triangle& tr, std::vector<Triangle>& outTriangles)
    {
        assert(vertices.size() == 3);

        for (VertexS& v : tr.vertices)
        {
            v.v.position.x /= v.v.position.w;
            v.v.position.y /= v.v.position.w;
            v.v.position.z /= v.v.position.w;
        }

        std::sort(std::begin(tr.vertices), std::end(tr.vertices), [](const VertexS& lhs, const VertexS& rhs) { return lhs.v.position.y < rhs.v.position.y; });

        for (VertexS& v : tr.vertices)
        {
            // todo.pavelza: Clipping might result in some vertices being slightly outside of -1 to 1 range, so we clamp. Will need to think how to avoid this.
            v.v.position.x = std::clamp(v.v.position.x, -1.0f, 1.0f);
            v.v.position.y = std::clamp(v.v.position.y, -1.0f, 1.0f);
            v.v.position.z = std::clamp(v.v.position.z, -1.0f, 1.0f);

            v.v.position.x = (OutputWidth - 1) * ((v.v.position.x + 1) / 2.0f);
            v.v.position.y = (OutputHeight - 1) * ((v.v.position.y + 1) / 2.0f);
        }

        tr.interpolants[0] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.red; });
        tr.interpolants[1] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.green; });
        tr.interpolants[2] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.blue; });

        tr.interpolants[3] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.v.textureCoord.x; });
        tr.interpolants[4] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.v.textureCoord.y; });

        tr.interpolants[5] = GetInterpolant(tr.vertices, [](const VertexS& v) { return 1.0f; });

        // No division by w, because it is already divided by w.
        tr.interpolants[6] = Interpolant(
            { tr.vertices[0].v.position.x, tr.vertices[0].v.position.y, tr.vertices[0].v.position.z },
            { tr.vertices[1].v.position.x, tr.vertices[1].v.position.y, tr.vertices[1].v.position.z },
            { tr.vertices[2].v.position.x, tr.vertices[2].v.position.y, tr.vertices[2].v.position.z });

        tr.interpolants[7] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.v.normal.x; });
        tr.interpolants[8] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.v.normal.y; });
        tr.interpolants[9] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.v.normal.z; });

        tr.interpolants[10] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.pos_view.x; });
        tr.interpolants[11] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.pos_view.y; });
        tr.interpolants[12] = GetInterpolant(tr.vertices, [](const VertexS& v) { return v.pos_view.z; });
        tr.texture = tr.vertices[0].v.materialId;

        Edge minMax(tr.vertices[0].v.position, tr.vertices[2].v.position, true);
        Edge minMiddle(tr.vertices[0].v.position, tr.vertices[1].v.position, true);
        Edge middleMax(tr.vertices[1].v.position, tr.vertices[2].v.position, false);

        tr.interpolantDatas.resize(minMax.pixelYEnd - minMax.pixelYBegin);
        tr.lerpDatas.reserve((minMax.pixelYEnd - minMax.pixelYBegin) * 10);

        for (int32_t y = minMax.pixelYBegin; y < minMax.pixelYEnd; y++)
        {
            Edge& left = minMax;
            Edge& right = y >= middleMax.pixelYBegin ? middleMax : minMiddle;

            int32_t leftPixelX = 0;
            left.CalculateCandX(tr.interpolants, y, tr.interpolantDatas[y - minMax.pixelYBegin].left, leftPixelX);

            int32_t rightPixelX = 0;
            right.CalculateCandX(tr.interpolants, y, tr.interpolantDatas[y - minMax.pixelYBegin].right, rightPixelX);

            if (leftPixelX > rightPixelX)
            {
                std::swap(leftPixelX, rightPixelX);
                std::swap(tr.interpolantDatas[y - minMax.pixelYBegin].left, tr.interpolantDatas[y - minMax.pixelYBegin].right);
            }

            for (int32_t x = leftPixelX; x < rightPixelX; x++)
            {
                tr.lerpDatas.push_back({
                    x,
                    y,
                    static_cast<float>(x - leftPixelX) / static_cast<float>(rightPixelX - leftPixelX),
                    y - minMax.pixelYBegin
                });
            }
        }

        outTriangles.push_back(std::move(tr));
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

    void AddTriangle(uint32_t index, const Model& model, const Matrix& transform, std::vector<Triangle>& outTriangles)
    {
        Triangle tr;
        tr.vertices.resize(3);

        const Vertex& a = model.vertices[model.indices[index * 3 + 0]];
        const Vertex& b = model.vertices[model.indices[index * 3 + 1]];
        const Vertex& c = model.vertices[model.indices[index * 3 + 2]];

        tr.vertices[0] = VertexS{
            a,
            a.position,
            a.color.GetVec().x,
            a.color.GetVec().y,
            a.color.GetVec().z
        };

        tr.vertices[1] = VertexS{
            b,
            b.position,
            b.color.GetVec().x,
            b.color.GetVec().y,
            b.color.GetVec().z
        };

        tr.vertices[2] = VertexS{
            c,
            c.position,
            c.color.GetVec().x,
            c.color.GetVec().y,
            c.color.GetVec().z
        };

        for (VertexS& v : tr.vertices)
        {
            v.v.position = PerspectiveTransform(static_cast<float>(OutputWidth), static_cast<float>(OutputHeight)) * transform * v.v.position;
            v.pos_view = transform * v.pos_view;
            // This is possible because we do not do non-uniform scale in transform. If we are about to do non-uniform scale, we should calculate the normal matrix.
            v.v.normal = transform * v.v.normal;
        }

        // Backface culling produces very rough results in view space (maybe need to figure out why some day). So we do it in clip space. And it seems to be the right (identical to hardware) way.
        // Doing clipspace culling before we split triangles that penetrate camera frustum gives us additional ~10ms gain for a frame in reference scene.
        // Front is counter clockwise.
        Vec v0 { tr.vertices[0].v.position.x / tr.vertices[0].v.position.w, tr.vertices[0].v.position.y / tr.vertices[0].v.position.w, tr.vertices[0].v.position.z / tr.vertices[0].v.position.w, 1.0f };
        Vec v1 { tr.vertices[1].v.position.x / tr.vertices[1].v.position.w, tr.vertices[1].v.position.y / tr.vertices[1].v.position.w, tr.vertices[1].v.position.z / tr.vertices[1].v.position.w, 1.0f };
        Vec v2 { tr.vertices[2].v.position.x / tr.vertices[2].v.position.w, tr.vertices[2].v.position.y / tr.vertices[2].v.position.w, tr.vertices[2].v.position.z / tr.vertices[2].v.position.w, 1.0f };
        if (cross(v2 - v0, v1 - v0).z > 0)
        {
            return;
        }

        // We must check that all triangle lies on outside of one of the planes,
        // since if we check that some vertices lie on the outside of one plane and others on outside of the other,
        // then part of the triangle might still be visible.
        std::array<bool, 6> planesToOutsideness { false };
        for (const VertexS& v : tr.vertices)
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

        if (std::all_of(tr.vertices.begin(), tr.vertices.end(), [](const VertexS& v){ return IsVertexInside(v, 0, 1) && IsVertexInside(v, 1, 1) && IsVertexInside(v, 2, 1) && IsVertexInside(v, 0, -1) && IsVertexInside(v, 1, -1) && IsVertexInside(v, 2, -1); }))
        {
            AddRawTriangle(tr, outTriangles);
            return;
        }

        std::vector<VertexS> vertices = std::move(tr.vertices);
        if (ClipTriangleAxis(vertices, 0) && ClipTriangleAxis(vertices, 1) && ClipTriangleAxis(vertices, 2))
        {
            assert(vertices.size() >= 3);

            for (size_t i = 2; i < vertices.size(); i++)
            {
                Triangle newTr;
                newTr.vertices.resize(3);

                newTr.vertices[0] = vertices[0];
                newTr.vertices[1] = vertices[i - 1];
                newTr.vertices[2] = vertices[i];

                AddRawTriangle(newTr, outTriangles);
            }
        }
    }

    bool SceneRendererSoftware::Render(const Scene& scene, Texture& texture)
    {
        OutputWidth = texture.GetWidth();
        OutputHeight = texture.GetHeight();

        BackBuffer.resize(OutputWidth * OutputHeight);
        std::fill(BackBuffer.begin(), BackBuffer.end(), Color::Black.rgba);

        ZBuffer.resize(OutputWidth * OutputHeight);
        std::fill(ZBuffer.begin(), ZBuffer.end(), 2.0f);

        IBuffer.clear();
        IBuffer.resize(OutputWidth * OutputHeight);

        TBuffer.clear();
        TBuffer.resize(OutputWidth * OutputHeight);

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

        static std::vector<Triangle> triangles;
        triangles.reserve(model.indices.size() / 3 * 2);
        triangles.clear();

        for (uint32_t i = 0; i < model.indices.size() / 3; i++)
        {
            AddTriangle(i, model, ViewTransform(scene.camera), triangles);
        }

        for (const Triangle& tr : triangles)
        {
            FillZBuffer(tr);
        }

        std::for_each(std::execution::par, triangles.begin(), triangles.end(), [](const Triangle& tr) { FillIBuffer(tr); });
        std::for_each(std::execution::par, triangles.begin(), triangles.end(), [](const Triangle& tr) { ShadeTriangle(tr); });

        for (size_t i = 0; i < BackBuffer.size(); i++)
        {
            texture.SetColor(OutputWidth * (OutputHeight - 1 - (i / OutputWidth)) + i % OutputWidth, Color(BackBuffer[i]));
        }

        return true;
    }
}