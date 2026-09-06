#include <windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler.lib")

#include <vector>

#include "Constants.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "Structs.h"
#include "UPrimitive.h"
#include "UBall.h"
#include "IActor.h"
#include "InputSystem.h"
#include "Sphere.h"
#include "Player.h"
#include "GameManager.h"
#include "Physics.h"
#include "Time.h"
#include "Ball.h"
#include "PlayerController.h"
#include "ConsoleHelper.h"
#include "AudioManager.h"
#include "AsyncTaskManager.h"
#include "Particles.h"
#include "GoalTrigger.h"

#include "GameTypes.h"
#include "SelectUI.h"
#include "UIHelpers.h"
#include "ResultUI.h"
#include "SkillGauge.h"

#include <WICTextureLoader.h>
using namespace DirectX;

EGameState gGameState = EGameState::Ready;
bool gIsWindowResizing = false;
bool gResetDeltaTime = false;

std::vector<CharacterData> gCharacterDatabase =
{
	{ 0, "Classic", L"pClassic.png"},
	{ 1, "Taehyun Kim", L"p1.jpg"},
	{ 2, "Hyungjun Kim", L"p2.jpg"},
	{ 3, "Chanil Chong", L"p3.jpg"},
	{ 4, "Hyobeom Kim", L"p4.jpg"}
};

URenderer renderer;

PlayerSelection gPlayer1Selection;
PlayerSelection gPlayer2Selection;

ID3D11ShaderResourceView* gBackgroundSprite = nullptr;
ID3D11ShaderResourceView* gLeftGoalSprite = nullptr;
ID3D11ShaderResourceView* gRightGoalSprite = nullptr;
ID3D11ShaderResourceView* gPlayButtonSprite = nullptr;
ID3D11ShaderResourceView* gExitButtonSprite = nullptr;
ID3D11ShaderResourceView* gTitleSprite = nullptr;

bool gWasLeftMouseDown = false;
bool gWasLeftMouseDown_Ready = false;

EMatchResult gMatchResult = EMatchResult::None;

void EndMatch(std::vector<IActor*>& actors, std::vector<SkillGauge*>& gauges)
{
	for (IActor* actor : actors)
	{
		delete actor;
	}
	actors.clear();

	for (SkillGauge* gauge : gauges)
	{
		delete gauge;
	}
	gauges.clear();

	gMatchResult = EMatchResult::None;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_ENTERSIZEMOVE:
		gIsWindowResizing = true;
		return 0;
	case WM_EXITSIZEMOVE:
		gIsWindowResizing = false;
		gResetDeltaTime = true;
		return 0;
	case WM_SIZING:
	{
		// lParam = 현재 변경 중인 윈도우의 RECT 구조체 포인터
		LPRECT rect = (LPRECT)lParam;


		RECT frameRect;
		AdjustWindowRectEx(&frameRect, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW, false, 0);

		// 프레임이 차지하는 너비와 높이 (보통 양수값)
		int frameWidth = (frameRect.right - frameRect.left);
		int frameHeight = (frameRect.bottom - frameRect.top);

		// 2. 현재 윈도우 전체 크기에서 프레임을 빼서 클라이언트 영역 크기 추출
		int currentClientWidth = (rect->right - rect->left) - frameWidth;
		int currentClientHeight = (rect->bottom - rect->top) - frameHeight;

		int width = rect->right - rect->left;
		int height = rect->bottom - rect->top;
		const float targetRatio = 16.0f / 9.0f;

		// wParam = 드래그하는 방향
		switch (wParam) {
		case WMSZ_LEFT:
		case WMSZ_RIGHT:
		case WMSZ_TOPLEFT:
		case WMSZ_TOPRIGHT:
		case WMSZ_BOTTOMLEFT:
		case WMSZ_BOTTOMRIGHT:
			// 가로 조절 시 세로를 맞춤: ClientHeight = ClientWidth / Ratio
			rect->bottom = rect->top + frameHeight + (int)(currentClientWidth / targetRatio);
			break;

		case WMSZ_TOP:
		case WMSZ_BOTTOM:
			// 세로 조절 시 가로를 맞춤: ClientWidth = ClientHeight * Ratio
			rect->right = rect->left + frameWidth + (int)(currentClientHeight * targetRatio);
			break;
		}
		return TRUE;
	}
	case WM_SIZE:
		if (renderer.Device != NULL && wParam != SIZE_MINIMIZED)
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);

			// 1. 기존 렌더타겟 뷰 해제 (이걸 안 하면 ResizeBuffers에서 에러남)
			if (renderer.FrameBufferRTV) 
			{ 
				renderer.ReleaseFrameBuffer();
			}

			HRESULT hr = renderer.SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
			if (FAILED(hr))
			{
				// 리사이즈 실패 예외 처리
				return 0;
			}
			renderer.UpdateViewportInfo();

			renderer.CreateFrameBuffer();
		}
		return 0;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

void HandleMessages(bool &bIsExit)
{
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_QUIT)
		{
			bIsExit = true;
			break;
		}
		else if (msg.message == WM_KEYDOWN)
		{
			if (msg.wParam == VK_OEM_PLUS || msg.wParam == VK_ADD)
			{
				// ++TargetBallCount;
			}
			if (msg.wParam == VK_OEM_MINUS || msg.wParam == VK_SUBTRACT)
			{
				// if (TargetBallCount) --TargetBallCount;
			}
		}
	}
}

std::unique_ptr<GameController> p1Controller;
std::unique_ptr<GameController> p2Controller;

namespace
{
	bool GDebugDrawColliders = false;
}

void DrawDebugCircle(
	URenderer* renderer,
	ID3D11Buffer* vertexBufferSphere,
	UINT numVerticesSphere,
	const FVector& center,
	float radius)
{
	if (!renderer || !vertexBufferSphere || radius <= 0.0f)
		return;

	const FVector4 posRadius(center.x, center.y, 0.0f, radius);
	const float angle = 0.0f;

	renderer->UpdateConstant(posRadius, angle);
	renderer->RenderPrimitive(vertexBufferSphere, numVerticesSphere);
}

void DrawDebugAABB(
	URenderer* renderer,
	const FVector& center,
	float halfWidth,
	float halfHeight)
{
	if (!renderer || halfWidth <= 0.0f || halfHeight <= 0.0f)
		return;

	DrawDebugRect(*renderer, center, halfWidth, halfHeight);
}

void DrawDebugColliders(
	URenderer* renderer,
	std::vector<IActor*>& actors,
	ID3D11Buffer* vertexBufferSphere,
	UINT numVerticesSphere)
{
	if (!renderer || !vertexBufferSphere)
		return;

	renderer->PrepareShader();

	for (IActor* actor : actors)
	{
		if (!actor)
			continue;

		const int colliderCount = actor->GetColliderPartCount();
		for (int i = 0; i < colliderCount; ++i)
		{
			const FColliderPart& part = actor->GetColliderPart(i);
			if (!part.bEnabled)
				continue;

			const FVector center = actor->GetColliderWorldCenter(i);

			if (part.Shape == ECollisionShape::Circle)
			{
				DrawDebugCircle(
					renderer,
					vertexBufferSphere,
					numVerticesSphere,
					center,
					part.Radius);
			}
			else if (part.Shape == ECollisionShape::AABB)
			{
				DrawDebugAABB(
					renderer,
					center,
					part.HalfExtent.x,
					part.HalfExtent.y
				);
			}
		}
	}
}

void InitGame(
	URenderer& renderer,
	std::vector<IActor*>& actors,
	const CharacterData* selectedCharacterP1,
	const CharacterData* selectedCharacterP2
)
{
	ID3D11ShaderResourceView* player1Tex = nullptr;
	ID3D11ShaderResourceView* player2Tex = nullptr;
	ID3D11ShaderResourceView* ballTex = nullptr;
	ID3D11ShaderResourceView* skillAuraSprite = nullptr;
	ID3D11ShaderResourceView* shoeSpriteLeft = nullptr;
	ID3D11ShaderResourceView* shoeSpriteRight = nullptr;

	// Load character sprites based on selected characters
	if (selectedCharacterP1)
	{
		std::wstring path = L"Assets\\left\\" + selectedCharacterP1->SpritePath;
		CreateWICTextureFromFile(
			renderer.Device,
			renderer.DeviceContext,
			path.c_str(),
			nullptr,
			&player1Tex
		);

		CreateWICTextureFromFile(
			renderer.Device,
			renderer.DeviceContext,
			L"Assets\\left\\shoes1.png",
			nullptr,
			&shoeSpriteLeft
		);
	}

	if (selectedCharacterP2)
	{
		std::wstring path = L"Assets\\right\\" + selectedCharacterP2->SpritePath;
		CreateWICTextureFromFile(
			renderer.Device,
			renderer.DeviceContext,
			path.c_str(),
			nullptr,
			&player2Tex
		);

		CreateWICTextureFromFile(
			renderer.Device,
			renderer.DeviceContext,
			L"Assets\\right\\shoes1.png",
			nullptr,
			&shoeSpriteRight
		);
	}

	CreateWICTextureFromFile(
		renderer.Device,
		renderer.DeviceContext,
		L"Assets\\ball.png",
		nullptr,
		&ballTex
	);
	CreateWICTextureFromFile(
		renderer.Device,
		renderer.DeviceContext,
		L"Assets\\skill_ready.png",
		nullptr,
		&skillAuraSprite
	);

	Player* player1 = new Player(1);
	player1->SetPosition(FVector(GM_defaults::leftPlayerX, GM_defaults::leftPlayerY, 0.0f));
	player1->SetScale(0.18f);
	player1->SetCharacterData(selectedCharacterP1);
	player1->SetBodySprite(player1Tex);
	player1->SetFeetSprites(player1Tex);
	player1->SetAuraSprite(skillAuraSprite);
	player1->SetFeetSprites(shoeSpriteLeft);
	actors.push_back(player1);

	p1Controller = std::make_unique<PlayerController>();
	dynamic_cast<PlayerController*>(p1Controller.get())->SetInputMap(1);
	p1Controller->SetControllTarget(player1);

	if (selectedCharacterP2)
	{
		Player* player2 = new Player(2);
		player2->SetPosition(FVector(GM_defaults::rightPlayerX, GM_defaults::rightPlayerY, 0.0f));
		player2->SetScale(0.18f);
		player2->SetCharacterData(selectedCharacterP2);
		player2->SetBodySprite(player2Tex);
		player2->SetFeetSprites(player2Tex);
		player2->SetAuraSprite(skillAuraSprite);
		player2->SetFeetSprites(shoeSpriteRight);

		actors.push_back(player2);

		// TODO: AI 지원 추가했을 경우 아래의 주석 해제
		//if (2p mode)
		//{
			p2Controller = std::make_unique<PlayerController>();
			dynamic_cast<PlayerController*>(p2Controller.get())->SetInputMap(2);
			p2Controller->SetControllTarget(player2);
		//}
		//else
		//{
		//	p2Controller = std::make_unique<AiController>();
		//	p2Controller->SetControllTarget(player2);
		//}

			player2->SetSkill(new SkillA());
	}

	Ball* ball = new Ball();
	ball->SetSprite(ballTex);
	actors.push_back(ball);

	player1->SetSkill(new SkillA());

	IActor* UpperGoalBorder = new IActor();
	UpperGoalBorder->SetApplyGravity(false);
	UpperGoalBorder->SetAngularVelocity(0.0f);
	UpperGoalBorder->SetBodyType(EBodyType::Static);
	UpperGoalBorder->SetCollisionMode(ECollisionMode::Block);
	UpperGoalBorder->SetCollisionShape(ECollisionShape::AABB);
	UpperGoalBorder->SetPosition(FVector(0.0f, -0.08f, 0.0f));

	UpperGoalBorder->AddAABBColliderPart(
		FVector(0.25f, 0.01f, 0.0f), 
		FVector(-1.0f, 0.0f, 0.0f),
		ECollisionMode::Block
	);
	UpperGoalBorder->AddAABBColliderPart(
		FVector(0.25f, 0.01f, 0.0f),
		FVector(1.0f, 0.0f, 0.0f),
		ECollisionMode::Block
	);

	actors.push_back(UpperGoalBorder);

	GoalTrigger* LeftUpperGoalBlock = new GoalTrigger();
	LeftUpperGoalBlock->SetApplyGravity(false);
	LeftUpperGoalBlock->SetAngularVelocity(0.0f);
	LeftUpperGoalBlock->SetBodyType(EBodyType::Static);
	LeftUpperGoalBlock->SetCollisionMode(ECollisionMode::Trigger);
	LeftUpperGoalBlock->SetCollisionShape(ECollisionShape::AABB);
	LeftUpperGoalBlock->SetPosition(FVector(0.0f, -0.05f, 0.0f));
	LeftUpperGoalBlock->AddAABBColliderPart(
		FVector(0.24f, 0.01f, 0.0f),
		FVector(-1.0f, 0.0f, 0.0f),
		ECollisionMode::Trigger
	);
	LeftUpperGoalBlock->SetPushDirection(1.0f);
	LeftUpperGoalBlock->SetPushPower(0.03f);

	actors.push_back(LeftUpperGoalBlock);

	GoalTrigger* RightUpperGoalBlock = new GoalTrigger();
	RightUpperGoalBlock->SetApplyGravity(false);
	RightUpperGoalBlock->SetAngularVelocity(0.0f);
	RightUpperGoalBlock->SetBodyType(EBodyType::Static);
	RightUpperGoalBlock->SetCollisionMode(ECollisionMode::Trigger);
	RightUpperGoalBlock->SetCollisionShape(ECollisionShape::AABB);
	RightUpperGoalBlock->SetPosition(FVector(0.0f, -0.05f, 0.0f));
	RightUpperGoalBlock->AddAABBColliderPart(
		FVector(0.24f, 0.01f, 0.0f),
		FVector(1.0f, 0.0f, 0.0f),
		ECollisionMode::Trigger
	);
	RightUpperGoalBlock->SetPushDirection(-1.0f);
	RightUpperGoalBlock->SetPushPower(0.03f);

	actors.push_back(RightUpperGoalBlock);
}

void Update(std::vector<IActor*>& actors, std::vector<SkillGauge*>& gauges)
{
	InputSystem::Update();

	if (p1Controller)
		p1Controller->Update();
	if (p2Controller)
		p2Controller->Update();
	
	for (IActor* actor : actors)
	{
		actor->Update();

		Player* player = dynamic_cast<Player*>(actor);
		if (player)
		{
			player->UpdateSkill(Time::DeltaTime);
		}
	}

	for (SkillGauge* gauge : gauges)
	{
		gauge->Update(gauge->linkedPlayer->GetSkillGauge());
	}
}

void StartMatch(URenderer& renderer, std::vector<IActor*>& actors, std::vector<SkillGauge*>& gauges, GameManager& gameManager)
{
	for (IActor* actor : actors)
	{
		delete actor;
	}
	actors.clear();

	const CharacterData& char1 = gCharacterDatabase[gPlayer1Selection.CharacterIndex];
	const CharacterData& char2 = gCharacterDatabase[gPlayer2Selection.CharacterIndex];

	InitGame(renderer, actors, &char1, &char2);

	// Create gauges and link to players
	SkillGauge::LinkPlayer(actors, gauges, &renderer);

	gameManager.resetMatch();
	gMatchResult = EMatchResult::None;
}

void DrawActors(URenderer* renderer, std::vector<IActor*>& actors)
{
	for (IActor* actor : actors)
	{
		actor->Render(renderer);
	}
}

void DrawReadyUI(URenderer& renderer, HWND hwnd)
{
	FVector playButtonPos(0.0f, -0.3f, 0.0f);
	FVector exitButtonPos(0.0f, -0.5f, 0.0f);
	FVector TitlePos(0.0f, 0.2f, 0.0f);

	float buttonImageWidth = 1146.0f;
	float buttonImageHeight = 501.0f;
	float buttonAspect = buttonImageWidth / buttonImageHeight;

	float renderHalfHeight = 0.06f;
	float renderHalfWidth = renderHalfHeight * buttonAspect;

	// 버튼 렌더
	renderer.DrawRectSprite(
		gPlayButtonSprite,
		playButtonPos,
		0.0f,
		renderHalfWidth,
		renderHalfHeight,
		false
	);

	renderer.DrawRectSprite(
		gExitButtonSprite,
		exitButtonPos,
		0.0f,
		renderHalfWidth,
		renderHalfHeight,
		false
	);

	// Title
	float titleImageWidth = 1365.0f;
	float titleImageHeight = 768.0f;
	float titleAspect = titleImageWidth / titleImageHeight;

	float renderTitleHalfHeight = 0.35f;
	float renderTitleHalfWidth = renderTitleHalfHeight * titleAspect;

	renderer.DrawRectSprite(
		gTitleSprite,
		TitlePos,
		0.0f,
		renderTitleHalfHeight,
		renderTitleHalfWidth,
		false
	);

	float hitHalfWidth = 0.14f;
	float hitHalfHeight = 0.06f;

	// 마우스 위치
	FVector mouse = GetMouseNDC(hwnd);
	bool isLeftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	if (!isLeftMouseDown)
	{
		gWasLeftMouseDown_Ready = false;
	}

	if (isLeftMouseDown && !gWasLeftMouseDown_Ready)
	{
		if (IsPointInRect(mouse, playButtonPos, hitHalfWidth, hitHalfHeight))
		{
			gGameState = EGameState::Select;
			gWasLeftMouseDown_Ready = true;
			gWasLeftMouseDown = true;
		}
		else if (IsPointInRect(mouse, exitButtonPos, hitHalfWidth, hitHalfHeight))
		{
			gWasLeftMouseDown_Ready = true;
			PostQuitMessage(0);
		}
	}

	DrawDebugRect(renderer, playButtonPos, hitHalfWidth, hitHalfHeight);
	DrawDebugRect(renderer, exitButtonPos, hitHalfWidth, hitHalfHeight);
}

void DrawTextNDC(const char* text, ImVec2 ndcPos, float ndcSize, ImU32 color, ETextAlign align = Center)
{
	if (ImGui::GetCurrentContext() == nullptr || ImGui::GetFont() == nullptr)
		return;

	ImGuiIO& io = ImGui::GetIO();
	ImVec2 displaySize = io.DisplaySize;
	if (displaySize.x <= 0.0f) return;

	// 1. 위치 변환 (NDC -> Screen)
	float screenX = (ndcPos.x + 1.0f) * (displaySize.x * 0.5f);
	float screenY = (1.0f - ndcPos.y) * (displaySize.y * 0.5f);

	// 2. 폰트 크기 계산 (NDC 가로 폭 기준)
	float fontSize = (ndcSize / 2.0f) * displaySize.x;

	// 3. 정렬을 위한 텍스트 크기 계산
	// 현재 설정된 폰트와 fontSize를 기준으로 텍스트의 실제 픽셀 너비를 구함
	ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);

	// 4. 정렬에 따른 X 좌표 보정
	if (align == Center)
	{
		screenX -= (textSize.x * 0.5f);
	}
	else if (align == Right)
	{
		screenX -= textSize.x;
	}

	// 5. 출력
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(screenX, screenY), color, text);
}

void RenderInGameUI(GameManager* gameManager)
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	// 배경 투명, 타이틀바 없음, 입력 무시 설정으로 '투명 캔버스' 생성
	ImGui::Begin("ScoreOverlay", nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoSavedSettings);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	char scoreBuf[10];
	sprintf_s(scoreBuf, "%0d:%0d", gameManager->GetScore(1), gameManager->GetScore(2));
	// NDC 기준 (0, 0.8) 위치에 점수 표시 (화면 중상 상단)
	DrawTextNDC(scoreBuf, ImVec2(0.0f, 0.9f), 0.1f, IM_COL32(255, 0, 0, 255));

	char timeBuf[10];
	float leftTime = gameManager->GetTimeLeft();
	int min = floor(leftTime / 60);
	int sec = floor(leftTime - 60 * min);
	sprintf_s(timeBuf, "%0d:%02d", min, sec);
	DrawTextNDC(timeBuf, ImVec2(0.0f, 0.95f), 0.05f, IM_COL32(0, 0, 0, 255));

	ImGui::End();
}

void DrawImGui(URenderer& renderer, std::vector<IActor*>& actors, std::vector<SkillGauge*>& gauges, GameManager& gameManager)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (gGameState == EGameState::Select)
	{
		DrawSelectUIText(gPlayer1Selection, gPlayer2Selection, gCharacterDatabase);
	}

	if (gGameState == EGameState::Game || gGameState == EGameState::Result)
	{
		RenderInGameUI(&gameManager);
	}
	
	//ImGui::End();
	
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UpdateParticleCollections(std::vector<ParticleCollection>& collections, float dt)
{
	for (int i = collections.size() - 1; i >= 0; i--)
	{
		collections[i].updateParticles(dt);
		if (collections[i].expired())
		{
			collections.erase(collections.begin() + i);
		}
	}
}

void RenderParticleCollections(std::vector<ParticleCollection>& collections, URenderer* renderer, ID3D11Buffer* vertexBuffer)
{
	for (int i = collections.size() - 1; i >= 0; i--)
	{
		collections[i].renderParticles(renderer, vertexBuffer);
	}
}

void DrawBackground(URenderer& renderer, HWND hwnd)
{
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);

	float windowWidth = static_cast<float>(clientRect.right - clientRect.left);
	float windowHeight = static_cast<float>(clientRect.bottom - clientRect.top);
	float windowAspect = windowWidth / windowHeight;

	float imageWidth = 3000.0f;
	float imageHeight = 2000.0f;
	float imageAspect = imageWidth / imageHeight;

	float bgHalfWidth = 1.0f;
	float bgHalfHeight = 1.0f;

	if (imageAspect > windowAspect)
	{
		bgHalfHeight = 1.0f;
		bgHalfWidth = imageAspect / windowAspect;
	}
	else
	{
		bgHalfWidth = 1.0f;
		bgHalfHeight = windowAspect / imageAspect;
	}

	renderer.DrawRectSprite(
		gBackgroundSprite,
		FVector(0.0f, 0.0f, 0.0f),
		0.0f,
		bgHalfWidth,
		bgHalfHeight,
		false
	);
}

void DrawGoals(URenderer& renderer)
{
	const float goalHalfWidth = 0.20f;
	const float goalHalfHeight = 0.45f;

	const FVector leftGoalPos(-0.87f, -0.36f, 0.0f);
	const FVector rightGoalPos(0.87f, -0.36f, 0.0f);

	if (gLeftGoalSprite)
	{
		renderer.DrawRectSprite(
			gLeftGoalSprite,
			leftGoalPos,
			0.0f,
			goalHalfWidth,
			goalHalfHeight,
			false
		);
	}

	if (gRightGoalSprite)
	{
		renderer.DrawRectSprite(
			gRightGoalSprite,
			rightGoalPos,
			0.0f,
			goalHalfWidth,
			goalHalfHeight,
			false
		);
	}
}

void ReleaseGameAssets(std::vector<IActor*>& actors, std::vector<SkillGauge*>& gauges)
{
	for (IActor* actor : actors)
	{
		delete actor;
	}
	actors.clear();

	for (SkillGauge* gauge : gauges)
	{
		delete gauge;
	}
	gauges.clear();

	if (gBackgroundSprite)
	{
		gBackgroundSprite->Release();
		gBackgroundSprite = nullptr;
	}

	if (gLeftGoalSprite)
	{
		gLeftGoalSprite->Release();
		gLeftGoalSprite = nullptr;
	}

	if (gRightGoalSprite)
	{
		gRightGoalSprite->Release();
		gRightGoalSprite = nullptr;
	}

	if (gPlayButtonSprite)
	{
		gPlayButtonSprite->Release();
		gPlayButtonSprite = nullptr;
	}

	if (gExitButtonSprite)
	{
		gExitButtonSprite->Release();
		gExitButtonSprite = nullptr;
	}

	if (gTitleSprite)
	{
		gTitleSprite->Release();
		gTitleSprite = nullptr;
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	// 클라이언트 영역을 16:9로 유지하기 위한 윈도우 사이즈 계산
	int targetClientWidth = 1280; 
	int targetClientHeight = 720;
	RECT windowRect = { 0, 0, targetClientWidth, targetClientHeight };

	DWORD windowStyle = WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW;
	AdjustWindowRectEx(&windowRect, windowStyle, FALSE, 0);

	int actualWindowWidth = windowRect.right - windowRect.left;
	int actualWindowHeight = windowRect.bottom - windowRect.top;
	
	HWND hWnd = CreateWindowExW(0, WindowClass, Title, 
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		actualWindowWidth, actualWindowHeight, nullptr, nullptr, hInstance, nullptr);

	renderer.Create(hWnd);
	renderer.CreateShader();
	renderer.CreateConstantBuffer();
	renderer.CreateRectConstantBuffer();
	renderer.CreateTextureStates();
	renderer.CreateSpriteQuad();

	CreateWICTextureFromFile(
		renderer.Device,
		renderer.DeviceContext,
		L"Assets\\background1.jpg",
		nullptr,
		&gBackgroundSprite
	);

	CreateWICTextureFromFile(
		renderer.Device,
		renderer.DeviceContext,
		L"Assets\\post_left.png",
		nullptr,
		&gLeftGoalSprite
	);

	CreateWICTextureFromFile(
		renderer.Device,
		renderer.DeviceContext,
		L"Assets\\post_right.png",
		nullptr,
		&gRightGoalSprite
	);

	CreateWICTextureFromFile(
		renderer.Device,
		renderer.DeviceContext,
		L"Assets\\button_play1.png",
		nullptr,
		&gPlayButtonSprite
	);

	CreateWICTextureFromFile(
		renderer.Device,
		renderer.DeviceContext,
		L"Assets\\button_exit.png",
		nullptr,
		&gExitButtonSprite
	);

	CreateWICTextureFromFile(
		renderer.Device,
		renderer.DeviceContext,
		L"Assets\\title1.png",
		nullptr,
		&gTitleSprite
	);

	LoadSelectUIAssets(renderer);
	LoadResultUIAssets(renderer);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);
	
	FVertexSimple groundVertices[] =
	{
		{ -1.0f, -0.72f, 0.0f, 0.2f, 0.7f, 0.2f, 1.0f },
		{  1.0f, -0.92f, 0.0f, 0.2f, 0.7f, 0.2f, 1.0f },
		{ -1.0f, -0.92f, 0.0f, 0.2f, 0.7f, 0.2f, 1.0f },

		{ -1.0f, -0.72f, 0.0f, 0.2f, 0.7f, 0.2f, 1.0f },
		{  1.0f, -0.72f, 0.0f, 0.2f, 0.7f, 0.2f, 1.0f },
		{  1.0f, -0.92f, 0.0f, 0.2f, 0.7f, 0.2f, 1.0f },
	};

	// Goal
	using namespace GM_defaults;
	FVertexSimple goalVertices_1[] =
	{
		{ rightGoalX, goalY + goalHeight, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
		{ rightGoalX + goalWidth, goalY, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
		{ rightGoalX, goalY, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },

		{ rightGoalX, goalY + goalHeight, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
		{ rightGoalX + goalWidth, goalY + goalHeight, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
		{ rightGoalX + goalWidth, goalY, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
	};

	FVertexSimple goalVertices_2[] =
	{
		{ leftGoalX, goalY + goalHeight, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
		{ leftGoalX + goalWidth, goalY, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
		{ leftGoalX, goalY, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },

		{ leftGoalX, goalY + goalHeight, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
		{ leftGoalX + goalWidth, goalY + goalHeight, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
		{ leftGoalX + goalWidth, goalY, 0.0f, 0.9f, 0.9f, 0.9f, 1.0f },
	};

	UINT numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	ID3D11Buffer* vertexBufferSphere = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
	ID3D11Buffer* vertexBufferGround = renderer.CreateVertexBuffer(groundVertices, sizeof(groundVertices));
	ID3D11Buffer* vertexBufferGoal_1 = renderer.CreateVertexBuffer(goalVertices_1, sizeof(goalVertices_1));
	ID3D11Buffer* vertexBufferGoal_2 = renderer.CreateVertexBuffer(goalVertices_2, sizeof(goalVertices_2));

	bool bIsExit = false;

	// Left, Right, Bottom, Top
	FWorldBounds bounds = { -1.0f, 1.0f, GROUND_HEIGHT, 1.0f };
	std::vector<IActor*> actors;
	std::vector<SkillGauge*> gauges;

	const int targetFPS = 60;
	const double targetFrameTime = 1000.0 / targetFPS;

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER prevCounter;
	QueryPerformanceCounter(&prevCounter);

#ifdef _DEBUG
	// 디버그 출력
	ConsoleHelper console;
#endif

	// 비동기 처리 관리자
	AsyncTaskManager* asyncTaskManager = AsyncTaskManager::getInstance();

	// 오디오 초기화
	AudioManager* audioManager = AudioManager::getInstance();
	audioManager->LoadAndPlayBGM("Assets/Ambient.wav");
	
	std::vector<ParticleCollection> particleCollection;
	
	GameState currentState = Playing;
	GameManager gameManager(&currentState);
	UPhysicsSystem physicsSystem;

	while (!bIsExit)
	{
		HandleMessages(bIsExit);
		LARGE_INTEGER nowCounter;
		QueryPerformanceCounter(&nowCounter);

		float dt = static_cast<float>(
			(nowCounter.QuadPart - prevCounter.QuadPart) /
			static_cast<double>(frequency.QuadPart)
			);
		prevCounter = nowCounter;

		if (gResetDeltaTime)
		{
			dt = 0.0f;
			QueryPerformanceCounter(&prevCounter);
			gResetDeltaTime = false;
		}
		if (dt > (1.0f / 30.0f))
		{
			dt = 1.0f / 30.0f;
		}
		gameManager.updatePause(dt);
		UpdateParticleCollections(particleCollection, dt); // Update all particle collection that are alive

		if (gGameState == EGameState::Game)
		{
			gameManager.updatePause(dt);

			if (currentState != Paused && currentState != GameOver)
			{
				gameManager.updateTime(dt);
				asyncTaskManager->Tick(dt);
				Update(actors, gauges);
				physicsSystem.Step(actors, bounds, dt);
				gameManager.handleGoals(actors, audioManager);

				int victory = gameManager.checkVictory();
				if (victory != -1)
				{
					if (victory == 0)
						gMatchResult = EMatchResult::Player1Win;
					else if (victory == 1)
						gMatchResult = EMatchResult::Player2Win;
					else
						gMatchResult = EMatchResult::Draw;

					gGameState = EGameState::Result;
				}
			}
		}

		renderer.Prepare();
		renderer.SetGameViewport();

		// ground 고정
		FVector4 groundPosRadius = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
		float groundAngle = 0.0f;

		renderer.PrepareShader();
		DrawBackground(renderer, hWnd);
		renderer.PrepareColorShader();
		//renderer.UpdateConstant(groundPosRadius, groundAngle);
		//renderer.RenderPrimitive(vertexBufferGround, 6);
		/*renderer.RenderPrimitive(vertexBufferGoal_1, 6);
		renderer.RenderPrimitive(vertexBufferGoal_2, 6);*/


		// Render particles
		renderer.PrepareColorShader();
		gameManager.UpdateParticleCollections(dt);
		gameManager.RenderParticleCollections(&renderer, vertexBufferSphere);
		for (IActor* actor : actors)
		{
			ParticleEmitter* emitter = dynamic_cast<ParticleEmitter*>(actor);
			if (emitter)
			{
				emitter->UpdateParticleCollections(dt);
				emitter->RenderParticleCollections(&renderer, vertexBufferSphere);
			}
		}

		renderer.SetUiViewport();

		if (gGameState == EGameState::Ready)
		{
			DrawReadyUI(renderer, hWnd);
		
		}
		else if (gGameState == EGameState::Select)
		{
			bool bStartClicked = HandleSelectUIInput(
				hWnd,
				renderer,
				//actors,
				gPlayer1Selection,
				gPlayer2Selection,
				gCharacterDatabase,
				//gGameState,
				gWasLeftMouseDown
			);

			DrawSelectUI(renderer, hWnd, gPlayer1Selection, gPlayer2Selection);
			
			if (bStartClicked)
			{
				StartMatch(renderer, actors, gauges, gameManager);
				gGameState = EGameState::Game;
			}
		}
		else if (gGameState == EGameState::Game || gGameState == EGameState::Result)
		{
			renderer.SetGameViewport();

			if (GDebugDrawColliders)
			{
				renderer.SetGameViewport();
				DrawDebugColliders(&renderer, actors, vertexBufferSphere, numVerticesSphere);
			}

			renderer.PrepareColorShader();

			// Render gauges
			for (SkillGauge* gauge : gauges)
			{
				gauge->Render(&renderer);
			}
			DrawGoals(renderer);
			DrawActors(&renderer, actors);

			if (gGameState == EGameState::Result)
			{
				renderer.SetUiViewport();
				DrawResultOverlay(renderer, hWnd, gMatchResult);

				if (HandleResultUIInput(hWnd))
				{
					EndMatch(actors, gauges);
					gGameState = EGameState::Ready;
					currentState = Playing;
					gameManager.resetMatch();

					gWasLeftMouseDown_Ready = true;
					gWasLeftMouseDown = true;
				}
			}
		}

		

		renderer.SetUiViewport();
		DrawImGui(renderer, actors, gauges, gameManager);
		// renderer.UpdateConstant(groundPosRadius, groundAngle);
		// renderer.RenderPrimitive(vertexBufferGround, 6);

		// DrawActors(&renderer, actors);
		// DrawImGui();

		renderer.SwapBuffer();

		LARGE_INTEGER frameEndCounter;
		double frameElapsedMs = 0.0;
		do
		{
			Sleep(0);
			QueryPerformanceCounter(&frameEndCounter);
			frameElapsedMs =
				(frameEndCounter.QuadPart - nowCounter.QuadPart) * 1000.0 /
				static_cast<double>(frequency.QuadPart);
		} while (frameElapsedMs < targetFrameTime);

		Time::UpdateTime(dt);
	}

	ReleaseGameAssets(actors, gauges);
	ReleaseSelectUIAssets();
	ReleaseResultUIAssets();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	return 0;
}
