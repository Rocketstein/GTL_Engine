#include "Engine/Fonts/FontCache.h"

FFontCache::FFontCache()
{
}

FFontCache::~FFontCache()
{
}

void FFontCache::Initialize()
{
	int indexX = 0;
	int indexY = 0;

	// ASCII
	for (char i = 33; i <= 126; i++) {
		FCharacterInfo cInfo;
		cInfo.StartU = indexX / FontData.BitmapWidth;
		cInfo.StartV = indexY / FontData.BitmapHeight;
		cInfo.USize = FontData.CellWidth / FontData.BitmapWidth;
		cInfo.VSize = FontData.CellHeight / FontData.BitmapHeight;

		CharInfoMap.emplace(i, cInfo);

		indexX += FontData.CellWidth;
		if (indexX >= FontData.BitmapWidth) {
			indexX = 0;
			indexY += FontData.CellHeight;
		}
	}

	// 한국어
	for (TCHAR i = 0xAC00; i <= 0xD7A3; i++) {
		FCharacterInfo cInfo;
		cInfo.StartU = indexX / FontData.BitmapWidth;
		cInfo.StartV = indexY / FontData.BitmapHeight;
		cInfo.USize = FontData.CellWidth / FontData.BitmapWidth;
		cInfo.VSize = FontData.CellHeight / FontData.BitmapHeight;

		CharInfoMap.emplace(i, cInfo);
		
		indexX += FontData.CellWidth;
		if (indexX >= FontData.BitmapWidth) {
			indexX = 0;
			indexY += FontData.CellHeight;
		}
	}
}

FCharacterInfo FFontCache::GetCharacterAtlasData(TCHAR c)
{
	return CharInfoMap[c];
}

void FFontCache::SetFontData()
{
	// Atlas Meta Data
	// 웹에서 제작해서 하드코딩

	// ASCII + Korean
	FAtlasData data;
	data.BitmapWidth = 2048.f;
	data.BitmapHeight = 2048.f;
	data.CellsPerRow = 128.f;
	data.CellsperColumn = 128.f;
	data.CellWidth = 16.f;
	data.CellHeight = 16.f;

	data.FontSize = 10.f;
	data.OffsetX = 0.f;
	data.OffsetY = 0.f;

	FontData = data;
}
