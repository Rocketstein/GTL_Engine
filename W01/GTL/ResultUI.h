#pragma once

#include <d3d11.h>
#include "GameTypes.h"
#include "Structs.h"

class URenderer;

void LoadResultUIAssets(URenderer& renderer);
void ReleaseResultUIAssets();

void DrawResultOverlay(URenderer& renderer, HWND hWnd, EMatchResult matchResult);
bool HandleResultUIInput(HWND hWnd);