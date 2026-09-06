#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <d3d11.h>

#include "GameTypes.h"
#include "Structs.h"

class URenderer;
class IActor;

void LoadSelectUIAssets(URenderer& renderer);
void ReleaseSelectUIAssets();

bool HandleSelectUIInput(
	HWND hWnd,
	URenderer& renderer,
	//std::vector<IActor*>& actors,
	PlayerSelection& player1Selection,
	PlayerSelection& player2Selection,
	const std::vector<CharacterData>& characterDatabase,
	//EGameState& gameState,
	bool& wasLeftMouseDown // 이게 뭐하는 변수지?
);

void DrawSelectUI(
	URenderer& renderer,
	HWND hWnd,
	const PlayerSelection& player1Selection,
	const PlayerSelection& player2Selection
);

void DrawSelectUIText(
	const PlayerSelection& player1Selection,
	const PlayerSelection& player2Selection,
	const std::vector<CharacterData>& charaterDatabase
);