#pragma once
#include "Constants.h"
#include "Structs.h"
#include "URenderer.h"
#include "Player.h"
#include "IActor.h"

using namespace SkillGaugeConstants;

class SkillGauge {
public:
	SkillGauge(const Player& p);
	void Update(float remainingGauge);
	void Render(URenderer* renderer);
	void SetBackgroundVertexBuffer(URenderer* renderer);
	void SetFillVertexBuffer(URenderer* renderer);

	static void LinkPlayer(std::vector<IActor*>& actors, std::vector<SkillGauge*>& gauges, URenderer* renderer) {
		std::cout << actors.size() << " actors in LinkPlayer\n";
		for (IActor* actor : actors) {
			Player* player = dynamic_cast<Player*>(actor);
			if (player) {
				SkillGauge* gauge = new SkillGauge(*player);
				gauge->SetBackgroundVertexBuffer(renderer);
				gauge->SetFillVertexBuffer(renderer);
				gauge->linkedPlayer = player;
				gauges.push_back(gauge);
			}
		}
	}

	FVertexSimple backgroundVertices[6];
	FVertexSimple fillVertices[6];
	int playerIndex;
	Player* linkedPlayer = nullptr;
private:
	ISkill* skill;	// Pointer to the skill associated with this gauge
	ID3D11Buffer* backgroundVertexBuffer = nullptr;
	ID3D11Buffer* fillVertexBuffer = nullptr;
	const float renderPosX;	// Top-Left
	const float renderPosY;	// Top-Left
	float currentGauge = 0.0f;
	const float maxGauge = maxSkillGauge;
};