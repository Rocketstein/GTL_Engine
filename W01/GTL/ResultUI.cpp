#include "SelectUI.h"
#include "UIHelpers.h"

#include "URenderer.h"
#include "IActor.h"
#include "ImGui/imgui.h"
#include <WICTextureLoader.h>

using namespace DirectX;

namespace
{
	//ID3D11ShaderResourceView* gResultSprite = nullptr;
	ID3D11ShaderResourceView* gWinSprite = nullptr;
	ID3D11ShaderResourceView* gLoseSprite = nullptr;
	ID3D11ShaderResourceView* gDrawSprite = nullptr;
	ID3D11ShaderResourceView* gBackToTitleSprite = nullptr;

	bool gWasLeftMouseDown_Result = false;
}

void LoadResultUIAssets(URenderer& renderer)
{
	//CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\score1.png", nullptr, &gResultSprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\youwin.png", nullptr, &gWinSprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\youlose.png", nullptr, &gLoseSprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\youdraw.png", nullptr, &gDrawSprite);
	CreateWICTextureFromFile(renderer.Device, renderer.DeviceContext, L"Assets\\back1.png", nullptr, &gBackToTitleSprite);
}

void ReleaseResultUIAssets()
{
	auto ReleaseSRV = [](ID3D11ShaderResourceView*& srv)
		{
			if (srv)
			{
				srv->Release();
				srv = nullptr;
			}
		};

	//ReleaseSRV(gResultSprite);
	ReleaseSRV(gWinSprite);
	ReleaseSRV(gLoseSprite);
	ReleaseSRV(gDrawSprite);
	ReleaseSRV(gBackToTitleSprite);
}

void DrawResultOverlay(URenderer& renderer, HWND hwnd, EMatchResult matchResult)
{
    UNREFERENCED_PARAMETER(hwnd);

    /*if (gResultSprite)
    {
        renderer.DrawRectSprite(
            gResultSprite,
            FVector(0.0f, 0.18f, 0.0f),
            0.0f,
            0.30f,
            0.12f,
            false
        );
    }*/

    ID3D11ShaderResourceView* resultTextSprite = nullptr;

    switch (matchResult)
    {
    case EMatchResult::Player1Win:
        resultTextSprite = gWinSprite;
        break;

    case EMatchResult::Player2Win:
        resultTextSprite = gLoseSprite;
        break;

    case EMatchResult::Draw:
        resultTextSprite = gDrawSprite;
        break;

    default:
        break;
    }


    if (resultTextSprite)
    {
        renderer.DrawRectSprite(
            resultTextSprite,
            FVector(0.0f, 0.10f, 0.0f),
            0.0f,
            0.64f,
            0.48f,
            false
        );
    }
    // && 수정(back 버튼 크기)
    float backImageWidth = 1146.0f;
    float backImageHeight = 501.0f;
    float backAspect = backImageWidth / backImageHeight;

    float backHalfHeight = 0.06f;
    float backHalfWidth = backHalfHeight * backAspect;

    FVector backButtonPos(0.0f, -0.40f, 0.0f);

    if (gBackToTitleSprite)
    {
        renderer.DrawRectSprite(
            gBackToTitleSprite,
            backButtonPos,
            0.0f,
            backHalfWidth,
            backHalfHeight,
            false
        );
    }

    DrawDebugRect(renderer, backButtonPos, 0.14f, 0.06f);
}

bool HandleResultUIInput(HWND hwnd)
{
    FVector buttonPos(0.0f, -0.40f, 0.0f);
    float hitHalfWidth = 0.14f;
    float hitHalfHeight = 0.06f;

    FVector mouse = GetMouseNDC(hwnd);
    bool isLeftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    bool clicked = false;

    if (isLeftMouseDown && !gWasLeftMouseDown_Result)
    {
        if (IsPointInRect(mouse, buttonPos, hitHalfWidth, hitHalfHeight))
        {
            clicked = true;
        }
    }

    gWasLeftMouseDown_Result = isLeftMouseDown;
    return clicked;
}