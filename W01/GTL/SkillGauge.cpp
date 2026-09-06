#include "SkillGauge.h"

SkillGauge::SkillGauge(const Player& p)
	: renderPosX(p.GetPlayerID() == 1 ? player1GaugePosX : player2GaugePosX), renderPosY(p.GetPlayerID() == 1 ? player1GaugePosY : player2GaugePosY) {
	skill = p.GetSkill();

	playerIndex = p.GetPlayerID();
	auto gaugeHalfWidth = gaugeWidth / 2.0f;
	auto gaugeHalfHeight = gaugeHeight / 2.0f;
	float left = -1 * gaugeHalfWidth;
	float right =  gaugeHalfWidth;
	float top = gaugeHalfHeight;
	float bottom = -1 * gaugeHalfHeight;

	// Triangle 1 (top-left, bottom-right, bottom-left)
	backgroundVertices[0] = { left,  top,    0, gaugeBackgroundColor[0], gaugeBackgroundColor[1], gaugeBackgroundColor[2], 1.0f};
	backgroundVertices[1] = { right, bottom, 0, gaugeBackgroundColor[0], gaugeBackgroundColor[1], gaugeBackgroundColor[2], 1.0f };
	backgroundVertices[2] = { left,  bottom, 0, gaugeBackgroundColor[0], gaugeBackgroundColor[1], gaugeBackgroundColor[2], 1.0f };

	// Triangle 2 (top-left, top-right, bottom-right)
	backgroundVertices[3] = { left,  top,    0, gaugeBackgroundColor[0], gaugeBackgroundColor[1], gaugeBackgroundColor[2], 1.0f };
	backgroundVertices[4] = { right, top,    0, gaugeBackgroundColor[0], gaugeBackgroundColor[1], gaugeBackgroundColor[2], 1.0f };
	backgroundVertices[5] = { right, bottom, 0, gaugeBackgroundColor[0], gaugeBackgroundColor[1], gaugeBackgroundColor[2], 1.0f };

	auto scale = gaugeWidth / numProgressbar;
	auto segmentWidth = (gaugeWidth / numProgressbar);
	auto segHalfWidth = segmentWidth / 2.0f;

	fillVertices[0] = { -segHalfWidth,  gaugeHalfHeight, 0, gaugeFillColor[0], gaugeFillColor[1], gaugeFillColor[2], 1.0f };
	fillVertices[1] = { segHalfWidth, -gaugeHalfHeight, 0, gaugeFillColor[0], gaugeFillColor[1], gaugeFillColor[2], 1.0f };
	fillVertices[2] = { -segHalfWidth, -gaugeHalfHeight, 0, gaugeFillColor[0], gaugeFillColor[1], gaugeFillColor[2], 1.0f };
	fillVertices[3] = { -segHalfWidth,  gaugeHalfHeight, 0, gaugeFillColor[0], gaugeFillColor[1], gaugeFillColor[2], 1.0f };
	fillVertices[4] = { segHalfWidth,  gaugeHalfHeight, 0, gaugeFillColor[0], gaugeFillColor[1], gaugeFillColor[2], 1.0f };
	fillVertices[5] = { segHalfWidth, -gaugeHalfHeight, 0, gaugeFillColor[0], gaugeFillColor[1], gaugeFillColor[2], 1.0f };
}

void SkillGauge::Update(float remainingGauge) {
	currentGauge = remainingGauge;
}

void SkillGauge::Render(URenderer* renderer) {
	// Render the background of the skill gauge
	// Update constant buffer
	FVector4 pRad = FVector4(renderPosX, renderPosY, 0.0f, 1.0f);
	float angle = 0.0f;
	renderer->UpdateConstant(pRad, angle);
	renderer->RenderPrimitive(backgroundVertexBuffer, 6);

	// Render the progress segments
	// Draw each filled segment with offset
	int filledSegments = static_cast<int>((currentGauge / 100.0f) * numProgressbar);
	filledSegments = max(0, min(filledSegments, numProgressbar)); // clamp
	float segmentWidth = gaugeWidth / numProgressbar;
	float startX = renderPosX - (gaugeWidth / 2.0f) + (segmentWidth / 2.0f);

	for (int i = 0; i < filledSegments; i++) {
		float segX = startX + i * segmentWidth;
		renderer->UpdateConstant(FVector4(segX, renderPosY, 0.0f, 1.0f), angle);
		renderer->RenderPrimitive(fillVertexBuffer, 6);
	}
}

void SkillGauge::SetBackgroundVertexBuffer(URenderer* renderer) {
	backgroundVertexBuffer = renderer->CreateVertexBuffer(backgroundVertices, sizeof(backgroundVertices));
}

void::SkillGauge::SetFillVertexBuffer(URenderer* renderer) {
	fillVertexBuffer = renderer->CreateVertexBuffer(fillVertices, sizeof(fillVertices));
}

//static void LinkPlayer(std::vector<IActor*>& actors, std::vector<SkillGauge*>& gauges, URenderer* renderer) {
//	for (IActor* actor : actors) {
//		Player* player = dynamic_cast<Player*>(actor);
//		if (player) {
//			SkillGauge* gauge = new SkillGauge(*player);
//			gauge->SetBackgroundVertexBuffer(renderer);
//			gauges.push_back(gauge);
//		}
//	}
//}