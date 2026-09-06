#pragma once
#include "Engine/Core/CoreTypes.h"
#include "Engine/Render/Common/RenderTypes.h"
#include <wrl/client.h>

class FD3DDevice;

struct TextureData
{
	ID3D11Texture2D* Texture;
	ID3D11ShaderResourceView* ShaderResourceView;
};

struct TextureInfo {
	float TextureWidth;
	float TextureHeight;
	float TexelWidth;    // 1/W, 셰이더 상수로 넘길 때 매번 나눗셈 방지
	float TexelHeight;   // 1/H
	int   MipLevels;     // 밉맵 샘플링 분기
	DXGI_FORMAT Format;  // 알파 유무 판단 → 블렌드 스테이트 자동 설정
};

class FRenderResourceManager
{
public:
	void Create(FD3DDevice* device);
	void Release();
	TextureData GetTexture(int id) { return TextureMap.at(id); }
	int CreateTexture(std::wstring filepath);
	TextureInfo GetTextureInfo(int id);
	int GetTextureSize() { return TextureMap.size(); }	//현재 불러온 텍스쳐 갯수 반환
	int GetDefaultTexture() { return TextureMap.begin()->first; }
	static std::wstring LoadResourceFilePath();
private:
	TMap<int, TextureData> TextureMap; //id, 텍스쳐 맵
	TMap<std::wstring, int> FilePathMap; //file path, texture id
	uint8 nextID = 0;
	FD3DDevice* device;

	static std::wstring GetFileDialoguePath();
};

