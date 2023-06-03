#include <renderer/rendererdx12.h>
#include <renderer/utils.h>

#include <d3d12.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

namespace Renderer
{
    struct GraphicsQueue
    {
        GraphicsQueue(ID3D12Device* device)
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            D3D_NOT_FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));

            D3D_NOT_FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
            D3D_NOT_FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&commandList)));

            D3D_NOT_FAILED(device->CreateFence(currentFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
            NOT_FAILED(fenceEventHandle = CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS), 0);
        }

        GraphicsQueue(const GraphicsQueue& other) = delete;
        GraphicsQueue(GraphicsQueue && other) noexcept = delete;
        GraphicsQueue& operator=(const GraphicsQueue& other) = delete;
        GraphicsQueue& operator=(GraphicsQueue&& other) noexcept = delete;

        ~GraphicsQueue()
        {
            fence->Release();
            commandList->Release();
            allocator->Release();
            queue->Release();
        }

        ID3D12GraphicsCommandList* GetList()
        {
            return commandList;
        }

        void AddBarrierToList(ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
        {
            D3D12_RESOURCE_BARRIER barrier;
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            barrier.Transition.StateBefore = from;
            barrier.Transition.StateAfter = to;
            barrier.Transition.pResource = resource;

            commandList->ResourceBarrier(1, &barrier);
        }

        void Execute(ID3D12PipelineState* pso)
        {
            commandList->Close();

            ID3D12CommandList* cmdsLists[1] = { commandList };
            queue->ExecuteCommandLists(1, cmdsLists);

            WaitForCommandListCompletion();

            D3D_NOT_FAILED(allocator->Reset());
            D3D_NOT_FAILED(commandList->Reset(allocator, pso));
        }

    private:
        void WaitForCommandListCompletion()
        {
            currentFenceValue++;
            queue->Signal(fence, currentFenceValue);

            if (fence->GetCompletedValue() != currentFenceValue)
            {
                D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
                WaitForSingleObject(fenceEventHandle, INFINITE);
                ResetEvent(fenceEventHandle);
            }
        }

        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12GraphicsCommandList* commandList = nullptr;
        ID3D12CommandQueue* queue = nullptr;
        ID3D12Fence* fence = nullptr;

        UINT64 currentFenceValue = 0;
        HANDLE fenceEventHandle = 0;
    };

    struct RendererDX12Context
    {
        RendererDX12Context(const Scene& scene, const Texture& texture, const std::wstring& shaderPath)
            : scene(scene)
            , texture(texture)
        {
            const Model& model = scene.models[0];

            ID3D12Debug* debugController = nullptr;
            D3D_NOT_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
            debugController->EnableDebugLayer();
            debugController->Release();

            D3D_NOT_FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));

            // rendering
            const UINT numberOfConstantStructs = UINT(1);
            const UINT numberOfTextures = UINT(model.materials.size() + 1); // add one extra texture for 0 texture case to even work, and we can use that as an error texture in the future

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

            D3D12_HEAP_PROPERTIES uploadHeapProperties = device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
            D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer)));

            D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDescription;
            constantBufferViewDescription.BufferLocation = constantBuffer->GetGPUVirtualAddress();
            constantBufferViewDescription.SizeInBytes = static_cast<UINT>(uploadBufferDescription.Width);

            device->CreateConstantBufferView(&constantBufferViewDescription, rootDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

            // root signature
            rootSignature = CreateRootSignature(numberOfConstantStructs, numberOfTextures);

            // pso
            pso = CreatePSO(shaderPath); // todo.pavelza: keep arguments? it seems to be easier when the state is not shared at all

            // queue
            queue = std::make_unique<GraphicsQueue>(device);
            queue->GetList()->SetPipelineState(pso);

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
            device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

            D3D12_HEAP_PROPERTIES defaultProperties = device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&currentBuffer)));

            currentBufferHandle = { SIZE_T(INT64(renderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr)) };
            device->CreateRenderTargetView(currentBuffer, nullptr, currentBufferHandle);

            // upload static geometry
            std::vector<XVertex> vertexData;
            for (const Vertex& vert : model.vertices)
            {
                vertexData.push_back(
                    XVertex({
                        DirectX::XMFLOAT3(vert.position.x, vert.position.y, vert.position.z),
                        DirectX::XMFLOAT2(vert.textureCoord.x, vert.textureCoord.y),
                        DirectX::XMFLOAT3(vert.normal.x, vert.normal.y, vert.normal.z),
                        DirectX::XMFLOAT3(vert.color.GetVec().x, vert.color.GetVec().y, vert.color.GetVec().z),
                        UINT(vert.materialId)
                        })
                );
            }

            vertexBufferView = UploadDataToGPU<XVertex, D3D12_VERTEX_BUFFER_VIEW>(vertexData);

            std::vector<std::uint16_t> indexData;
            for (uint32_t i : model.indices)
            {
                indexData.push_back(static_cast<uint16_t>(i));
            }

            indexBufferView = UploadDataToGPU<uint16_t, D3D12_INDEX_BUFFER_VIEW>(indexData);

            for (size_t i = 0; i < model.materials.size(); i++)
            {
                UploadTexturesToGPU(model.materials[i].textureName, static_cast<int32_t>(i));
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
            UINT textureCount = 0;
        };

        struct XVertex
        {
            DirectX::XMFLOAT3 Pos;
            DirectX::XMFLOAT2 TextureCoord;
            DirectX::XMFLOAT3 Normal;
            DirectX::XMFLOAT3 Color;
            UINT TextureIndex;
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

        void SetConstants(const DirectX::XMMATRIX& mvp, const DirectX::XMMATRIX& mv, const DirectX::XMVECTOR& lightPos, UINT numberOfTextures)
        {
            Constants constantsStruct;
            DirectX::XMStoreFloat4x4(&constantsStruct.worldViewProj, mvp);
            DirectX::XMStoreFloat4x4(&constantsStruct.worldView, mv);
            DirectX::XMStoreFloat4(&constantsStruct.lightPos, lightPos);

            constantsStruct.textureCount = numberOfTextures;

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
            D3D_NOT_FAILED(device->CreateRootSignature(0, rootSignatureData->GetBufferPointer(), rootSignatureData->GetBufferSize(), IID_PPV_ARGS(&result)));

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
            psoDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
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
            D3D_NOT_FAILED(device->CreateGraphicsPipelineState(&psoDescription, IID_PPV_ARGS(&result)));

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
            D3D_NOT_FAILED(device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&heap)));

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
            D3D_NOT_FAILED(device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&heap)));

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

            D3D12_HEAP_PROPERTIES defaultHeapProperties = device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
            ID3D12Resource* depthStencilBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &depthBufferDescription, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&depthStencilBuffer)));

            D3D12_DESCRIPTOR_HEAP_DESC depthStencilViewHeapDescription;
            depthStencilViewHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            depthStencilViewHeapDescription.NumDescriptors = 1;
            depthStencilViewHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            depthStencilViewHeapDescription.NodeMask = 0;

            ID3D12DescriptorHeap* depthStencilViewHeap = nullptr;
            D3D_NOT_FAILED(device->CreateDescriptorHeap(&depthStencilViewHeapDescription, IID_PPV_ARGS(&depthStencilViewHeap)));

            D3D12_CPU_DESCRIPTOR_HANDLE resultHandle(depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart());
            device->CreateDepthStencilView(depthStencilBuffer, nullptr, resultHandle);
            queue->AddBarrierToList(depthStencilBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

            queue->Execute(pso);

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

            D3D12_HEAP_PROPERTIES uploadHeapProperties = device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);

            ID3D12Resource* dataUploadBuffer = nullptr; // todo.pavelza: should be cleared later
            D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));

            BYTE* mappedData = nullptr;
            dataUploadBuffer->Map(0, nullptr, (void**)&mappedData);
            memcpy(mappedData, data.data(), data.size() * sizeof(T));
            dataUploadBuffer->Unmap(0, nullptr);

            D3D12_HEAP_PROPERTIES defaultHeapProperties = device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);

            ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&dataBuffer)));
            
            queue->GetList()->CopyBufferRegion(dataBuffer, 0, dataUploadBuffer, 0, data.size() * sizeof(T));
            queue->AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
            queue->Execute(pso);

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
            device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

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

            D3D12_HEAP_PROPERTIES uploadProperties = device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
            ID3D12Resource* dataUploadBuffer = nullptr; // todo.pavelza: should be cleared later
            D3D_NOT_FAILED(device->CreateCommittedResource(&uploadProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));

            D3D12_HEAP_PROPERTIES defaultProperties = device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
            ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&dataBuffer)));

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

            queue->GetList()->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
            queue->AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
            
            queue->Execute(pso);

            // Describe and create a SRV for the texture.
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            D3D12_CPU_DESCRIPTOR_HANDLE secondConstantBufferHandle{ SIZE_T(INT64(rootDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * (index + 1))) };
            device->CreateShaderResourceView(dataBuffer, &srvDesc, secondConstantBufferHandle);
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
            device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

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

            D3D12_HEAP_PROPERTIES readbackProperties = device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_READBACK);
            ID3D12Resource* readbackBuffer = nullptr; // todo.pavelza: should be cleared later
            D3D_NOT_FAILED(device->CreateCommittedResource(&readbackProperties, D3D12_HEAP_FLAG_NONE, &readbackBufferDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer)));

            D3D12_TEXTURE_COPY_LOCATION dest;
            dest.pResource = readbackBuffer;
            dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dest.PlacedFootprint = footprint;

            D3D12_TEXTURE_COPY_LOCATION src;
            src.pResource = dataBuffer;
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.SubresourceIndex = 0;

            queue->AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
            queue->GetList()->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
            queue->AddBarrierToList(dataBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

            queue->Execute(pso);

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

            SetConstants(mvpX, mvX, lightPos, (UINT)scene.models[0].materials.size());

            D3D12_VIEWPORT viewport;
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = static_cast<FLOAT>(texture.GetWidth());
            viewport.Height = static_cast<FLOAT>(texture.GetHeight());
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;

            queue->GetList()->RSSetViewports(1, &viewport);

            D3D12_RECT scissor;
            scissor.left = 0;
            scissor.top = 0;
            scissor.right = static_cast<LONG>(texture.GetWidth());
            scissor.bottom = static_cast<LONG>(texture.GetHeight());

            queue->GetList()->RSSetScissorRects(1, &scissor);

            // todo.pavelza: when does it become copy dest?
            queue->AddBarrierToList(currentBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);

            FLOAT clearColor[4] = { 0.0f, 0.f, 0.f, 1.000000000f };
            queue->GetList()->ClearRenderTargetView(currentBufferHandle, clearColor, 0, nullptr);
            queue->GetList()->ClearDepthStencilView(depthBufferHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

            queue->GetList()->OMSetRenderTargets(1, &currentBufferHandle, true, &depthBufferHandle);

            ID3D12DescriptorHeap* descriptorHeaps[] = { rootDescriptorHeap };
            queue->GetList()->SetDescriptorHeaps(1, descriptorHeaps);
            queue->GetList()->SetGraphicsRootSignature(rootSignature);

            queue->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView);
            queue->GetList()->IASetIndexBuffer(&indexBufferView);

            queue->GetList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            queue->GetList()->SetGraphicsRootDescriptorTable(0, rootDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

            queue->GetList()->DrawIndexedInstanced((UINT)scene.models[0].indices.size(), 1, 0, 0, 0);

            queue->Execute(pso);

            NOT_FAILED(DownloadTextureFromGPU(currentBuffer, output), false);

            return true;
        }

        ID3D12DescriptorHeap* rootDescriptorHeap = nullptr;
        ID3D12Device* device = nullptr;
        ID3D12Resource* constantBuffer = nullptr;
        ID3D12Resource* currentBuffer = nullptr; // render target
        D3D12_CPU_DESCRIPTOR_HANDLE currentBufferHandle {};
        D3D12_CPU_DESCRIPTOR_HANDLE depthBufferHandle {};
        ID3D12RootSignature* rootSignature = nullptr;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
        D3D12_INDEX_BUFFER_VIEW indexBufferView {};
        ID3D12PipelineState* pso = nullptr;

        std::unique_ptr<GraphicsQueue> queue;

        const Scene& scene;
        const Texture& texture;
    };

    bool RendererDX12::Render(const Scene& scene, Texture& texture)
    {
        if (!context)
        {
            context = std::make_shared<RendererDX12Context>(scene, texture, std::wstring(shaderPath.begin(), shaderPath.end()));
        }

        return context->Render(texture);
    }
}