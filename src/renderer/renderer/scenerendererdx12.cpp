#include <renderer/scenerendererdx12.h>
#include <renderer/utils.h>
#include <renderer/devicedx12.h>

#include <d3d12.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <d3d12shader.h>

// todo: verify if needed
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <map>
#include <cassert>
#include <iostream>


namespace Renderer
{
    constexpr UINT NumberOfConstantStructs = UINT(1);
    constexpr UINT NumberOfGBufferTextures = UINT(4);

    struct GPU
    {

        GPU(const std::wstring& shaderPath)
        {

        }
    };


    struct PSOBuilder
    {
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
                if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                {
                    switch (componentCount)
                    {
                        case 1: format = DXGI_FORMAT_R32_FLOAT; break;
                        case 2: format = DXGI_FORMAT_R32G32_FLOAT; break;
                        case 3: format = DXGI_FORMAT_R32G32B32_FLOAT; break;
                        case 4: format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
                    }
                }
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                {
                    switch (componentCount)
                    {
                        case 1: format = DXGI_FORMAT_R32_UINT; break;
                        case 2: format = DXGI_FORMAT_R32G32_UINT; break;
                        case 3: format = DXGI_FORMAT_R32G32B32_UINT; break;
                        case 4: format = DXGI_FORMAT_R32G32B32A32_UINT; break;
                    }
                }
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                {
                    switch (componentCount)
                    {
                        case 1: format = DXGI_FORMAT_R32_SINT; break;
                        case 2: format = DXGI_FORMAT_R32G32_SINT; break;
                        case 3: format = DXGI_FORMAT_R32G32B32_SINT; break;
                        case 4: format = DXGI_FORMAT_R32G32B32A32_SINT; break;
                    }
                }

                definition.vertexAttributes.push_back({ paramDesc.SemanticName, format });
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
                            typeName = "int32_t";
                            break;
                        case D3D_SVC_VECTOR:
                            typeName = "DirectX::XMINT" + std::to_string(typeDescription.Columns);
                            break;
                        case D3D_SVC_MATRIX_ROWS:
                            typeName = "DirectX::XMINT" + std::to_string(typeDescription.Rows) + "x" + std::to_string(typeDescription.Columns);
                            break;
                        case D3D_SVC_MATRIX_COLUMNS:
                            typeName = "DirectX::XMINT" + std::to_string(typeDescription.Columns) + "x" + std::to_string(typeDescription.Rows);
                            break;
                        }
                        break;
                    case D3D_SVT_UINT:
                        switch (typeDescription.Class)
                        {
                        case D3D_SVC_SCALAR:
                            typeName = "uint32_t";
                            break;
                        case D3D_SVC_VECTOR:
                            typeName = "DirectX::XMUINT" + std::to_string(typeDescription.Columns);
                            break;
                        case D3D_SVC_MATRIX_ROWS:
                            typeName = "DirectX::XMUINT" + std::to_string(typeDescription.Rows) + "x" + std::to_string(typeDescription.Columns);
                            break;
                        case D3D_SVC_MATRIX_COLUMNS:
                            typeName = "DirectX::XMUINT" + std::to_string(typeDescription.Columns) + "x" + std::to_string(typeDescription.Rows);
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
                            typeName = "DirectX::XMFLOAT" + std::to_string(typeDescription.Rows) + "x" + std::to_string(typeDescription.Columns);
                            break;
                        case D3D_SVC_MATRIX_COLUMNS:
                            typeName = "DirectX::XMFLOAT" + std::to_string(typeDescription.Columns) + "x" + std::to_string(typeDescription.Rows);
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

    struct RendererDX12Context
    {
        RendererDX12Context(const Scene& scene, const Texture& texture, const std::wstring& shaderPath, const DeviceDX12& device)
            : scene(scene)
            , texture(texture)
            , deviceDX12(device)
        {
            // collect all materials
            std::vector<std::string> allMaterials;
            for (const Model& model : scene.models)
            {
                for (const Material& mat : model.materials)
                {
                    allMaterials.push_back(mat.textureName);
                }
            }

            // rendering
            const UINT numberOfTextures = UINT(allMaterials.size() + 1);

            rootDescriptorHeap = CreateRootDescriptorHeap(NumberOfConstantStructs + numberOfTextures + NumberOfGBufferTextures);

            // Constant buffer
            D3D12_RESOURCE_DESC uploadBufferDescription;
            uploadBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            uploadBufferDescription.Alignment = 0;
            uploadBufferDescription.Width = AlignBytes<Constants>();
            uploadBufferDescription.Height = 1;
            uploadBufferDescription.DepthOrArraySize = 1;
            uploadBufferDescription.MipLevels = 1;
            uploadBufferDescription.Format = DXGI_FORMAT_UNKNOWN;

            uploadBufferDescription.SampleDesc.Count = 1;
            uploadBufferDescription.SampleDesc.Quality = 0;

            uploadBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            uploadBufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES uploadHeapProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDescription, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&constantBuffer)));

            constantBuffer->SetName(L"Constant buffer.");

            D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDescription;
            constantBufferViewDescription.BufferLocation = constantBuffer->GetGPUVirtualAddress();
            constantBufferViewDescription.SizeInBytes = static_cast<UINT>(uploadBufferDescription.Width);

            deviceDX12.GetDevice()->CreateConstantBufferView(&constantBufferViewDescription, rootDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

            // root signature
            rootSignature = CreateRootSignature(numberOfTextures);

            // pso
            gbufferPSO = CreatePSO(shaderPath + L"gbuffer.hlsl", D3D12_CULL_MODE_BACK, 3); // todo.pavelza: should be destroyed somehow?
            noCullingGbufferPso = CreatePSO(shaderPath + L"gbuffer.hlsl", D3D12_CULL_MODE_NONE, 3); // todo.pavelza: should be destroyed somehow?
            finalImagePSO = CreatePSO(shaderPath + L"default.hlsl", D3D12_CULL_MODE_BACK, 1);

            // queue
            deviceDX12.GetQueue().SetCurrentPipelineStateObject(gbufferPSO);

            // move constant buffer into correct state
            deviceDX12.GetQueue().AddBarrierToList(constantBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);

            gBuffer = new RenderTarget(deviceDX12, rootDescriptorHeap, texture.GetWidth(), texture.GetHeight(), RenderTarget::Type::GBuffer);
            finalImage = new RenderTarget(deviceDX12, rootDescriptorHeap, texture.GetWidth(), texture.GetHeight(), RenderTarget::Type::FinalImage);

            // upload static geometry
            std::vector<XVertex> vertexData;
            size_t totalMaterials = 0;
            for (const Model& model : scene.models)
            {
                for (const Vertex& vert : model.vertices)
                {
                    vertexData.push_back(
                        XVertex({
                            DirectX::XMFLOAT3(vert.position.x, vert.position.y, vert.position.z),
                            DirectX::XMFLOAT2(vert.textureCoord.x, vert.textureCoord.y),
                            DirectX::XMFLOAT3(vert.normal.x, vert.normal.y, vert.normal.z),
                            DirectX::XMFLOAT3(vert.color.GetVec().x, vert.color.GetVec().y, vert.color.GetVec().z),
                            vert.materialId == -1 ? -1 : INT(totalMaterials + vert.materialId + NumberOfGBufferTextures)
                            })
                    );
                }
                totalMaterials += model.materials.size();
            }

            // deferred rendering quad
            vertexData.push_back(XVertex({ DirectX::XMFLOAT3(-1, -1, 0), DirectX::XMFLOAT2(0, 0), DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, 0), -1 }));
            uint16_t lowLeftIndex = static_cast<uint16_t>(vertexData.size() - 1);

            vertexData.push_back(XVertex({ DirectX::XMFLOAT3(-1,  1, 0), DirectX::XMFLOAT2(0, 1), DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, 0), -1 }));
            uint16_t upLeftIndex = static_cast<uint16_t>(vertexData.size() - 1);

            vertexData.push_back(XVertex({ DirectX::XMFLOAT3(1,  1, 0), DirectX::XMFLOAT2(1, 1), DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, 0), -1 }));
            uint16_t upRightIndex = static_cast<uint16_t>(vertexData.size() - 1);

            vertexData.push_back(XVertex({ DirectX::XMFLOAT3(1,  -1, 0), DirectX::XMFLOAT2(1, 0), DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, 0), -1 }));
            uint16_t lowRightIndex = static_cast<uint16_t>(vertexData.size() - 1);

            vertexBufferView = UploadDataToGPU<XVertex, D3D12_VERTEX_BUFFER_VIEW>(vertexData);

            std::vector<std::uint16_t> indexData;
            for (const Model& model : scene.models)
            {
                for (uint32_t i : model.indices)
                {
                    indexData.push_back(static_cast<uint16_t>(i));
                }
            }

            indexData.push_back(lowLeftIndex);
            indexData.push_back(lowRightIndex);
            indexData.push_back(upLeftIndex);

            indexData.push_back(upLeftIndex);
            indexData.push_back(lowRightIndex);
            indexData.push_back(upRightIndex);

            indexBufferView = UploadDataToGPU<uint16_t, D3D12_INDEX_BUFFER_VIEW>(indexData);

            size_t totalIndex = 0;
            for (const Model& model : scene.models)
            {
                for (size_t i = 0; i < model.materials.size(); i++)
                {
                    UploadTexturesToGPU(model.materials[i].textureName, static_cast<int32_t>(totalIndex));
                    totalIndex += 1;
                }
            }
        }

        struct Constants
        {
            DirectX::XMFLOAT4X4 worldViewProj = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
            DirectX::XMFLOAT4X4 worldView = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
            DirectX::XMFLOAT4 lightPos = { 0.0f, 0.0f, 0.0f, 0.0f };
        };

        struct XVertex
        {
            DirectX::XMFLOAT3 Pos;
            DirectX::XMFLOAT2 TextureCoord;
            DirectX::XMFLOAT3 Normal;
            DirectX::XMFLOAT3 Color;
            INT TextureIndex;
        };

        uint64_t AlignBytes(uint64_t size, uint64_t alignment = 256)
        {
            const uint64_t upperBound = size + alignment - 1;
            return upperBound - upperBound % alignment;
        }

        template<typename T>
        uint64_t AlignBytes(uint64_t alignment = 256)
        {
            return AlignBytes(sizeof(T), alignment);
        }

        void SetConstants(const DirectX::XMMATRIX& mvp, const DirectX::XMMATRIX& mv, const DirectX::XMVECTOR& lightPos)
        {
            Constants constantsStruct;
            DirectX::XMStoreFloat4x4(&constantsStruct.worldViewProj, mvp);
            DirectX::XMStoreFloat4x4(&constantsStruct.worldView, mv);
            DirectX::XMStoreFloat4(&constantsStruct.lightPos, lightPos);

            BYTE* mappedData = nullptr;
            constantBuffer->Map(0, nullptr, (void**)&mappedData);
            memcpy(mappedData, &constantsStruct, sizeof(Constants));
            constantBuffer->Unmap(0, nullptr);
        }

        ID3D12RootSignature* CreateRootSignature(UINT numberOfTextures)
        {
            D3D12_DESCRIPTOR_RANGE constantBufferDescriptorRange;
            constantBufferDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            constantBufferDescriptorRange.NumDescriptors = NumberOfConstantStructs;
            constantBufferDescriptorRange.BaseShaderRegister = 0;
            constantBufferDescriptorRange.RegisterSpace = 0;
            constantBufferDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE textureDescriptorRange;
            textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            textureDescriptorRange.NumDescriptors = numberOfTextures + NumberOfGBufferTextures;
            textureDescriptorRange.BaseShaderRegister = 0;
            textureDescriptorRange.RegisterSpace = 0;
            textureDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE descriptorRanges[2] = { constantBufferDescriptorRange, textureDescriptorRange };

            D3D12_ROOT_PARAMETER constantBufferParameter;
            constantBufferParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            constantBufferParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            constantBufferParameter.DescriptorTable.NumDescriptorRanges = 2;
            constantBufferParameter.DescriptorTable.pDescriptorRanges = descriptorRanges;

            D3D12_STATIC_SAMPLER_DESC sampler = {};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.MipLODBias = 0;
            sampler.MaxAnisotropy = 0;
            sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            sampler.MinLOD = 0.0f;
            sampler.MaxLOD = D3D12_FLOAT32_MAX;
            sampler.ShaderRegister = 0;
            sampler.RegisterSpace = 0;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC rootSignatureDescription;
            rootSignatureDescription.NumParameters = 1;
            rootSignatureDescription.pParameters = &constantBufferParameter;
            rootSignatureDescription.NumStaticSamplers = 1;
            rootSignatureDescription.pStaticSamplers = &sampler;
            rootSignatureDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ID3DBlob* rootSignatureData = nullptr;
            D3D_NOT_FAILED(D3D12SerializeRootSignature(&rootSignatureDescription, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureData, nullptr));

            ID3D12RootSignature* result = nullptr;
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateRootSignature(0, rootSignatureData->GetBufferPointer(), rootSignatureData->GetBufferSize(), IID_PPV_ARGS(&result)));

            return result;
        }

        ID3D12PipelineState* CreatePSO(const std::wstring& shaderPath, D3D12_CULL_MODE cullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK, int32_t numRenderTargets = 1)
        {
            UINT flags = D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;

            ID3DBlob* errorBlob = nullptr;
            ID3DBlob* vertexShaderBlob = nullptr;
            //D3D_NOT_FAILED(D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_1", flags, 0, &vertexShaderBlob, nullptr));
            D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_1", flags, 0, &vertexShaderBlob, &errorBlob);

            if (errorBlob)
            {
                LOG((char*)errorBlob->GetBufferPointer());
                errorBlob->Release();
            }

            ID3DBlob* pixelShaderBlob = nullptr;
            D3D_NOT_FAILED(D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_1", flags, 0, &pixelShaderBlob, nullptr));

            constexpr size_t vertexFieldsCount = 5;
            D3D12_INPUT_ELEMENT_DESC inputElementDescription[vertexFieldsCount];

            inputElementDescription[0].SemanticName = "POSITION";
            inputElementDescription[0].SemanticIndex = 0;
            inputElementDescription[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
            inputElementDescription[0].InputSlot = 0;
            inputElementDescription[0].AlignedByteOffset = 0;
            inputElementDescription[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            inputElementDescription[0].InstanceDataStepRate = 0;

            inputElementDescription[1].SemanticName = "TEXCOORD";
            inputElementDescription[1].SemanticIndex = 0;
            inputElementDescription[1].Format = DXGI_FORMAT_R32G32_FLOAT;
            inputElementDescription[1].InputSlot = 0;
            inputElementDescription[1].AlignedByteOffset = 12;
            inputElementDescription[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            inputElementDescription[1].InstanceDataStepRate = 0;

            inputElementDescription[2].SemanticName = "NORMALS";
            inputElementDescription[2].SemanticIndex = 0;
            inputElementDescription[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
            inputElementDescription[2].InputSlot = 0;
            inputElementDescription[2].AlignedByteOffset = 20;
            inputElementDescription[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            inputElementDescription[2].InstanceDataStepRate = 0;

            inputElementDescription[3].SemanticName = "COLOR";
            inputElementDescription[3].SemanticIndex = 0;
            inputElementDescription[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
            inputElementDescription[3].InputSlot = 0;
            inputElementDescription[3].AlignedByteOffset = 32;
            inputElementDescription[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            inputElementDescription[3].InstanceDataStepRate = 0;

            inputElementDescription[4].SemanticName = "TEXINDEX";
            inputElementDescription[4].SemanticIndex = 0;
            inputElementDescription[4].Format = DXGI_FORMAT_R32_SINT;
            inputElementDescription[4].InputSlot = 0;
            inputElementDescription[4].AlignedByteOffset = 44;
            inputElementDescription[4].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            inputElementDescription[4].InstanceDataStepRate = 0;

            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescription = {};
            psoDescription.InputLayout = { inputElementDescription, vertexFieldsCount };
            psoDescription.pRootSignature = rootSignature;
            psoDescription.VS = { (BYTE*)vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
            psoDescription.PS = { (BYTE*)pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

            psoDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            psoDescription.RasterizerState.CullMode = cullMode;
            psoDescription.RasterizerState.FrontCounterClockwise = true;
            psoDescription.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            psoDescription.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            psoDescription.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            psoDescription.RasterizerState.DepthClipEnable = true;
            psoDescription.RasterizerState.MultisampleEnable = false;
            psoDescription.RasterizerState.AntialiasedLineEnable = false;
            psoDescription.RasterizerState.ForcedSampleCount = 0;
            psoDescription.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

            psoDescription.BlendState.AlphaToCoverageEnable = false;
            psoDescription.BlendState.IndependentBlendEnable = false;
            for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            {
                psoDescription.BlendState.RenderTarget[i].BlendEnable = false;
                psoDescription.BlendState.RenderTarget[i].LogicOpEnable = false;
                psoDescription.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
                psoDescription.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
                psoDescription.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
                psoDescription.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
                psoDescription.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
                psoDescription.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
                psoDescription.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
                psoDescription.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            }

            psoDescription.DepthStencilState.DepthEnable = true;
            psoDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            psoDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
            psoDescription.DepthStencilState.StencilEnable = false;
            psoDescription.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
            psoDescription.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

            psoDescription.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
            psoDescription.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            psoDescription.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            psoDescription.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

            psoDescription.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
            psoDescription.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            psoDescription.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            psoDescription.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

            psoDescription.SampleMask = UINT_MAX;
            psoDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDescription.NumRenderTargets = numRenderTargets;

            for (UINT i = 0; i < numRenderTargets; ++i)
            {
                psoDescription.RTVFormats[i] = numRenderTargets == 3 ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
            }

            psoDescription.SampleDesc.Count = 1;
            psoDescription.SampleDesc.Quality = 0;
            psoDescription.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

            ID3D12PipelineState* result = nullptr;
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateGraphicsPipelineState(&psoDescription, IID_PPV_ARGS(&result)));

            return result;
        }

        ID3D12DescriptorHeap* CreateRootDescriptorHeap(UINT descriptorsCount)
        {
            D3D12_DESCRIPTOR_HEAP_DESC heapDescription;
            heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDescription.NumDescriptors = descriptorsCount;
            heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heapDescription.NodeMask = 0;

            ID3D12DescriptorHeap* heap = nullptr;
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&heap)));

            return heap;
        }

        template<typename T>
        void SetBufferViewStrideOrFormat(T& bufferView, UINT stride)
        {
        }

        template<>
        void SetBufferViewStrideOrFormat(D3D12_VERTEX_BUFFER_VIEW& bufferView, UINT stride)
        {
            bufferView.StrideInBytes = stride;
        }

        template<>
        void SetBufferViewStrideOrFormat(D3D12_INDEX_BUFFER_VIEW& bufferView, UINT stride)
        {
            bufferView.Format = DXGI_FORMAT_R16_UINT;
        }

        template <typename T, typename R>
        R UploadDataToGPU(std::vector<T>& data)
        {
            D3D12_RESOURCE_DESC dataBufferDescription;
            dataBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            dataBufferDescription.Alignment = 0;
            dataBufferDescription.Height = 1;
            dataBufferDescription.DepthOrArraySize = 1;
            dataBufferDescription.MipLevels = 1;
            dataBufferDescription.Format = DXGI_FORMAT_UNKNOWN;

            dataBufferDescription.SampleDesc.Count = 1;
            dataBufferDescription.SampleDesc.Quality = 0;

            dataBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            dataBufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
            dataBufferDescription.Width = data.size() * sizeof(T);

            D3D12_HEAP_PROPERTIES uploadHeapProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);

            ID3D12Resource* dataUploadBuffer = nullptr; // todo.pavelza: should be cleared later
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));

            dataUploadBuffer->SetName(L"Data upload buffer.");

            deviceDX12.GetQueue().AddBarrierToList(dataUploadBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);
            BYTE* mappedData = nullptr;
            dataUploadBuffer->Map(0, nullptr, (void**)&mappedData);
            memcpy(mappedData, data.data(), data.size() * sizeof(T));
            dataUploadBuffer->Unmap(0, nullptr);

            D3D12_HEAP_PROPERTIES defaultHeapProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);

            ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&dataBuffer)));
            
            dataBuffer->SetName(L"Data buffer.");

            deviceDX12.GetQueue().AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
            deviceDX12.GetQueue().GetList()->CopyBufferRegion(dataBuffer, 0, dataUploadBuffer, 0, data.size() * sizeof(T));
            deviceDX12.GetQueue().AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
            deviceDX12.GetQueue().Execute();

            R bufferView;
            bufferView.BufferLocation = dataBuffer->GetGPUVirtualAddress();
            SetBufferViewStrideOrFormat(bufferView, sizeof(T));
            bufferView.SizeInBytes = sizeof(T) * static_cast<UINT>(data.size());

            return bufferView;
        }

        void UploadTexturesToGPU(const std::string& textureName, int32_t index)
        {
            Texture texture;
            NOT_FAILED(Load(textureName, texture), false);

            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = texture.GetWidth();
            textureDesc.Height = static_cast<UINT>(texture.GetHeight());
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 totalBytes = 0;
            deviceDX12.GetDevice()->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

            D3D12_RESOURCE_DESC uploadBufferDescription;
            uploadBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            uploadBufferDescription.Alignment = 0;
            uploadBufferDescription.Width = totalBytes;
            uploadBufferDescription.Height = 1;
            uploadBufferDescription.DepthOrArraySize = 1;
            uploadBufferDescription.MipLevels = 1;
            uploadBufferDescription.Format = DXGI_FORMAT_UNKNOWN;
            uploadBufferDescription.SampleDesc.Count = 1;
            uploadBufferDescription.SampleDesc.Quality = 0;
            uploadBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            uploadBufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES uploadProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
            ID3D12Resource* dataUploadBuffer = nullptr; // todo.pavelza: should be cleared later
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&uploadProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDescription, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));

            dataUploadBuffer->SetName(L"Texture upload buffer.");

            deviceDX12.GetQueue().AddBarrierToList(dataUploadBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);

            D3D12_HEAP_PROPERTIES defaultProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
            ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&dataBuffer)));

            dataBuffer->SetName(L"Texture buffer");

            deviceDX12.GetQueue().AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

            BYTE* mappedData = nullptr;
            dataUploadBuffer->Map(0, nullptr, (void**)&mappedData);

            for (size_t i = 0; i < numRows; i++)
            {
                memcpy(mappedData + footprint.Footprint.RowPitch * i, texture.GetBuffer() + rowSizeInBytes * i, rowSizeInBytes);
            }
            dataUploadBuffer->Unmap(0, nullptr);

            D3D12_TEXTURE_COPY_LOCATION dest;
            dest.pResource = dataBuffer;
            dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dest.PlacedFootprint = {};
            dest.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION src;
            src.pResource = dataUploadBuffer;
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = footprint;

            deviceDX12.GetQueue().GetList()->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
            deviceDX12.GetQueue().AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
            
            deviceDX12.GetQueue().Execute();

            // Describe and create a SRV for the texture.
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            D3D12_CPU_DESCRIPTOR_HANDLE textureHandle = GetRootDescriptorHeapTextureHandle(index + NumberOfGBufferTextures);
            deviceDX12.GetDevice()->CreateShaderResourceView(dataBuffer, &srvDesc, textureHandle);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetRootDescriptorHeapTextureHandle(int32_t index)
        {
            // duplicates in render target
            return { SIZE_T(INT64(rootDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(deviceDX12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * (index + NumberOfConstantStructs))) };
        }

        bool DownloadTextureFromGPU(ID3D12Resource* dataBuffer, Texture& texture)
        {
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = texture.GetWidth();
            textureDesc.Height = static_cast<UINT>(texture.GetHeight());
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 totalBytes = 0;
            deviceDX12.GetDevice()->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

            D3D12_RESOURCE_DESC readbackBufferDescription;
            readbackBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            readbackBufferDescription.Alignment = 0;
            readbackBufferDescription.Width = totalBytes;
            readbackBufferDescription.Height = 1;
            readbackBufferDescription.DepthOrArraySize = 1;
            readbackBufferDescription.MipLevels = 1;
            readbackBufferDescription.Format = DXGI_FORMAT_UNKNOWN;
            readbackBufferDescription.SampleDesc.Count = 1;
            readbackBufferDescription.SampleDesc.Quality = 0;
            readbackBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            readbackBufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES readbackProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_READBACK);
            ID3D12Resource* readbackBuffer = nullptr; // todo.pavelza: should be cleared later
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&readbackProperties, D3D12_HEAP_FLAG_NONE, &readbackBufferDescription, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&readbackBuffer)));

            readbackBuffer->SetName(L"Readback buffer.");

            deviceDX12.GetQueue().AddBarrierToList(readbackBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

            D3D12_TEXTURE_COPY_LOCATION dest;
            dest.pResource = readbackBuffer;
            dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dest.PlacedFootprint = footprint;

            D3D12_TEXTURE_COPY_LOCATION src;
            src.pResource = dataBuffer;
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.SubresourceIndex = 0;

            deviceDX12.GetQueue().AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
            deviceDX12.GetQueue().GetList()->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
            deviceDX12.GetQueue().AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

            deviceDX12.GetQueue().Execute();

            // copy from readback buffer to cpu
            BYTE* mappedData = nullptr;
            readbackBuffer->Map(0, nullptr, (void**)&mappedData);

            for (size_t i = 0; i < numRows; i++)
            {
                memcpy(texture.GetBuffer() + rowSizeInBytes * i, mappedData + footprint.Footprint.RowPitch * i, rowSizeInBytes);
            }
            readbackBuffer->Unmap(0, nullptr);

            readbackBuffer->Release();

            return true;
        }

        bool Render(Texture& output)
        {
            // Build the view matrix.
            Matrix mvp = PerspectiveTransform(static_cast<float>(texture.GetWidth()), static_cast<float>(texture.GetHeight())) * ViewTransform(scene.camera);

            Matrix mv = ViewTransform(scene.camera);

            DirectX::XMMATRIX mvpX
            {
                {mvp.m[0], mvp.m[1], mvp.m[2], mvp.m[3]},
                {mvp.m[4], mvp.m[5], mvp.m[6], mvp.m[7]},
                {mvp.m[8], mvp.m[9], mvp.m[10], mvp.m[11]},
                {mvp.m[12], mvp.m[13], mvp.m[14], mvp.m[15]}
            };

            DirectX::XMMATRIX mvX
            {
                {mv.m[0], mv.m[1], mv.m[2], mv.m[3]},
                {mv.m[4], mv.m[5], mv.m[6], mv.m[7]},
                {mv.m[8], mv.m[9], mv.m[10], mv.m[11]},
                {mv.m[12], mv.m[13], mv.m[14], mv.m[15]}
            };

            // calculate light's position in view space
            Vec position_view = ViewTransform(scene.camera) * scene.light.position;
            DirectX::XMVECTOR lightPos = { position_view.x, position_view.y, position_view.z, position_view.w };

            SetConstants(mvpX, mvX, lightPos);

            D3D12_VIEWPORT viewport;
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = static_cast<FLOAT>(texture.GetWidth());
            viewport.Height = static_cast<FLOAT>(texture.GetHeight());
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;

            deviceDX12.GetQueue().GetList()->RSSetViewports(1, &viewport);

            D3D12_RECT scissor;
            scissor.left = 0;
            scissor.top = 0;
            scissor.right = static_cast<LONG>(texture.GetWidth());
            scissor.bottom = static_cast<LONG>(texture.GetHeight());

            deviceDX12.GetQueue().GetList()->RSSetScissorRects(1, &scissor);

            gBuffer->ClearAndSetRenderTargets(deviceDX12.GetQueue());

            ID3D12DescriptorHeap* descriptorHeaps[] = { rootDescriptorHeap };
            deviceDX12.GetQueue().GetList()->SetDescriptorHeaps(1, descriptorHeaps);
            deviceDX12.GetQueue().GetList()->SetGraphicsRootSignature(rootSignature);

            deviceDX12.GetQueue().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView);
            deviceDX12.GetQueue().GetList()->IASetIndexBuffer(&indexBufferView);

            deviceDX12.GetQueue().GetList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            deviceDX12.GetQueue().GetList()->SetGraphicsRootDescriptorTable(0, rootDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

            size_t lastIndex = 0;
            size_t lastVertex = 0;
            for (size_t i = 0; i < scene.models.size(); i++)
            {
                const Model& model = scene.models[i];
                size_t indexCount = model.indices.size();

                if (model.backfaceCulling) // todo: sort to minimize switches?
                {
                    deviceDX12.GetQueue().SetCurrentPipelineStateObject(gbufferPSO);
                }
                else
                {
                    deviceDX12.GetQueue().SetCurrentPipelineStateObject(noCullingGbufferPso);
                }

                deviceDX12.GetQueue().GetList()->DrawIndexedInstanced((UINT)indexCount, 1, (UINT)lastIndex, (INT)lastVertex, 0);
                lastVertex += model.vertices.size();
                lastIndex += indexCount;
            }

            deviceDX12.GetQueue().Execute();

            // draw lighting

            deviceDX12.GetQueue().GetList()->RSSetViewports(1, &viewport);
            deviceDX12.GetQueue().GetList()->RSSetScissorRects(1, &scissor);

            finalImage->ClearAndSetRenderTargets(deviceDX12.GetQueue());

            deviceDX12.GetQueue().GetList()->SetDescriptorHeaps(1, descriptorHeaps);
            deviceDX12.GetQueue().GetList()->SetGraphicsRootSignature(rootSignature);

            deviceDX12.GetQueue().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView);
            deviceDX12.GetQueue().GetList()->IASetIndexBuffer(&indexBufferView);

            deviceDX12.GetQueue().GetList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            deviceDX12.GetQueue().GetList()->SetGraphicsRootDescriptorTable(0, rootDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

            deviceDX12.GetQueue().SetCurrentPipelineStateObject(finalImagePSO);
            deviceDX12.GetQueue().GetList()->DrawIndexedInstanced((UINT)6, 1, (UINT)lastIndex, (INT)0, 0);

            deviceDX12.GetQueue().Execute();

            NOT_FAILED(DownloadTextureFromGPU(finalImage->GetBuffer(0), output), false);

            return true;
        }

        RenderTarget* gBuffer = nullptr;
        RenderTarget* finalImage = nullptr;

        ID3D12DescriptorHeap* rootDescriptorHeap = nullptr;

        ID3D12Resource* constantBuffer = nullptr;

        ID3D12RootSignature* rootSignature = nullptr;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
        D3D12_INDEX_BUFFER_VIEW indexBufferView {};

        ID3D12PipelineState* gbufferPSO = nullptr; // todo.pavelza: should be destroyed somehow?
        ID3D12PipelineState* noCullingGbufferPso = nullptr; // todo.pavelza: should be destroyed somehow?
        ID3D12PipelineState* finalImagePSO = nullptr; // todo.pavelza: should be destroyed somehow?


        const DeviceDX12& deviceDX12;
        const Scene& scene;
        const Texture& texture;
    };

    bool SceneRendererDX12::Render(const Scene& scene, Texture& texture)
    {
        if (!context)
        {
            context = std::make_shared<RendererDX12Context>(scene, texture, std::wstring(shaderFolderPath.begin(), shaderFolderPath.end()), deviceDX12);
        }

        return context->Render(texture);
    }
}