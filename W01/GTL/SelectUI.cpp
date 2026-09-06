#include "SelectUI.h"
#include "UIHelpers.h"

#include "URenderer.h"
#include "IActor.h"
#include "ImGui/imgui.h"
#include <WICTextureLoader.h>

using namespace DirectX;

namespace
{
	ID3D11ShaderResourceView* gSelectPanelSprite = nullptr;
	ID3D11ShaderResourceView* gArrowLeftSprite = nullptr; // 이거 p1의 arrow랑 p2 arrow 따로 있어서 2개 더 만들어야 할 걸
	ID3D11ShaderResourceView* gArrowRightSprite = nullptr;
	ID3D11ShaderResourceView* gMatchStartSprite = nullptr;
	ID3D11ShaderResourceView* gVsSprite = nullptr;

	ID3D11ShaderResourceView* gP1Sprite = nullptr;
	ID3D11ShaderResourceView* gP2Sprite = nullptr;
	ID3D11ShaderResourceView* gP3Sprite = nullptr;
	ID3D11ShaderResourceView* gP4Sprite = nullptr;
	ID3D11ShaderResourceView* gP5Sprite = nullptr;

	/*FVector GetMouseNDC(HWND hWnd)
	{
		POINT p;
		GetCursorPos(&p);
		ScreenToClient(hWnd, &p);

		RECT rc;
		GetClientRect(hWnd, &rc);

		float width = (float)(rc.right - rc.left);
		float height = (float)(rc.bottom - rc.top);

		float x = (p.x / width) * 2.0f - 1.0f;
		float y = -((p.y / height) * 2.0f - 1.0f);

		return FVector(x, y, 0.0f);
	}*/

	/*bool IsPointInRect(const FVector& point, const FVector& center, float halfWidth, float halfHeight)
	{
		return
			point.x > center.x - halfWidth &&
			point.x < center.x + halfWidth &&
			point.y > center.y - halfHeight &&
			point.y < center.y + halfHeight;
	}*/

	ID3D11ShaderResourceView* GetPortraitByIndex(int index)
	{
		switch (index)
		{
		case 0: return gP1Sprite;
		case 1: return gP2Sprite;
		case 2: return gP3Sprite;
		case 3: return gP4Sprite;
		case 4: return gP5Sprite;
		default: return nullptr;
		}
	}
}

void LoadSelectUIAssets(URenderer& renderer)
{
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\select_panel.png", nullptr, &gSelectPanelSprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\button_arrow_left.png", nullptr, &gArrowLeftSprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\button_arrow_right.png", nullptr, &gArrowRightSprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\button_start1.png", nullptr, &gMatchStartSprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\vs1.png", nullptr, &gVsSprite);

	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\player_classic.png", nullptr, &gP1Sprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\p1.jpg", nullptr, &gP2Sprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\p2.jpg", nullptr, &gP3Sprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\p3.jpg", nullptr, &gP4Sprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\p4.jpg", nullptr, &gP5Sprite);
}

void ReleaseSelectUIAssets()
{
	auto ReleaseSRV = [](ID3D11ShaderResourceView*& srv)
		{
			if (srv)
			{
				srv->Release();
				srv = nullptr;
			}
		};

	ReleaseSRV(gSelectPanelSprite);
	ReleaseSRV(gArrowLeftSprite);
	ReleaseSRV(gArrowRightSprite);
	ReleaseSRV(gMatchStartSprite);
	ReleaseSRV(gVsSprite);

	ReleaseSRV(gP1Sprite);
	ReleaseSRV(gP2Sprite);
	ReleaseSRV(gP3Sprite);
	ReleaseSRV(gP4Sprite);
}

bool HandleSelectUIInput(
	HWND hwnd,
	URenderer& renderer,
	//std::vector<IActor*>& actors,
	PlayerSelection& player1Selection,
	PlayerSelection& player2Selection,
	const std::vector<CharacterData>& characterDatabase,
	//EGameState& gameState,
	bool& wasLeftMouseDown
)
{
	//FVector mouse = GetMouseNDC(hwnd);
	//bool isLeftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	//FVector p1LeftPos(-0.52f, 0.10f, 0.0f);
	//FVector p1RightPos(-0.24f, 0.10f, 0.0f);
	//FVector p2LeftPos(0.24f, 0.10f, 0.0f);
	//FVector p2RightPos(0.52f, 0.10f, 0.0f);
	//FVector startPos(0.0f, -0.34f, 0.0f);

	//float arrowHalfW = 0.04f;
	//float arrowHalfH = 0.05f;

	//float startHalfW = 0.22f;
	//float startHalfH = 0.08f;

	//if (isLeftMouseDown && !wasLeftMouseDown)
	//{
	//	if (IsPointInRect(mouse, p1LeftPos, arrowHalfW, arrowHalfH))
	//	{
	//		player1Selection.CharacterIndex =
	//			(player1Selection.CharacterIndex - 1 + (int)characterDatabase.size()) % (int)characterDatabase.size();
	//	}
	//	else if (IsPointInRect(mouse, p1RightPos, arrowHalfW, arrowHalfH))
	//	{
	//		player1Selection.CharacterIndex =
	//			(player1Selection.CharacterIndex + 1) % (int)characterDatabase.size();
	//	}
	//	else if (IsPointInRect(mouse, p2LeftPos, arrowHalfW, arrowHalfH))
	//	{
	//		player2Selection.CharacterIndex =
	//			(player2Selection.CharacterIndex - 1 + (int)characterDatabase.size()) % (int)characterDatabase.size();
	//	}
	//	else if (IsPointInRect(mouse, p2RightPos, arrowHalfW, arrowHalfH))
	//	{
	//		player2Selection.CharacterIndex =
	//			(player2Selection.CharacterIndex + 1) % (int)characterDatabase.size();
	//	}
	//	else if (IsPointInRect(mouse, startPos, startHalfW, startHalfH))
	//	{
	//		// main.cpp에서 StartMatch를 호출하도록 상태만 바꾸는 방식으로 두는 것도 가능
	//		// 지금은 gameState만 바꾸지 않고 main에서 처리하도록 유지해도 됨.
	//	}
	//}

	FVector mouse = GetMouseNDC(hwnd);
	bool isLeftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	FVector p1LeftPos(-0.52f, 0.10f, 0.0f);
	FVector p1RightPos(-0.24f, 0.10f, 0.0f);
	FVector p2LeftPos(0.20f, 0.10f, 0.0f);
	FVector p2RightPos(0.48f, 0.10f, 0.0f);
	FVector startPos(0.0f, -0.40f, 0.0f);

	float arrowHitHalfW = 0.065f;
	float arrowHitHalfH = 0.045f;

	float startHitHalfW = 0.14f;
	float startHitHalfH = 0.06f;

	bool startClicked = false;

	if (isLeftMouseDown && !wasLeftMouseDown)
	{
		if (IsPointInRect(mouse, p1LeftPos, arrowHitHalfW, arrowHitHalfH))
		{
			player1Selection.CharacterIndex =
				(player1Selection.CharacterIndex - 1 + (int)characterDatabase.size()) % (int)characterDatabase.size();
		}
		else if (IsPointInRect(mouse, p1RightPos, arrowHitHalfW, arrowHitHalfH))
		{
			player1Selection.CharacterIndex =
				(player1Selection.CharacterIndex + 1) % (int)characterDatabase.size();
		}
		else if (IsPointInRect(mouse, p2LeftPos, arrowHitHalfW, arrowHitHalfH))
		{
			player2Selection.CharacterIndex =
				(player2Selection.CharacterIndex - 1 + (int)characterDatabase.size()) % (int)characterDatabase.size();
		}
		else if (IsPointInRect(mouse, p2RightPos, arrowHitHalfW, arrowHitHalfH))
		{
			player2Selection.CharacterIndex =
				(player2Selection.CharacterIndex + 1) % (int)characterDatabase.size();
		}
		else if (IsPointInRect(mouse, startPos, startHitHalfW, startHitHalfH))
		{
			startClicked = true;
		}
	}

	wasLeftMouseDown = isLeftMouseDown;
	return startClicked;
}

void DrawSelectUI(
	URenderer& renderer,
	HWND hwnd,
	const PlayerSelection& player1Selection,
	const PlayerSelection& player2Selection
)
{


	(void)hwnd;

	renderer.DrawRectSprite(
		gSelectPanelSprite,
		FVector(0.0f, 0.05f, 0.0f),
		0.0f,
		1.20f,
		1.40f,
		false
	);

	if (gVsSprite)
	{
		renderer.DrawRectSprite(
			gVsSprite,
			FVector(0.0f, 0.08f, 0.0f),
			0.0f,
			0.18f,
			0.20f,
			false
		);
	}

	ID3D11ShaderResourceView* leftPortrait = GetPortraitByIndex(player1Selection.CharacterIndex);
	ID3D11ShaderResourceView* rightPortrait = GetPortraitByIndex(player2Selection.CharacterIndex);

	if (leftPortrait)
	{
		renderer.DrawRectSprite(
			leftPortrait,
			FVector(-0.38f, 0.10f, 0.0f),
			0.0f,
			0.16f,
			0.20f,
			false
		);
	}

	if (rightPortrait)
	{
		renderer.DrawRectSprite(
			rightPortrait,
			FVector(0.34f, 0.10f, 0.0f),
			0.0f,
			0.16f,
			0.20f,
			false
		);
	}

	float arrowImageWidth = 458.0f;
	float arrowImageHeight = 671.0f;
	float arrowAspect = arrowImageWidth / arrowImageHeight;

	float arrowHalfHeight = 0.05f;
	float arrowHalfWidth = arrowHalfHeight * arrowAspect;

	if (gArrowLeftSprite)
		renderer.DrawRectSprite(gArrowLeftSprite, FVector(-0.52f, 0.10f, 0.0f), 0.0f, arrowHalfWidth, arrowHalfHeight, false);
	if (gArrowRightSprite)
		renderer.DrawRectSprite(gArrowRightSprite, FVector(-0.24f, 0.10f, 0.0f), 0.0f, arrowHalfWidth, arrowHalfHeight, false);
	if (gArrowLeftSprite)
		renderer.DrawRectSprite(gArrowLeftSprite, FVector(0.20f, 0.10f, 0.0f), 0.0f, arrowHalfWidth, arrowHalfHeight, false);
	if (gArrowRightSprite)
		renderer.DrawRectSprite(gArrowRightSprite, FVector(0.48f, 0.10f, 0.0f), 0.0f, arrowHalfWidth, arrowHalfHeight, false);

	float matchStartImageWidth = 1146.0f;
	float matchStartImageHeight = 501.0f;
	float matchStartAspect = matchStartImageWidth / matchStartImageHeight;

	float matchStartHalfHeight = 0.06f;
	float matchStartHalfWidth = matchStartHalfHeight * matchStartAspect;

	if (gMatchStartSprite)
	{
		renderer.DrawRectSprite(
			gMatchStartSprite,
			FVector(0.0f, -0.40f, 0.0f),
			0.0f,
			matchStartHalfWidth,
			matchStartHalfHeight,
			false
		);
	}
	DrawDebugRect(renderer, FVector(-0.52f, 0.10f, 0.0f), 0.04f, 0.05f);
	DrawDebugRect(renderer, FVector(-0.24f, 0.10f, 0.0f), 0.04f, 0.05f);
	DrawDebugRect(renderer, FVector(0.20f, 0.10f, 0.0f), 0.04f, 0.05f);
	DrawDebugRect(renderer, FVector(0.48f, 0.10f, 0.0f), 0.04f, 0.05f);

	DrawDebugRect(renderer, FVector(0.0f, -0.40f, 0.0f), 0.14f, 0.06f);
}

void DrawSelectUIText(
	const PlayerSelection& player1Selection,
	const PlayerSelection& player2Selection,
	const std::vector<CharacterData>& characterDatabase
)
{
	ImVec2 displaySize = ImGui::GetIO().DisplaySize;

	if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
		return;

	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::Begin("SelectText", nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoInputs);

	ImGui::SetWindowPos(ImVec2(0, 0));
	ImGui::SetWindowSize(displaySize);

	const float baseWidth = 1280.0f;
	const float baseHeight = 720.0f;

	float scaleX = displaySize.x / baseWidth;
	float scaleY = displaySize.y / baseHeight;
	float uiScale = (scaleX < scaleY) ? scaleX : scaleY;

	if (uiScale <= 0.0f)
		uiScale = 0.01f;

	float fontScale = 2.4f * uiScale;
	if (fontScale <= 0.0f)
		fontScale = 0.01f;

	ImGui::SetWindowFontScale(fontScale);

	const char* leftName = characterDatabase[player1Selection.CharacterIndex].Name.c_str();
	const char* rightName = characterDatabase[player2Selection.CharacterIndex].Name.c_str();

	ImVec2 leftSize = ImGui::CalcTextSize(leftName);
	ImVec2 rightSize = ImGui::CalcTextSize(rightName);

	const float leftCenterRatio = 410.0f / 1280.0f;
	const float rightCenterRatio = 870.0f / 1280.0f;
	const float textYRatio = 415.0f / 720.0f;

	float leftCenterX = displaySize.x * leftCenterRatio;
	float rightCenterX = displaySize.x * rightCenterRatio;
	float textY = displaySize.y * textYRatio;

	ImGui::SetCursorPos(ImVec2(leftCenterX - leftSize.x * 0.5f, textY));
	ImGui::Text("%s", leftName);

	ImGui::SetCursorPos(ImVec2(rightCenterX - rightSize.x * 0.5f, textY));
	ImGui::Text("%s", rightName);

	ImGui::End();
}