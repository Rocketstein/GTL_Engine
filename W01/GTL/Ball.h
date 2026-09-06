#pragma once
#include "IActor.h"
using namespace BallConstants;

class Ball : public IActor
{
public:
	Ball();

	void Update();
	virtual void Reset();
	void Render(URenderer* renderer) override;
	void OnHitWall();
};