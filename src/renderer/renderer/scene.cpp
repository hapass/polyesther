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
        struct Context
        {
            std::map<std::string, uint32_t> materials;
            std::vector<Vec> textureCoords;
            std::vector<Vec> positions;
            std::vector<Color> colors;
            std::vector<Vec> normals;
            int32_t currentMaterialId = -1;
        };

        std::string GetFileExtension(const std::string& fileName)
        {
            std::string result(fileName);
            size_t pos = result.rfind('.');
            if (pos == std::string::npos || pos == result.size() - 1)
            {
                return std::string();
            }

            return result.substr(pos + 1);
        }

        std::string ReplaceFileNameInFullPath(const std::string& fullFileName, const std::string& newFileName)
        {
            std::string result(fullFileName);
            size_t pos = result.rfind('\\');
            if (pos == std::string::npos || pos == result.size() - 1)
            {
                return std::string();
            }

            result.replace(pos + 1, std::string::npos, newFileName);
            return result;
        }

        bool Read(std::stringstream& lineStream, uint32_t elements, float defaultVal, Vec& vec)
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
                        // do not log error, can be optional, error will be logged further
                        return false;
                    }
                }
                else
                {
                    vec.Set(currentIndex, defaultVal);
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
                    int32_t index = --positionIndex;
                    vert.position = loadContext.positions.at(index);
                    vert.color = loadContext.colors.at(index);
                }
                else
                {
                    REPORT_ERROR();
                }

                faceDescriptionStream.ignore(1);

                int32_t textureCoordIndex;
                if (faceDescriptionStream >> textureCoordIndex)
                {
                    vert.textureCoord = loadContext.textureCoords.at(--textureCoordIndex);
                }
                else
                {
                    REPORT_ERROR();
                }

                faceDescriptionStream.ignore(1);

                int32_t normalIndex;
                if (faceDescriptionStream >> normalIndex)
                {
                    vert.normal = loadContext.normals.at(--normalIndex);
                }
                else
                {
                    REPORT_ERROR();
                }

                vert.materialId = loadContext.currentMaterialId;
                vertices.push_back(vert);
            }

            return true;
        }

        bool Load(const std::string& fullFileName, std::vector<Material>& materials)
        {
            const char* TYPE_MATERIAL= "newmtl";
            const char* TYPE_TEXTURE_FILENAME = "map_Kd";

            std::fstream file(fullFileName);
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
                            REPORT_ERROR();
                        }
                    }
                    else if (primitiveType == TYPE_TEXTURE_FILENAME)
                    {
                        std::string textureFileName;
                        if (lineStream >> textureFileName)
                        {
                            Material material;
                            material.name = currentMaterialName;
                            material.textureName = ReplaceFileNameInFullPath(fullFileName, textureFileName);
                            materials.push_back(std::move(material));
                        }
                        else
                        {
                            REPORT_ERROR();
                        }
                    }
                }
            }

            REPORT_ERROR_IF_FALSE(file.is_open());
        }

        bool Load(const std::string& fullFileName, Model& model)
        {
            const char* TYPE_VERTEX = "v";
            const char* TYPE_NORMAL = "vn";
            const char* TYPE_TEXTURE_COORDS = "vt";
            const char* TYPE_FACE = "f";
            const char* TYPE_MATERIAL_LIB = "mtllib";
            const char* TYPE_MATERIAL = "usemtl";

            Context loadContext;

            std::map<Vertex, uint32_t> vertexIndices;

            std::fstream file(fullFileName);
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

                            Vec color;
                            if (Read(lineStream, 3, 1.0f, color))
                            {
                                loadContext.colors.push_back(Color(color));
                            }
                            else
                            {
                                loadContext.colors.push_back(Color());
                            }
                        }
                        else
                        {
                            REPORT_ERROR();
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
                            REPORT_ERROR();
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
                            REPORT_ERROR();
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
                            REPORT_ERROR();
                        }
                    }
                    else if (primitiveType == TYPE_MATERIAL_LIB)
                    {
                        std::string materialFileName;
                        if (lineStream >> materialFileName)
                        {
                            if (Load(ReplaceFileNameInFullPath(fullFileName, materialFileName), model.materials))
                            {
                                for (size_t i = 0; i < model.materials.size(); i++)
                                {
                                    loadContext.materials[model.materials[i].name] = static_cast<uint32_t>(i);
                                }
                            }
                            else
                            {
                                REPORT_ERROR();
                            }
                        }
                        else
                        {
                            REPORT_ERROR();
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
                            REPORT_ERROR();
                        }
                    }
                }
            }

            REPORT_ERROR_IF_FALSE(file.is_open());
        }

        bool Load(const std::string& fullFileName, Light& light)
        {
            std::fstream file(fullFileName);
            std::string line;

            if (std::getline(file, line))
            {
                std::stringstream lineStream(line);

                Vec params;
                if (Read(lineStream, 3, 0.0f, params))
                {
                    light.ambientStrength = params.x;
                    light.specularStrength = params.y;
                    light.specularShininess = params.z;
                }
                else
                {
                    REPORT_ERROR();
                }
            }
            else
            {
                REPORT_ERROR();
            }

            if (std::getline(file, line))
            {
                std::stringstream lineStream(line);

                Vec color;
                if (Read(lineStream, 3, 1.0f, color))
                {
                    light.color = Color(color);
                }
                else
                {
                    REPORT_ERROR();
                }
            }
            else
            {
                REPORT_ERROR();
            }

            REPORT_ERROR_IF_FALSE(file.is_open());
        }

        bool Load(const std::string& fullFileName, Camera& camera)
        {
            std::fstream file(fullFileName);
            std::string line;

            if (std::getline(file, line))
            {
                std::stringstream lineStream(line);

                Vec rotation;
                if (Read(lineStream, 2, 0.0f, rotation))
                {
                    camera.pitch = rotation.x;
                    camera.yaw = rotation.y;
                }
                else
                {
                    REPORT_ERROR();
                }
            }

            REPORT_ERROR_IF_FALSE(file.is_open());
        }
    }

    bool operator<(const Vertex& lhs, const Vertex& rhs)
    {
        return std::tie(lhs.materialId, lhs.color.rgba_vec, lhs.normal, lhs.position, lhs.textureCoord) <
            std::tie(rhs.materialId, rhs.color.rgba_vec, rhs.normal, rhs.position, rhs.textureCoord);
    }

    bool operator==(const Vertex& lhs, const Vertex& rhs)
    {
        return !(lhs < rhs) && !(rhs < lhs);
    }

    bool Load(const std::string& fullFileName, Scene& scene)
    {
        const char* EXTENSION_MODEL = "obj";
        const char* EXTENSION_LIGHT = "lig";
        const char* EXTENSION_CAMERA = "cam";

        std::fstream file(fullFileName);
        std::string line;

        while (std::getline(file, line))
        {
            std::stringstream lineStream(line);

            std::string fileName;
            if (lineStream >> fileName)
            {
                std::string extension = GetFileExtension(fileName);

                if (extension == EXTENSION_MODEL)
                {
                    Model model;
                    if (Read(lineStream, 3, 1.0f, model.position))
                    {
                        if (Load(ReplaceFileNameInFullPath(fullFileName, fileName), model))
                        {
                            std::string culling;
                            if (lineStream >> culling)
                            {
                                model.backfaceCulling = culling != "culling_off"; // todo.pavelza: add tests
                            }

                            scene.models.push_back(std::move(model));
                        }
                        else
                        {
                            REPORT_ERROR();
                        }
                    }
                    else
                    {
                        REPORT_ERROR();
                    }
                }
                else if (extension == EXTENSION_CAMERA)
                {
                    if (Read(lineStream, 3, 1.0f, scene.camera.position))
                    {
                        if (!Load(ReplaceFileNameInFullPath(fullFileName, fileName), scene.camera))
                        {
                            REPORT_ERROR();
                        }
                    }
                    else
                    {
                        REPORT_ERROR();
                    }
                }
                else if (extension == EXTENSION_LIGHT)
                {
                    if (Read(lineStream, 3, 1.0f, scene.light.position))
                    {
                        if (!Load(ReplaceFileNameInFullPath(fullFileName, fileName), scene.light))
                        {
                            REPORT_ERROR();
                        }
                    }
                    else
                    {
                        REPORT_ERROR();
                    }
                }
            }
            else
            {
                REPORT_ERROR();
            }


        }

        REPORT_ERROR_IF_FALSE(file.is_open());
    }

    Matrix PerspectiveTransform(float width, float height)
    {
        static float Far = 400.0f; // todo.pavelza: move to camera
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