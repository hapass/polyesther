#include <renderer/scene.h>

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
            std::map<Vertex, uint32_t> vertices;
            std::map<std::string, Material> materials;
            std::vector<Vec> textureCoords;
            std::vector<Vec> positions;
            std::vector<Vec> normals;
            std::string currentMaterial;
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

        bool Read(std::stringstream& lineStream, Context& loadContext, Model& model)
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

                vert.materialId = loadContext.materials.at(loadContext.currentMaterial).id;

                if (loadContext.vertices.count(vert) == 0)
                {
                    model.vertices.push_back(vert);
                    uint32_t index = static_cast<uint32_t>(model.vertices.size()) - 1;
                    model.indices.push_back(index);
                    loadContext.vertices[vert] = index;
                }
                else
                {
                    model.indices.push_back(loadContext.vertices[vert]);
                }
            }

            return true;
        }

        bool Load(const std::string& fileName, std::map<std::string, Material>& materials)
        {
            const char* TYPE_MATERIAL= "newmtl";
            const char* TYPE_TEXTURE_FILENAME = "map_Kd";

            std::fstream file(RootFolder + fileName);
            std::string line;

            std::string currentMaterial;
            uint32_t currentMaterialId = 0;

            while (std::getline(file, line))
            {
                std::stringstream lineStream(line);
                std::string primitiveType;
                if (lineStream >> primitiveType)
                {
                    if (primitiveType == TYPE_MATERIAL)
                    {
                        if (lineStream >> currentMaterial)
                        {
                            // success
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
                            material.id = currentMaterialId++;
                            material.textureName = materialFileName;
                            materials[currentMaterial] = std::move(material);
                        }
                        else
                        {
                            return false;
                        }
                    }
                }
            }

            return true;
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
                        if (Read(lineStream, loadContext, model))
                        {
                            // should think about refactoring
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
                            if (Load(fileName, loadContext.materials))
                            {
                                // success
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
                        if (lineStream >> loadContext.currentMaterial)
                        {
                            // success
                        }
                        else
                        {
                            return false;
                        }
                    }
                }
            }

            return true;
        }
    }

    bool operator<(const Vertex& lhs, const Vertex& rhs)
    {
        return std::tie(lhs.color.rgb_vec, lhs.normal, lhs.position, lhs.textureCoord) <
            std::tie(rhs.color.rgb_vec, rhs.normal, rhs.position, rhs.textureCoord);
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

        return true;
    }
}