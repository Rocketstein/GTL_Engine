#include "UIHelpers.h"

#include "URenderer.h"
#include "Structs.h"

bool GDebugDrawUIRects = false;

FVector GetMouseNDC(HWND hwnd)
{
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(hwnd, &p);

	RECT rc;
	GetClientRect(hwnd, &rc);

	float width = (float)(rc.right - rc.left);
	float height = (float)(rc.bottom - rc.top);

	float x = (p.x / width) * 2.0f - 1.0f;
	float y = -((p.y / height) * 2.0f - 1.0f);

	return FVector(x, y, 0.0f);
}

bool IsPointInRect(
	const FVector& point, const FVector& center,
	float halfWidth, float halfHeight)
{
	return
		point.x > center.x - halfWidth &&
		point.x < center.x + halfWidth &&
		point.y > center.y - halfHeight &&
		point.y < center.y + halfHeight;
}

void DrawDebugRect(URenderer& renderer, const FVector& center, float halfWidth, float halfHeight)
{
	if (!GDebugDrawUIRects) return;

	FVertexSimple vertices[6] =
	{
		{ center.x - halfWidth, center.y + halfHeight, 0.0f, 1.0f, 0.0f, 0.0f, 0.35f },
		{ center.x + halfWidth, center.y - halfHeight, 0.0f, 1.0f, 0.0f, 0.0f, 0.35f },
		{ center.x - halfWidth, center.y - halfHeight, 0.0f, 1.0f, 0.0f, 0.0f, 0.35f },

		{ center.x - halfWidth, center.y + halfHeight, 0.0f, 1.0f, 0.0f, 0.0f, 0.35f },
		{ center.x + halfWidth, center.y + halfHeight, 0.0f, 1.0f, 0.0f, 0.0f, 0.35f },
		{ center.x + halfWidth, center.y - halfHeight, 0.0f, 1.0f, 0.0f, 0.0f, 0.35f },
	};

	ID3D11Buffer* rectBuffer = renderer.CreateVertexBuffer(vertices, sizeof(vertices));
	if (!rectBuffer) return;

	FVector4 posRadius[1] = { FVector4(0.0f, 0.0f, 0.0f, 1.0f) };
	float angle[1] = { 0.0f };

	renderer.PrepareShader();
	renderer.UpdateConstant(posRadius, angle, 1);
	renderer.RenderPrimitive(rectBuffer, 6);

	renderer.ReleaseVertexBuffer(rectBuffer);
}