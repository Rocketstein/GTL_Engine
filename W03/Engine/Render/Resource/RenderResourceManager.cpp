#pragma once
#pragma comment( lib, "dxguid.lib")
#include "RenderResourceManager.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include "DirectXTK/WICTextureLoader.h"
#include "Render/Device/D3DDevice.h"
#include <fstream>
#include <filesystem>
#include <iostream>





void FRenderResourceManager::Create(FD3DDevice* device)
{
    this->device = device;
    CreateTexture(L"Assets/Icons/light_icon.png");
}

void FRenderResourceManager::Release()
{
    for (auto& [id, data] : TextureMap)
    {
        if (data.ShaderResourceView) { data.ShaderResourceView->Release(); data.ShaderResourceView = nullptr; }
        if (data.Texture) { data.Texture->Release(); data.Texture = nullptr; }
    }
    TextureMap.clear();
    FilePathMap.clear();
    this->device = nullptr;
}

int FRenderResourceManager::CreateTexture(std::wstring filepath)
{

    //이미 텍스쳐 파일이 존재한다면
    if (FilePathMap.find(filepath) != FilePathMap.end()) {
        int TextureID = FilePathMap.at(filepath);
        return TextureID;
    }

    TextureData data = {};

    ID3D11Resource* RawTexture = nullptr;

    device->GetDeviceContext()->Flush();

    HRESULT Hr = CreateWICTextureFromFile(
        device->GetDevice(),
        device->GetDeviceContext(),
        filepath.c_str(),
        &RawTexture,           // nullptr 말고 받아야 Texture2D 꺼낼 수 있음
        &data.ShaderResourceView
    );
    if (FAILED(Hr))
    {
        // 로드 실패 처리
        return -1;
    }

    // ID3D11Resource → ID3D11Texture2D 로 캐스팅
    RawTexture->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&data.Texture);
    RawTexture->Release(); // QueryInterface가 레퍼런스 카운트 올리므로 원본 해제

    int NewId = static_cast<int>(TextureMap.size());
    TextureMap.emplace(NewId, data);
    FilePathMap.emplace(filepath, NewId);

    return NewId;
}

TextureInfo FRenderResourceManager::GetTextureInfo(int id)
{
    TextureInfo info = {};
    D3D11_TEXTURE2D_DESC desc;
    TextureMap.at(id).Texture->GetDesc(&desc);

    info.TextureWidth = (float)desc.Width;
    info.TextureHeight = (float)desc.Height;
    info.TexelWidth = 1.f / desc.Width;
    info.TexelHeight = 1.f / desc.Height;
    info.MipLevels = desc.MipLevels;
    info.Format = desc.Format;

    return info;
}

std::wstring FRenderResourceManager::LoadResourceFilePath()
{
    std::wstring FilePath = GetFileDialoguePath();
    if (FilePath.empty()) { return L""; }
    std::ifstream File(FilePath);
    if (!File.is_open()) {
        // Failed to open file at target destination
        std::cerr << "Failed to open file at target destination" << std::endl;
        return nullptr;
    }

    return FilePath;
}

std::wstring FRenderResourceManager::GetFileDialoguePath()
{
    std::wstring FileDestination;
    WCHAR szFile[MAX_PATH] = L"";

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"Image Files (*.png;*.jpg;*.jpeg;*.PNG;)\0*.png;*.jpg;*.jpeg;*.PNG\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"png";
    ofn.lpstrTitle = L"Load Image";
    ofn.lpstrInitialDir = L".";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileName(&ofn)) {
        return L"";
    }

    FileDestination = szFile;

    const size_t DotPos = FileDestination.rfind('.');
    if (DotPos == std::string::npos) {
        std::cerr << "Invalid file: no extension found" << std::endl;
        return L"";
    }

    const std::wstring Extension = FileDestination.substr(DotPos);

    const std::vector<std::wstring> ValidExtensions = { L".png", L".jpg", L".jpeg", L".PNG"};
    const bool bValidExtension = std::any_of(ValidExtensions.begin(), ValidExtensions.end(),
        [&Extension](const std::wstring& Ext) { return Extension == Ext; });

    if (!bValidExtension) {
        std::cerr << "Invalid file type: got " << Extension.c_str() << std::endl;
        return L"";
    }
    
    return FileDestination;
}
