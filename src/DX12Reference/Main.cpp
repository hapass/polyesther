#define _USE_MATH_DEFINES

#include <Windows.h>
#include <dxgi.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3dcompiler.h>

#include <stdint.h>
#include <cassert>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include <array>
#include <cmath>
#include <wincodec.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "../common/stb_image.h"
#include "../common/profile.h"
#include "../common/math.h"
#include "../common/common.h"

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

void SetModelViewProjection(ID3D12Resource* constantBuffer, const DirectX::XMMATRIX& mvp, const DirectX::XMMATRIX& mv, const DirectX::XMVECTOR& lightPos)
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

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    bool isRunning = true;

    WNDCLASS MainWindowClass = {};
    MainWindowClass.lpfnWndProc = MainWindowProc;
    MainWindowClass.hInstance = hInstance;
    MainWindowClass.lpszClassName = L"MainWindow";

    if (RegisterClassW(&MainWindowClass))
    {
        RECT clientArea;
        clientArea.left = 0;
        clientArea.top = 0;
        clientArea.right = WindowWidth;
        clientArea.bottom = WindowHeight;

        AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);

        HWND window;
        NOT_FAILED(window = CreateWindowExW(
            0,
            L"MainWindow",
            L"Software Rasterizer",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            clientArea.right - clientArea.left,
            clientArea.bottom - clientArea.top,
            0,
            0,
            hInstance,
            0
        ), 0);

        int32_t channels;
        NOT_FAILED(Texture = stbi_load("../../assets/texture.jpg", &TextureWidth, &TextureHeight, &channels, 4), 0);

        auto frameExpectedTime = chrono::milliseconds(1000 / FPS);

        LoadContext context;

        // init model
        models.push_back(LoadOBJ("../../assets/cube.obj", 50.0f, Vec { 0.0f, 0.0f, 0.0f, 1.0f }, context));

        // init light
        models.push_back(LoadOBJ("../../assets/cube.obj", 10.0f, Vec { 100.0f, 100.0f, 100.0f, 1.0f }, context));

        Camera camera;
        camera.position.z = 200;
        camera.position.x = 10;
        camera.position.y = 0;

        camera.pitch = 0.0f;
        camera.yaw = 0.0f;

        camera.forward = Vec{ 0.0f, 0.0f, -10.0f, 0.0f };
        camera.left = Vec{ -10.0f, 0.0f, 0.0f, 0.0f };

        light.position = models[1].position;

        ID3D12Debug* debugController = nullptr;
        D3D_NOT_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
        debugController->Release();

        IDXGIFactory* factory = nullptr;
        D3D_NOT_FAILED(CreateDXGIFactory(IID_PPV_ARGS(&factory)));

        IDXGIAdapter* adapter = nullptr;
        factory->EnumAdapters(0, &adapter);

        ID3D12Device* device = nullptr;
        D3D_NOT_FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

        ID3D12CommandQueue* queue = nullptr;
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        D3D_NOT_FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));

        DXGI_SWAP_CHAIN_DESC swapChainDescription;

        swapChainDescription.BufferDesc.Width = GameWidth;
        swapChainDescription.BufferDesc.Height = GameHeight;
        swapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDescription.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        swapChainDescription.SampleDesc.Count = 1;
        swapChainDescription.SampleDesc.Quality = 0;

        swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDescription.BufferCount = 2;

        swapChainDescription.OutputWindow = window;
        swapChainDescription.Windowed = true;

        swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDescription.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        IDXGISwapChain* swapChain = nullptr;
        D3D_NOT_FAILED(factory->CreateSwapChain(queue, &swapChainDescription, &swapChain));

        D3D12_DESCRIPTOR_HEAP_DESC renderTargetViewHeapDescription;
        renderTargetViewHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        renderTargetViewHeapDescription.NumDescriptors = 2;
        renderTargetViewHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        renderTargetViewHeapDescription.NodeMask = 0;

        ID3D12DescriptorHeap* renderTargetViewHeap = nullptr;
        D3D_NOT_FAILED(device->CreateDescriptorHeap(&renderTargetViewHeapDescription, IID_PPV_ARGS(&renderTargetViewHeap)));

        ID3D12Resource* zeroBuffer = nullptr;
        D3D_NOT_FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&zeroBuffer)));

        D3D12_CPU_DESCRIPTOR_HANDLE zeroBufferHandle{ SIZE_T(INT64(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart().ptr)) };
        device->CreateRenderTargetView(zeroBuffer, nullptr, zeroBufferHandle);

        ID3D12Resource* firstBuffer = nullptr;
        D3D_NOT_FAILED(swapChain->GetBuffer(1, IID_PPV_ARGS(&firstBuffer)));

        D3D12_CPU_DESCRIPTOR_HANDLE firstBufferHandle{ SIZE_T(INT64(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV))) };
        device->CreateRenderTargetView(firstBuffer, nullptr, firstBufferHandle);

        D3D12_RESOURCE_DESC depthBufferDescription;
        depthBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthBufferDescription.Alignment = 0;
        depthBufferDescription.Width = GameWidth;
        depthBufferDescription.Height = GameHeight;
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

        D3D12_HEAP_PROPERTIES defaultHeapProperties;
        defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProperties.VisibleNodeMask = 1;
        defaultHeapProperties.CreationNodeMask = 1;

        ID3D12Resource* depthStencilBuffer = nullptr;
        D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &depthBufferDescription, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&depthStencilBuffer)));

        D3D12_DESCRIPTOR_HEAP_DESC depthStencilViewHeapDescription;
        depthStencilViewHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        depthStencilViewHeapDescription.NumDescriptors = 1;
        depthStencilViewHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        depthStencilViewHeapDescription.NodeMask = 0;

        ID3D12DescriptorHeap* depthStencilViewHeap = nullptr;
        D3D_NOT_FAILED(device->CreateDescriptorHeap(&depthStencilViewHeapDescription, IID_PPV_ARGS(&depthStencilViewHeap)));

        D3D12_CPU_DESCRIPTOR_HANDLE depthBufferHandle(depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart());
        device->CreateDepthStencilView(depthStencilBuffer, nullptr, depthBufferHandle);

        ID3D12CommandAllocator* allocator = nullptr;
        D3D_NOT_FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));

        ID3D12GraphicsCommandList* commandList = nullptr;
        D3D_NOT_FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&commandList)));

        D3D12_RESOURCE_BARRIER depthBufferWriteTransition;
        depthBufferWriteTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBufferWriteTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

        depthBufferWriteTransition.Transition.pResource = depthStencilBuffer;
        depthBufferWriteTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        depthBufferWriteTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBufferWriteTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        commandList->ResourceBarrier(1, &depthBufferWriteTransition);

        UINT64 currentFenceValue = 0;

        ID3D12Fence* fence = nullptr;
        D3D_NOT_FAILED(device->CreateFence(currentFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HANDLE fenceEventHandle = CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
        assert(fenceEventHandle);

        bool isZeroBufferInTheBack = true;

        // rendering

        // Constant buffer
        D3D12_HEAP_PROPERTIES uploadHeapProperties;
        uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeapProperties.VisibleNodeMask = 1;
        uploadHeapProperties.CreationNodeMask = 1;

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

        ID3D12Resource* uploadBuffer = nullptr;
        D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)));

        D3D12_DESCRIPTOR_HEAP_DESC constantBufferDescriptorHeapDescription;
        constantBufferDescriptorHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        constantBufferDescriptorHeapDescription.NumDescriptors = 2;
        constantBufferDescriptorHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        constantBufferDescriptorHeapDescription.NodeMask = 0;

        ID3D12DescriptorHeap* constantBufferDescriptorHeap = nullptr;
        D3D_NOT_FAILED(device->CreateDescriptorHeap(&constantBufferDescriptorHeapDescription, IID_PPV_ARGS(&constantBufferDescriptorHeap)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDescription;
        constantBufferViewDescription.BufferLocation = uploadBuffer->GetGPUVirtualAddress();
        constantBufferViewDescription.SizeInBytes = static_cast<UINT>(uploadBufferDescription.Width);

        device->CreateConstantBufferView(&constantBufferViewDescription, constantBufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        // root signature
        D3D12_DESCRIPTOR_RANGE constantBufferDescriptorRange;
        constantBufferDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        constantBufferDescriptorRange.NumDescriptors = 1;
        constantBufferDescriptorRange.BaseShaderRegister = 0;
        constantBufferDescriptorRange.RegisterSpace = 0;
        constantBufferDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE textureDescriptorRange;
        textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        textureDescriptorRange.NumDescriptors = 1;
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

        ID3D12RootSignature* rootSignature = nullptr;
        D3D_NOT_FAILED(device->CreateRootSignature(0, rootSignatureData->GetBufferPointer(), rootSignatureData->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

        // pso
        ID3DBlob* vertexShaderBlob = nullptr;
        D3D_NOT_FAILED(D3DCompileFromFile(L"color.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_0", 0, 0, &vertexShaderBlob, nullptr));

        ID3DBlob* pixelShaderBlob = nullptr;
        D3D_NOT_FAILED(D3DCompileFromFile(L"color.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_0", 0, 0, &pixelShaderBlob, nullptr));

        constexpr size_t vertexFieldsCount = 3;
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

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateObjectDescription = {};
        pipelineStateObjectDescription.InputLayout = { inputElementDescription, vertexFieldsCount };
        pipelineStateObjectDescription.pRootSignature = rootSignature;
        pipelineStateObjectDescription.VS = { (BYTE*)vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
        pipelineStateObjectDescription.PS = { (BYTE*)pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

        pipelineStateObjectDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pipelineStateObjectDescription.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        pipelineStateObjectDescription.RasterizerState.FrontCounterClockwise = true;
        pipelineStateObjectDescription.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        pipelineStateObjectDescription.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        pipelineStateObjectDescription.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        pipelineStateObjectDescription.RasterizerState.DepthClipEnable = true;
        pipelineStateObjectDescription.RasterizerState.MultisampleEnable = false;
        pipelineStateObjectDescription.RasterizerState.AntialiasedLineEnable = false;
        pipelineStateObjectDescription.RasterizerState.ForcedSampleCount = 0;
        pipelineStateObjectDescription.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        pipelineStateObjectDescription.BlendState.AlphaToCoverageEnable = false;
        pipelineStateObjectDescription.BlendState.IndependentBlendEnable = false;
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            pipelineStateObjectDescription.BlendState.RenderTarget[i].BlendEnable = false;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].LogicOpEnable = false;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
            pipelineStateObjectDescription.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        pipelineStateObjectDescription.DepthStencilState.DepthEnable = true;
        pipelineStateObjectDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pipelineStateObjectDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pipelineStateObjectDescription.DepthStencilState.StencilEnable = false;
        pipelineStateObjectDescription.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        pipelineStateObjectDescription.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

        pipelineStateObjectDescription.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        pipelineStateObjectDescription.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        pipelineStateObjectDescription.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        pipelineStateObjectDescription.SampleMask = UINT_MAX;
        pipelineStateObjectDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateObjectDescription.NumRenderTargets = 1;
        pipelineStateObjectDescription.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipelineStateObjectDescription.SampleDesc.Count = 1;
        pipelineStateObjectDescription.SampleDesc.Quality = 0;
        pipelineStateObjectDescription.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        ID3D12PipelineState* pso = nullptr;
        D3D_NOT_FAILED(device->CreateGraphicsPipelineState(&pipelineStateObjectDescription, IID_PPV_ARGS(&pso)));

        // upload static geometry

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

        D3D12_RESOURCE_BARRIER genericReadBufferTransition;
        genericReadBufferTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        genericReadBufferTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        genericReadBufferTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        genericReadBufferTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
        genericReadBufferTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        ID3D12Resource* vertexBuffer = nullptr;
        {
            std::vector<XVertex> data;

            for (uint32_t i = 0; i < indicesObj.size(); i++)
            {
                data.push_back(
                    XVertex({
                        DirectX::XMFLOAT3(verticesObj[indicesObj[i].vert].x, verticesObj[indicesObj[i].vert].y, verticesObj[indicesObj[i].vert].z),
                        DirectX::XMFLOAT2(textureX[indicesObj[i].text], textureY[indicesObj[i].text]),
                        DirectX::XMFLOAT3(normalsObj[indicesObj[i].norm].x, normalsObj[indicesObj[i].norm].y, normalsObj[indicesObj[i].norm].z)
                    })
                );
            }

            dataBufferDescription.Width = data.size() * sizeof(XVertex);

            ID3D12Resource* dataUploadBuffer = nullptr; // TODO: should be cleared later
            D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));
        
            BYTE* mappedData = nullptr;
            dataUploadBuffer->Map(0, nullptr, (void**)&mappedData);
            memcpy(mappedData, data.data(), data.size() * sizeof(XVertex));
            dataUploadBuffer->Unmap(0, nullptr);

            ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&dataBuffer)));
            commandList->CopyBufferRegion(dataBuffer, 0, dataUploadBuffer, 0, data.size() * sizeof(XVertex));

            genericReadBufferTransition.Transition.pResource = dataBuffer;
            commandList->ResourceBarrier(1, &genericReadBufferTransition);

            commandList->Close();

            ID3D12CommandList* cmdsLists[1] = { commandList };
            queue->ExecuteCommandLists(1, cmdsLists);

            currentFenceValue++;
            queue->Signal(fence, currentFenceValue);

            if (fence->GetCompletedValue() != currentFenceValue)
            {
                D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
                WaitForSingleObject(fenceEventHandle, INFINITE);
                ResetEvent(fenceEventHandle);
            }

            D3D_NOT_FAILED(allocator->Reset());
            D3D_NOT_FAILED(commandList->Reset(allocator, pso));

            vertexBuffer = dataBuffer;
        }

        ID3D12Resource* indexBuffer = nullptr;
        {
            std::vector<std::uint16_t> data;

            for (uint32_t i = 0; i < indicesObj.size(); i++)
            {
                data.push_back(static_cast<uint16_t>(i));
            }

            dataBufferDescription.Width = data.size() * sizeof(std::uint16_t);

            ID3D12Resource* dataUploadBuffer = nullptr; // TODO: should be cleared later
            D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));

            BYTE* mappedData = nullptr;
            dataUploadBuffer->Map(0, nullptr, (void**)&mappedData);
            memcpy(mappedData, data.data(), data.size() * sizeof(std::uint16_t));
            dataUploadBuffer->Unmap(0, nullptr);

            ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &dataBufferDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&dataBuffer)));
            commandList->CopyBufferRegion(dataBuffer, 0, dataUploadBuffer, 0, data.size() * sizeof(std::uint16_t));

            genericReadBufferTransition.Transition.pResource = dataBuffer;
            commandList->ResourceBarrier(1, &genericReadBufferTransition);

            commandList->Close();

            ID3D12CommandList* cmdsLists[1] = { commandList };
            queue->ExecuteCommandLists(1, cmdsLists);

            currentFenceValue++;
            queue->Signal(fence, currentFenceValue);

            if (fence->GetCompletedValue() != currentFenceValue)
            {
                D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
                WaitForSingleObject(fenceEventHandle, INFINITE);
                ResetEvent(fenceEventHandle);
            }

            D3D_NOT_FAILED(allocator->Reset());
            D3D_NOT_FAILED(commandList->Reset(allocator, pso));

            indexBuffer = dataBuffer;
        }

        // texture heap
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = TextureWidth;
        textureDesc.Height = TextureHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ID3D12Resource* texture = nullptr;
        {
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 totalBytes = 0;
            device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

            uploadBufferDescription.Width = totalBytes;

            ID3D12Resource* dataUploadBuffer = nullptr; // TODO: should be cleared later
            D3D_NOT_FAILED(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&dataUploadBuffer)));

            ID3D12Resource* dataBuffer = nullptr;
            D3D_NOT_FAILED(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&dataBuffer)));

            BYTE* mappedData = nullptr;
            dataUploadBuffer->Map(0, nullptr, (void**)&mappedData);

            for (size_t i = 0; i < numRows; i++)
            {
                memcpy(mappedData + footprint.Footprint.RowPitch * i, Texture + rowSizeInBytes * i, rowSizeInBytes);
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

            commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);

            genericReadBufferTransition.Transition.pResource = dataBuffer;
            commandList->ResourceBarrier(1, &genericReadBufferTransition);
            commandList->Close();

            ID3D12CommandList* cmdsLists[1] = { commandList };
            queue->ExecuteCommandLists(1, cmdsLists);

            currentFenceValue++;
            queue->Signal(fence, currentFenceValue);

            if (fence->GetCompletedValue() != currentFenceValue)
            {
                D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
                WaitForSingleObject(fenceEventHandle, INFINITE);
                ResetEvent(fenceEventHandle);
            }

            D3D_NOT_FAILED(allocator->Reset());
            D3D_NOT_FAILED(commandList->Reset(allocator, pso));

            texture = dataBuffer;
        }

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE secondConstantBufferHandle{ SIZE_T(INT64(constantBufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))) };
        device->CreateShaderResourceView(texture, &srvDesc, secondConstantBufferHandle);

        while (isRunning)
        {
            auto frameStart = chrono::steady_clock::now();

            MSG message;
            if (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    OutputDebugString(L"Quit message reached loop.");
                    isRunning = false;
                }
                else
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }

            Vec forward = cam(camera) * camera.forward;
            Vec left = cam(camera) * camera.left;

            if (KeyDown[UpKey])
            {
                camera.pitch -= 0.1f;
                if (camera.pitch < 0)
                {
                    camera.pitch += static_cast<float>(M_PI) * 2;
                }
            }

            if (KeyDown[DownKey])
            {
                camera.pitch += 0.1f;
                if (camera.pitch > static_cast<float>(M_PI) * 2)
                {
                    camera.pitch -= static_cast<float>(M_PI) * 2;
                }
            }

            if (KeyDown[RightKey])
            {
                camera.yaw -= 0.1f;
                if (camera.yaw < 0)
                {
                    camera.yaw += static_cast<float>(M_PI) * 2;
                }
            }

            if (KeyDown[LeftKey])
            {
                camera.yaw += 0.1f;
                if (camera.yaw > static_cast<float>(M_PI) * 2)
                {
                    camera.yaw -= static_cast<float>(M_PI) * 2;
                }
            }

            if (KeyDown[WKey]) 
            {
                camera.position = camera.position + forward;
            }

            if (KeyDown[AKey])
            {
                camera.position = camera.position + left;
            }

            if (KeyDown[SKey])
            {
                camera.position = camera.position - forward;
            }

            if (KeyDown[DKey])
            {
                camera.position = camera.position - left;
            }

            // calculate light's position in view space
            light.position_view = view(camera) * light.position;

            // upload mvp matrix
            {
                // Build the view matrix.
                Matrix mvp = perspective(static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)) * view(camera);

                Matrix mv = view(camera);

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

                DirectX::XMVECTOR lightPos = { light.position_view.x, light.position_view.y, light.position_view.z, light.position_view.w };

                SetModelViewProjection(uploadBuffer, mvpX, mvX, lightPos);
            }

            ID3D12Resource* currentBuffer = isZeroBufferInTheBack ? zeroBuffer : firstBuffer;
            D3D12_CPU_DESCRIPTOR_HANDLE* currentBufferHandle = isZeroBufferInTheBack ? &zeroBufferHandle : &firstBufferHandle;

            D3D12_VIEWPORT viewport;
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = GameWidth;
            viewport.Height = GameHeight;
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;

            commandList->RSSetViewports(1, &viewport);

            D3D12_RECT scissor;
            scissor.left = 0;
            scissor.top = 0;
            scissor.bottom = GameHeight;
            scissor.right = GameWidth;

            commandList->RSSetScissorRects(1, &scissor);

            D3D12_RESOURCE_BARRIER renderTargetBufferTransition;
            renderTargetBufferTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            renderTargetBufferTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            renderTargetBufferTransition.Transition.pResource = currentBuffer;
            renderTargetBufferTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            renderTargetBufferTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            renderTargetBufferTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &renderTargetBufferTransition);

            FLOAT clearColor[4] = { 0.0f, 0.f, 0.f, 1.000000000f };
            commandList->ClearRenderTargetView(*currentBufferHandle, clearColor, 0, nullptr);
            commandList->ClearDepthStencilView(depthBufferHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

            commandList->OMSetRenderTargets(1, currentBufferHandle, true, &depthBufferHandle);

            ID3D12DescriptorHeap* descriptorHeaps[] = { constantBufferDescriptorHeap };
            commandList->SetDescriptorHeaps(1, descriptorHeaps);
            commandList->SetGraphicsRootSignature(rootSignature);

            D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
            vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
            vertexBufferView.StrideInBytes = sizeof(XVertex);
            vertexBufferView.SizeInBytes = sizeof(XVertex) * static_cast<UINT>(indicesObj.size());
            commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

            D3D12_INDEX_BUFFER_VIEW indexBufferView;
            indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
            indexBufferView.Format = DXGI_FORMAT_R16_UINT;
            indexBufferView.SizeInBytes = sizeof(std::uint16_t) * static_cast<UINT>(indicesObj.size());
            commandList->IASetIndexBuffer(&indexBufferView);

            commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            commandList->SetGraphicsRootDescriptorTable(0, constantBufferDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

            uint32_t indices_offset = 0;
            for (size_t i = 0; i < models.size(); i++)
            {
                const Model& model = models.at(i);
                commandList->DrawIndexedInstanced(model.indices_count, 1, indices_offset, 0, 0);
                indices_offset += model.indices_count;
            }

            D3D12_RESOURCE_BARRIER presentBufferTransition;
            presentBufferTransition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            presentBufferTransition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            presentBufferTransition.Transition.pResource = currentBuffer;
            presentBufferTransition.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            presentBufferTransition.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            presentBufferTransition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &presentBufferTransition);
            commandList->Close();

            ID3D12CommandList* cmdsLists[1] = { commandList };
            queue->ExecuteCommandLists(1, cmdsLists);

            D3D_NOT_FAILED(swapChain->Present(0, 0));
            isZeroBufferInTheBack = !isZeroBufferInTheBack;

            currentFenceValue++;
            queue->Signal(fence, currentFenceValue);

            if (fence->GetCompletedValue() != currentFenceValue)
            {
                D3D_NOT_FAILED(fence->SetEventOnCompletion(currentFenceValue, fenceEventHandle));
                WaitForSingleObject(fenceEventHandle, INFINITE);
                ResetEvent(fenceEventHandle);
            }

            // clean stuff
            D3D_NOT_FAILED(allocator->Reset());
            D3D_NOT_FAILED(commandList->Reset(allocator, pso));

            auto frameActualTime = frameStart - chrono::steady_clock::now();
            auto timeLeft = frameExpectedTime - frameActualTime;

            if (timeLeft > 0ms) 
            {
                this_thread::sleep_for(timeLeft);
            }
        }

        // release hepas and render targets
        CloseHandle(fenceEventHandle);
        swapChain->Release();
        commandList->Release();
        allocator->Release();
        queue->Release();
        device->Release();
        stbi_image_free(Texture);
    }

    return 0;
}