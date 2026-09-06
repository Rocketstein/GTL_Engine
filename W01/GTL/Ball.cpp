#include "Ball.h"
#include "AudioManager.h"

Ball::Ball()
{
	SetApplyGravity(true);
	SetBodyType(EBodyType::Dynamic);
	SetApplyRotation(true);
	SetMass(0.3f);
	SetCollisionShape(ECollisionShape::Circle);

	AddRenderPart(BALL_SIZE / 2, FVector(0.0f, 0.0f, 0.0f));
	AddCircleColliderPart(BALL_SIZE / 2, FVector(0.0f, 0.0f, 0.0f), ECollisionMode::Block);
    SetRestitution(BALL_RESTITUTION);

	SetPosition(FVector(GM_defaults::kickoffX, GM_defaults::kickoffY, 0.0f));
	SetRotation(0.0f);
	SetVelocity(FVector(0.006f, -0.004f, 0.0f));
	SetScale(BALL_SIZE);
}

void Ball::Update()
{
}

void Ball::Reset()
{
	SetVelocity(FVector(0.0f, 0.0f));
	SetAngularVelocity(0.0f);
	SetRotation(0.0f);
}

void Ball::Render(URenderer* renderer)
{
	// IActor::Render(renderer);
	if (!renderer || !GetSprite()) return;

	renderer->DrawSprite(GetSprite(), GetPosition(), GetRotation(), GetScale(), false);
}

void Ball::OnHitWall()
{
	// 플레이어 다리 사이에 꼈을 때 무한 사운드로 게임 터지는 것 방지
	// 속도뿐만 아니라 위치도 검사
	static FVector posBeforeHit = { 0, 0, 0 };
	float deltaPos = (GetPosition() - posBeforeHit).squareMagnitude();
	if (GetVelocity().squareMagnitude() > BALL_SOUND_THRESHOLD_VELOCITY
		&& deltaPos > BALL_SOUND_THRESHOLD_DISPLACEMENT)
	{
		AudioManager::getInstance()->PlaySFX("ball_bounce");
	}
	posBeforeHit = GetPosition();
}