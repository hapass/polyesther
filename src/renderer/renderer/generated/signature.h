#pragma once

struct Constants
{
    DirectX::XMFLOAT4X4 worldViewProj;
    DirectX::XMFLOAT4X4 worldView;
    DirectX::XMFLOAT4 lightPos;
};

struct XVertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT2 TextureCoord;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT3 Color;
    INT TextureIndex;
};