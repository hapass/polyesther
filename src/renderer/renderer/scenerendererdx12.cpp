#include <renderer/scenerendererdx12.h>
#include <renderer/utils.h>
#include <renderer/devicedx12.h>

#include <d3d12.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

namespace Renderer
{
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
            const UINT numberOfConstantStructs = UINT(1);
            const UINT numberOfTextures = UINT(allMaterials.size() + 1); // add one extra texture for 0 texture case to even work, and we can use that as an error texture in the future

            rootDescriptorHeap = CreateRootDescriptorHeap(numberOfConstantStructs + numberOfTextures);

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
            rootSignature = CreateRootSignature(numberOfConstantStructs, numberOfTextures);

            // pso
            ID3D12PipelineState* pso = CreatePSO(shaderPath); // todo.pavelza: should be destroyed somehow?

            // queue
            deviceDX12.GetQueue().SetCurrentPipelineStateObject(pso);

            // move constant buffer into correct state
            deviceDX12.GetQueue().AddBarrierToList(constantBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);

            // depth
            depthBufferHandle = CreateDepthBuffer(texture.GetWidth(), texture.GetHeight());

            // render target
            ID3D12DescriptorHeap* renderTargetDescriptorHeap = CreateRTVDescriptorHeap();

            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = texture.GetWidth();
            textureDesc.Height = static_cast<UINT>(texture.GetHeight());
            textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 totalBytes = 0;
            deviceDX12.GetDevice()->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = textureDesc.Format;
            std::copy(clearColor, clearColor + 4, clearValue.Color);

            D3D12_HEAP_PROPERTIES defaultProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&currentBuffer)));

            currentBuffer->SetName(L"Render target.");

            currentBufferHandle = { SIZE_T(INT64(renderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr)) };
            deviceDX12.GetDevice()->CreateRenderTargetView(currentBuffer, nullptr, currentBufferHandle);

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
                            vert.materialId == -1 ? -1 : INT(totalMaterials + vert.materialId)
                            })
                    );
                }
                totalMaterials += model.materials.size();
            }

            vertexBufferView = UploadDataToGPU<XVertex, D3D12_VERTEX_BUFFER_VIEW>(vertexData);

            std::vector<std::uint16_t> indexData;
            for (const Model& model : scene.models)
            {
                for (uint32_t i : model.indices)
                {
                    indexData.push_back(static_cast<uint16_t>(i));
                }
            }

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

        ID3D12RootSignature* CreateRootSignature(UINT numberOfConstantStructs, UINT numberOfTextures)
        {
            D3D12_DESCRIPTOR_RANGE constantBufferDescriptorRange;
            constantBufferDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            constantBufferDescriptorRange.NumDescriptors = numberOfConstantStructs;
            constantBufferDescriptorRange.BaseShaderRegister = 0;
            constantBufferDescriptorRange.RegisterSpace = 0;
            constantBufferDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE textureDescriptorRange;
            textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            textureDescriptorRange.NumDescriptors = numberOfTextures;
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

        ID3D12PipelineState* CreatePSO(const std::wstring& shaderPath)
        {
            UINT flags = D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;

            //ID3DBlob* errorBlob = nullptr;
            ID3DBlob* vertexShaderBlob = nullptr;
            D3D_NOT_FAILED(D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_1", flags, 0, &vertexShaderBlob, nullptr));

            //if (errorBlob)
            //{
            //    LOG((char*)errorBlob->GetBufferPointer());
            //    errorBlob->Release();
            //}

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
            inputElementDescription[4].Format = DXGI_FORMAT_R32_UINT;
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
            psoDescription.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
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
            psoDescription.NumRenderTargets = 1;
            psoDescription.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
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

        ID3D12DescriptorHeap* CreateRTVDescriptorHeap()
        {
            D3D12_DESCRIPTOR_HEAP_DESC heapDescription;
            heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDescription.NumDescriptors = 1;
            heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            heapDescription.NodeMask = 0;

            ID3D12DescriptorHeap* heap = nullptr;
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&heap)));

            return heap;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE CreateDepthBuffer(size_t width, size_t height)
        {
            D3D12_RESOURCE_DESC depthBufferDescription;
            depthBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            depthBufferDescription.Alignment = 0;
            depthBufferDescription.Width = width;
            depthBufferDescription.Height = static_cast<UINT>(height);
            depthBufferDescription.DepthOrArraySize = 1;
            depthBufferDescription.MipLevels = 1;
            depthBufferDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

            depthBufferDescription.SampleDesc.Count = 1;
            depthBufferDescription.SampleDesc.Quality = 0;

            depthBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            depthBufferDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE clearValue;
            clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            clearValue.DepthStencil.Depth = 1.0f;
            clearValue.DepthStencil.Stencil = 0;

            D3D12_HEAP_PROPERTIES defaultHeapProperties = deviceDX12.GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
            ID3D12Resource* depthStencilBuffer = nullptr;
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &depthBufferDescription, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&depthStencilBuffer)));

            depthStencilBuffer->SetName(L"Depth stencil buffer.");

            D3D12_DESCRIPTOR_HEAP_DESC depthStencilViewHeapDescription;
            depthStencilViewHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            depthStencilViewHeapDescription.NumDescriptors = 1;
            depthStencilViewHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            depthStencilViewHeapDescription.NodeMask = 0;

            ID3D12DescriptorHeap* depthStencilViewHeap = nullptr;
            D3D_NOT_FAILED(deviceDX12.GetDevice()->CreateDescriptorHeap(&depthStencilViewHeapDescription, IID_PPV_ARGS(&depthStencilViewHeap)));

            D3D12_CPU_DESCRIPTOR_HANDLE resultHandle(depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart());
            deviceDX12.GetDevice()->CreateDepthStencilView(depthStencilBuffer, nullptr, resultHandle);
            deviceDX12.GetQueue().AddBarrierToList(depthStencilBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

            deviceDX12.GetQueue().Execute();

            return resultHandle;
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

            D3D12_CPU_DESCRIPTOR_HANDLE secondConstantBufferHandle{ SIZE_T(INT64(rootDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(deviceDX12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * (index + 1))) };
            deviceDX12.GetDevice()->CreateShaderResourceView(dataBuffer, &srvDesc, secondConstantBufferHandle);
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

            deviceDX12.GetQueue().GetList()->ClearRenderTargetView(currentBufferHandle, clearColor, 0, nullptr);
            deviceDX12.GetQueue().GetList()->ClearDepthStencilView(depthBufferHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

            deviceDX12.GetQueue().GetList()->OMSetRenderTargets(1, &currentBufferHandle, true, &depthBufferHandle);

            ID3D12DescriptorHeap* descriptorHeaps[] = { rootDescriptorHeap };
            deviceDX12.GetQueue().GetList()->SetDescriptorHeaps(1, descriptorHeaps);
            deviceDX12.GetQueue().GetList()->SetGraphicsRootSignature(rootSignature);

            deviceDX12.GetQueue().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView);
            deviceDX12.GetQueue().GetList()->IASetIndexBuffer(&indexBufferView);

            deviceDX12.GetQueue().GetList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            deviceDX12.GetQueue().GetList()->SetGraphicsRootDescriptorTable(0, rootDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

            UINT lastIndex = 0;
            UINT lastVertex = 0;
            for (const Model& model : scene.models)
            {
                UINT indexCount = (UINT)model.indices.size();
                deviceDX12.GetQueue().GetList()->DrawIndexedInstanced(indexCount, 1, lastIndex, lastVertex, 0);
                lastVertex += model.vertices.size();
                lastIndex += indexCount;
            }

            deviceDX12.GetQueue().Execute();

            NOT_FAILED(DownloadTextureFromGPU(currentBuffer, output), false);

            return true;
        }


        FLOAT clearColor[4] = { 0.0f, 0.f, 0.f, 1.000000000f };
        ID3D12DescriptorHeap* rootDescriptorHeap = nullptr;

        ID3D12Resource* constantBuffer = nullptr;
        ID3D12Resource* currentBuffer = nullptr; // render target
        D3D12_CPU_DESCRIPTOR_HANDLE currentBufferHandle {};
        D3D12_CPU_DESCRIPTOR_HANDLE depthBufferHandle {};
        ID3D12RootSignature* rootSignature = nullptr;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
        D3D12_INDEX_BUFFER_VIEW indexBufferView {};

        const DeviceDX12& deviceDX12;
        const Scene& scene;
        const Texture& texture;
    };

    bool SceneRendererDX12::Render(const Scene& scene, Texture& texture)
    {
        if (!context)
        {
            context = std::make_shared<RendererDX12Context>(scene, texture, std::wstring(shaderPath.begin(), shaderPath.end()), deviceDX12);
        }

        return context->Render(texture);
    }
}