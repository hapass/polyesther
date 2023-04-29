#define _USE_MATH_DEFINES

#include <renderer/scene.h>

#include <renderer/utils.h>

#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <map>
#include <cassert>
#include <iostream>

namespace Renderer
{
    namespace
    {
        const char* RootFolder = "C:\\Users\\hapas\\Documents\\Code\\software_rasterizer\\assets\\";

        struct Context
        {
            std::map<std::string, uint32_t> materials;
            std::vector<Vec> textureCoords;
            std::vector<Vec> positions;
            std::vector<Vec> normals;
            uint32_t currentMaterialId = 0;
        };

        bool Read(std::stringstream& lineStream, uint32_t elements, float def, Vec& vec)
        {
            uint32_t currentIndex = 0;
            while (currentIndex < 4)
            {
                if (currentIndex < elements)
                {
                    float val = 0.0f;
                    if (lineStream >> val)
                    {
                        vec.Set(currentIndex, val);
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    vec.Set(currentIndex, def);
                }

                currentIndex++;
            }

            return true;
        }

        bool Read(std::stringstream& lineStream, const Context& loadContext, std::vector<Vertex>& vertices)
        {
            std::string faceDescription;
            while (lineStream >> faceDescription)
            {
                Vertex vert;

                std::stringstream faceDescriptionStream(faceDescription);

                int32_t positionIndex;
                if (faceDescriptionStream >> positionIndex)
                {
                    vert.position = loadContext.positions.at(--positionIndex);
                }
                else
                {
                    return false;
                }

                faceDescriptionStream.ignore(1);

                int32_t textureCoordIndex;
                if (faceDescriptionStream >> textureCoordIndex)
                {
                    vert.textureCoord = loadContext.textureCoords.at(--textureCoordIndex);
                }
                else
                {
                    return false;
                }

                faceDescriptionStream.ignore(1);

                int32_t normalIndex;
                if (faceDescriptionStream >> normalIndex)
                {
                    vert.normal = loadContext.normals.at(--normalIndex);
                }
                else
                {
                    return false;
                }

                vert.materialId = loadContext.currentMaterialId;
                vertices.push_back(vert);
            }

            return true;
        }

        bool Load(const std::string& fileName, std::vector<Material>& materials)
        {
            const char* TYPE_MATERIAL= "newmtl";
            const char* TYPE_TEXTURE_FILENAME = "map_Kd";

            std::fstream file(RootFolder + fileName);
            std::string line;

            std::string currentMaterialName;

            while (std::getline(file, line))
            {
                std::stringstream lineStream(line);
                std::string primitiveType;
                if (lineStream >> primitiveType)
                {
                    if (primitiveType == TYPE_MATERIAL)
                    {
                        std::string materialName;
                        if (lineStream >> materialName)
                        {
                            currentMaterialName = materialName;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else if (primitiveType == TYPE_TEXTURE_FILENAME)
                    {
                        std::string materialFileName;
                        if (lineStream >> materialFileName)
                        {
                            Material material;
                            material.name = currentMaterialName;
                            material.textureName = materialFileName;
                            materials.push_back(std::move(material));
                        }
                        else
                        {
                            
                            return false;
                        }
                    }
                }
            }

            return file.is_open();
        }

        bool Load(const std::string& fileName, Model& model)
        {
            const char* TYPE_VERTEX = "v";
            const char* TYPE_NORMAL = "vn";
            const char* TYPE_TEXTURE_COORDS = "vt";
            const char* TYPE_FACE = "f";
            const char* TYPE_MATERIAL_LIB = "mtllib";
            const char* TYPE_MATERIAL = "usemtl";

            Context loadContext;

            std::map<Vertex, uint32_t> vertexIndices;

            std::fstream file(RootFolder + fileName);
            std::string line;

            while (std::getline(file, line))
            {
                std::stringstream lineStream(line);
                std::string primitiveType;
                if (lineStream >> primitiveType)
                {
                    if (primitiveType == TYPE_VERTEX)
                    {
                        Vec position;
                        if (Read(lineStream, 3, 1.0f, position))
                        {
                            loadContext.positions.push_back(position);
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else if (primitiveType == TYPE_NORMAL)
                    {
                        Vec normal;
                        if (Read(lineStream, 3, 0.0f, normal))
                        {
                            loadContext.normals.push_back(normal);
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else if (primitiveType == TYPE_TEXTURE_COORDS)
                    {
                        Vec uv;
                        if (Read(lineStream, 2, 0.0f, uv))
                        {
                            loadContext.textureCoords.push_back(uv);
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else if (primitiveType == TYPE_FACE)
                    {
                        std::vector<Vertex> vertices;
                        if (Read(lineStream, loadContext, vertices))
                        {
                            for (const Vertex& vertex : vertices)
                            {
                                if (vertexIndices.count(vertex) == 0)
                                {
                                    model.vertices.push_back(vertex);
                                    vertexIndices[vertex] = static_cast<uint32_t>(model.vertices.size()) - 1;
                                }
                                
                                model.indices.push_back(vertexIndices[vertex]);
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else if (primitiveType == TYPE_MATERIAL_LIB)
                    {
                        std::string fileName;
                        if (lineStream >> fileName)
                        {
                            if (Load(fileName, model.materials))
                            {
                                for (size_t i = 0; i < model.materials.size(); i++)
                                {
                                    loadContext.materials[model.materials[i].name] = static_cast<uint32_t>(i);
                                }
                            }
                            else
                            {
                                return false;
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else if (primitiveType == TYPE_MATERIAL)
                    {
                        std::string currentMaterial;
                        if (lineStream >> currentMaterial)
                        {
                            loadContext.currentMaterialId = loadContext.materials.at(currentMaterial);
                        }
                        else
                        {
                            return false;
                        }
                    }
                }
            }

            return file.is_open();
        }
    }

    bool operator<(const Vertex& lhs, const Vertex& rhs)
    {
        return std::tie(lhs.materialId, lhs.color.rgb_vec, lhs.normal, lhs.position, lhs.textureCoord) <
            std::tie(rhs.materialId, rhs.color.rgb_vec, rhs.normal, rhs.position, rhs.textureCoord);
    }

    bool operator==(const Vertex& lhs, const Vertex& rhs)
    {
        return !(lhs < rhs) && !(rhs < lhs);
    }

    bool Load(const std::string& fileName, Scene& scene)
    {
        std::fstream file(RootFolder + fileName);
        std::string line;

        while (std::getline(file, line))
        {
            Model model;
            std::stringstream lineStream(line);

            if (Read(lineStream, 3, 1.0f, model.position))
            {
                std::string modelFileName;
                if (lineStream >> modelFileName)
                {
                    if (Load(modelFileName, model))
                    {
                        scene.models.push_back(std::move(model));
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        return file.is_open();
    }

    Matrix PerspectiveTransform(float width, float height)
    {
        static float Far = 400.0f;
        static float Near = .1f;
        static float FOV = 25;

        float halfFieldOfView = FOV * (static_cast<float>(M_PI) / 180);
        float aspect = width / height;

        Matrix m;

        //col 1
        m.m[0] = 1.0f / (tanf(halfFieldOfView) * aspect);
        m.m[4] = 0.0f;
        m.m[8] = 0.0f;
        m.m[12] = 0.0f;

        //col 2
        m.m[1] = 0.0f;
        m.m[5] = 1.0f / tanf(halfFieldOfView);
        m.m[9] = 0.0f;
        m.m[13] = 0.0f;

        //col 3
        m.m[2] = 0.0f;
        m.m[6] = 0.0f;
        m.m[10] = Far / (Near - Far);
        m.m[14] = -1.0f;

        //col 4
        m.m[3] = 0.0f;
        m.m[7] = 0.0f;
        m.m[11] = Near * Far / (Near - Far);
        m.m[15] = 0.0f;

        return m;
    }

    Matrix ViewTransform(const Camera& camera)
    {
        Matrix rTranslate = translate(-camera.position.x, -camera.position.y, -camera.position.z);
        Matrix rYaw = transpose(rotateY(camera.yaw));
        Matrix rPitch = transpose(rotateX(camera.pitch));

        return rPitch * rYaw * rTranslate;
    }

    Matrix ModelTransform(const Model& model)
    {
        return translate(model.position.x, model.position.y, model.position.z);
    }

    Matrix CameraTransform(const Camera& camera)
    {
        Matrix rYaw = rotateY(camera.yaw);
        Matrix rPitch = rotateX(camera.pitch);

        return rYaw * rPitch;
    }
}