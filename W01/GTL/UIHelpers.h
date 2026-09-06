#pragma once

#include <windows.h>
#include "Structs.h"

class URenderer;

FVector GetMouseNDC(HWND hwnd);

extern bool GDebugDrawUIRects;

bool IsPointInRect(
	const FVector& point,
	const FVector& center,
	float halfWidth,
	float halfHeight
);

void DrawDebugRect(
	URenderer& renderer,
	const FVector& center,
	float halfWidth,
	float halfHeight
);