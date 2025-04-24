#include <iostream>

#include <d3d12.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <d3d12shader.h>

#include <filesystem>
#include <string>
#include <cassert>
#include <iostream>
#include <fstream>

#include <utils.h>

#pragma comment(lib, "D3d12.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct SigDefinition
{
    struct VertexAttribute
    {
        std::string semanticName;
        DXGI_FORMAT format;
        uint32_t sizeInBytes;

        std::string name;
        std::string type;
    };

    std::vector<VertexAttribute> vertexAttributes;
    std::vector<std::tuple<std::string, std::string>> constants;
};

ID3D12ShaderReflection* CreateShaderReflection(const std::wstring& shaderPath, const std::string& shaderType, const std::string& shaderModel)
{
    UINT flags = D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
    ID3DBlob* blob = nullptr;
    D3D_NOT_FAILED(D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, shaderType.c_str(), shaderModel.c_str(), flags, 0, &blob, nullptr));

    ID3D12ShaderReflection* reflector = nullptr;
    D3D_NOT_FAILED(D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&reflector)));

    return reflector;
}

bool Load(const std::string& path, SigDefinition& definition)
{
    std::wstring shaderPath(path.begin(), path.end());

    if (ID3D12ShaderReflection* vertexShaderReflector = CreateShaderReflection(shaderPath, "VS", "vs_5_1"))
    {
        D3D12_SHADER_DESC shaderDesc;
        vertexShaderReflector->GetDesc(&shaderDesc);

        for (UINT i = 0; i < shaderDesc.InputParameters; i++)
        {
            D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
            vertexShaderReflector->GetInputParameterDesc(i, &paramDesc);

            int componentCount = 0;
            if (paramDesc.Mask & 0x1) componentCount++;
            if (paramDesc.Mask & 0x2) componentCount++;
            if (paramDesc.Mask & 0x4) componentCount++;
            if (paramDesc.Mask & 0x8) componentCount++;

            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            std::string type;
            uint32_t sizeInBytes = 0;
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
            {
                switch (componentCount)
                {
                    case 1: format = DXGI_FORMAT_R32_FLOAT; type = "float"; sizeInBytes = 4; break;
                    case 2: format = DXGI_FORMAT_R32G32_FLOAT; type = "DirectX::XMFLOAT2"; sizeInBytes = 8; break;
                    case 3: format = DXGI_FORMAT_R32G32B32_FLOAT; type = "DirectX::XMFLOAT3"; sizeInBytes = 12; break;
                    case 4: format = DXGI_FORMAT_R32G32B32A32_FLOAT; type = "DirectX::XMFLOAT4"; sizeInBytes = 16; break;
                }
            }
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
            {
                switch (componentCount)
                {
                    case 1: format = DXGI_FORMAT_R32_UINT; type = "UINT"; sizeInBytes = 4; break;
                    case 2: format = DXGI_FORMAT_R32G32_UINT; type = "DirectX::XMUINT2"; sizeInBytes = 8; break;
                    case 3: format = DXGI_FORMAT_R32G32B32_UINT; type = "DirectX::XMUINT3"; sizeInBytes = 12; break;
                    case 4: format = DXGI_FORMAT_R32G32B32A32_UINT; type = "DirectX::XMUINT4"; sizeInBytes = 16; break;
                }
            }
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
            {
                switch (componentCount)
                {
                    case 1: format = DXGI_FORMAT_R32_SINT; type = "INT"; sizeInBytes = 4; break;
                    case 2: format = DXGI_FORMAT_R32G32_SINT; type = "DirectX::XMINT2"; sizeInBytes = 8; break;
                    case 3: format = DXGI_FORMAT_R32G32B32_SINT; type = "DirectX::XMINT3"; sizeInBytes = 12; break;
                    case 4: format = DXGI_FORMAT_R32G32B32A32_SINT; type = "DirectX::XMINT4"; sizeInBytes = 16; break;
                }
            }

            std::string semanticName = paramDesc.SemanticName;
            std::string name = semanticName;
            for (char& c : name) c = std::tolower(static_cast<unsigned char>(c));

            name += "Var";

            definition.vertexAttributes.push_back({ semanticName, format, sizeInBytes, name, type });
        }

        vertexShaderReflector->Release();
    }

    if (ID3D12ShaderReflection* pixelShaderReflector = CreateShaderReflection(shaderPath, "PS", "ps_5_1"))
    {
        D3D12_SHADER_DESC shaderDesc;
        pixelShaderReflector->GetDesc(&shaderDesc);

        for (UINT i = 0; i < shaderDesc.ConstantBuffers; i++)
        {
            ID3D12ShaderReflectionConstantBuffer* constantBuffer = pixelShaderReflector->GetConstantBufferByIndex(i);
            D3D12_SHADER_BUFFER_DESC bufferDesc;
            constantBuffer->GetDesc(&bufferDesc);

            for (UINT j = 0; j < bufferDesc.Variables; j++)
            {
                ID3D12ShaderReflectionVariable* variable = constantBuffer->GetVariableByIndex(j);
                D3D12_SHADER_VARIABLE_DESC variableDescription;
                variable->GetDesc(&variableDescription);

                ID3D12ShaderReflectionType* type = variable->GetType();
                D3D12_SHADER_TYPE_DESC typeDescription;
                type->GetDesc(&typeDescription);

                std::string typeName = "unknown";
                switch (typeDescription.Type)
                {
                case D3D_SVT_BOOL:
                    switch (typeDescription.Class)
                    {
                    case D3D_SVC_SCALAR:
                        typeName = "bool";
                        break;
                    }
                    break;
                case D3D_SVT_INT:
                    switch (typeDescription.Class)
                    {
                    case D3D_SVC_SCALAR:
                        typeName = "INT";
                        break;
                    case D3D_SVC_VECTOR:
                        typeName = "DirectX::XMINT" + std::to_string(typeDescription.Columns);
                        break;
                    case D3D_SVC_MATRIX_ROWS:
                        typeName = "DirectX::XMINT" + std::to_string(typeDescription.Rows) + "X" + std::to_string(typeDescription.Columns);
                        break;
                    case D3D_SVC_MATRIX_COLUMNS:
                        typeName = "DirectX::XMINT" + std::to_string(typeDescription.Columns) + "X" + std::to_string(typeDescription.Rows);
                        break;
                    }
                    break;
                case D3D_SVT_UINT:
                    switch (typeDescription.Class)
                    {
                    case D3D_SVC_SCALAR:
                        typeName = "UINT";
                        break;
                    case D3D_SVC_VECTOR:
                        typeName = "DirectX::XMUINT" + std::to_string(typeDescription.Columns);
                        break;
                    case D3D_SVC_MATRIX_ROWS:
                        typeName = "DirectX::XMUINT" + std::to_string(typeDescription.Rows) + "X" + std::to_string(typeDescription.Columns);
                        break;
                    case D3D_SVC_MATRIX_COLUMNS:
                        typeName = "DirectX::XMUINT" + std::to_string(typeDescription.Columns) + "X" + std::to_string(typeDescription.Rows);
                        break;
                    }
                    break;
                case D3D_SVT_FLOAT:
                    switch (typeDescription.Class)
                    {
                    case D3D_SVC_SCALAR:
                        typeName = "float";
                        break;
                    case D3D_SVC_VECTOR:
                        typeName = "DirectX::XMFLOAT" + std::to_string(typeDescription.Columns);
                        break;
                    case D3D_SVC_MATRIX_ROWS:
                        typeName = "DirectX::XMFLOAT" + std::to_string(typeDescription.Rows) + "X" + std::to_string(typeDescription.Columns);
                        break;
                    case D3D_SVC_MATRIX_COLUMNS:
                        typeName = "DirectX::XMFLOAT" + std::to_string(typeDescription.Columns) + "X" + std::to_string(typeDescription.Rows);
                        break;
                    }
                    break;
                }

                definition.constants.push_back({ variableDescription.Name, typeName });
            }
        }
    }

    return true;
}

int main()
{
    SigDefinition definition;
    if (Load((std::filesystem::current_path() / ".." / "assets" / "default.hlsl").string(), definition))
    {
        std::filesystem::path generatedDir = std::filesystem::current_path() / "generated";
        std::filesystem::create_directory(generatedDir);
        std::ofstream file((generatedDir / "signature.h").string());

        if (file.is_open())
        {
            file << "#pragma once\n";
            file << "struct Constants\n";
            file << "{\n";
            for (auto& tuple : definition.constants) file << "    " << std::get<1>(tuple) << " " << std::get<0>(tuple) << ";\n";
            file << "};\n";

            file << "struct XVertex\n";
            file << "{\n";
            for (auto& vertexAttribute : definition.vertexAttributes) file << "    " << vertexAttribute.type << " " << vertexAttribute.name << ";\n";
            file << "};\n";

            file << "struct XVertexMetadata\n";
            file << "{\n";
            file << "    " << "struct VertexAttribute\n";
            file << "    " << "{\n";
            file << "    " << "    " << "const char* semanticName;\n";
            file << "    " << "    " << "const DXGI_FORMAT format;\n";
            file << "    " << "    " << "const uint32_t sizeInBytes;\n";
            file << "    " << "};\n";

            file << "    " << "static constexpr size_t vertexAttributesCount = " << definition.vertexAttributes.size() << ";\n";

            file << "    " << "static constexpr std::array<VertexAttribute, vertexAttributesCount> vertexAttributes {{";
            for (size_t i = 0; i < definition.vertexAttributes.size(); i++)
            {
                auto& vertexAttribute = definition.vertexAttributes[i];
                file << "{" << "\"" << vertexAttribute.semanticName << "\", static_cast<DXGI_FORMAT>(" << vertexAttribute.format << "), " << vertexAttribute.sizeInBytes << "}";

                if ((i + 1) != definition.vertexAttributes.size()) file << ","; else file << "}};\n";
            }

            file << "};\n";

            std::cout << "Signature has been generated." << std::endl;
            return 0;
        }
    }

    std::cout << "Signature generation failed." << std::endl;
    return 1;
}