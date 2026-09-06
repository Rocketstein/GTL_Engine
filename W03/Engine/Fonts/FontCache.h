#pragma once
#include "Engine/Core/CoreTypes.h"

// Atlas Bake된 Font의 UV 정보를 캐싱하는 곳입니다.

struct FCharacterInfo
{
	float StartU, StartV;
	float USize, VSize;
};

struct FAtlasData
{
	float BitmapWidth;
	float BitmapHeight;
	float CellsPerRow;
	float CellsperColumn;
	float CellWidth;
	float CellHeight;

	float FontSize;
	float OffsetX;
	float OffsetY;
};

class FFontCache
{
public:
	FFontCache();
	~FFontCache();

public:
	void SetFontData();
	FAtlasData GetFontData() { return FontData; }
	void Initialize();
	FCharacterInfo GetCharacterAtlasData(TCHAR c);	

private:
	TMap<TCHAR, FCharacterInfo> CharInfoMap;
	FAtlasData FontData;
};