#include "Player.h"
#include "Constants.h"
#include "Time.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace
{
	constexpr int BACK_FOOT_RENDER_INDEX = 0;
	constexpr int BODY_RENDER_INDEX = 1;
	constexpr int FRONT_FOOT_RENDER_INDEX = 2;
	constexpr int SKILL_AURA_RENDER_INDEX = 3;

	constexpr int BODY_COLLIDER_INDEX = 0;
	constexpr int FRONT_FOOT_COLLIDER_INDEX = 1;
	constexpr int BACK_FOOT_COLLIDER_INDEX = 2;

	constexpr float PLAYER_ACCEL = 25.0f;
	constexpr float PLAYER_DECEL = 35.0f;

	constexpr float BODY_SIZE = 0.045f;
	constexpr float FOOT_SIZE = 0.0225f;
	//constexpr float SKILL_AURA_SIZE = 0.15f;
}

static float MoveToward(float current, float target, float maxDelta)
{
	float delta = target - current;

	if (std::fabs(delta) <= maxDelta)
		return target;

	return current + (delta > 0.0f ? maxDelta : -maxDelta);
}

Player::Player(int pID) :
	playerID(pID),
	moveSpeed(PLAYER_MOVESPEED),
	bGrounded(false),
	bKicking(false),
	bDashing(false),
	kickFrameCount(0),
	walkFrameCount(0),
	dashTimer(DASH_COOLDOWN),
	lastLeftInputTime(-1.0f),
	lastRightInputTime(-1.0f),
	moveDir(0)
{
	SetPosition(FVector(0.0f, -0.35f, 0.0f));
	SetRotation(0.0f);
	SetScale(0.18f);
	SetRestitution(0.0f);

	SetApplyGravity(true);
	SetBodyType(EBodyType::Dynamic);
	SetApplyRotation(true);
	SetMass(10.0f);
	SetCollisionShape(ECollisionShape::Circle);
	SetCollisionMode(ECollisionMode::Block);

	// ��Ƽ ���� ��Ʈ
	ClearRenderParts();
	AddRenderPart(FOOT_SIZE * 2, FVector(0.0f, -PLAYER_LEG_LENGTH, 0.0f)); // back foot
	AddRenderPart(BODY_SIZE, FVector(0.0f, 0.0f, 0.0f));               // body
	AddRenderPart(FOOT_SIZE * 2, FVector(0.0f, -PLAYER_LEG_LENGTH, 0.0f)); // front foot
	AddRenderPart(BODY_SIZE * 2.2f, FVector(0.0f, 0.0f, 0.0f));               // skill aura (placeholder)
	RenderParts[SKILL_AURA_RENDER_INDEX].bVisible = false;		   // Set the skill aura sprite invisible as default

	// ��Ƽ �ݶ��̴�
	ClearColliderParts();
	AddCircleColliderPart(BODY_SIZE, FVector(0.0f, 0.0f, 0.0f), ECollisionMode::Block);               // body
	AddCircleColliderPart(FOOT_SIZE, FVector(0.0f, -PLAYER_LEG_LENGTH, 0.0f), ECollisionMode::Block); // front foot
	AddCircleColliderPart(FOOT_SIZE, FVector(0.0f, -PLAYER_LEG_LENGTH, 0.0f), ECollisionMode::Block);// back foot
	//AddAABBColliderPart(FVector(0.25f, 0.001f, 0), FVector(0.0f, -PLAYER_LEG_LENGTH - 0.1f, 0.0f), ECollisionMode::Block);
}

Player::~Player()
{
}

void Player::Update()
{
	//std::cout << "PlayerID: " << playerID << " SkillGauge: " << skillGauge << '\n';
	SetRotation(0.0f);

	HandlePlayerX();
	UpdateFeetPosition();
	UpdateFlags();
	HandlePlayerY();
	RecoverSkillGauge();

	if (!bControllable) {
		FVector vel = GetVelocity();
		vel.x = 0;
		SetVelocity(vel);
	}

	// std::cout << "Player Xpos: " << GetPosition().x << '\n';
	// std::cout << "Player Yvel: " << GetVelocity().y << '\n';
}

void Player::Reset()
{
	SetVelocity(FVector(0, 0, 0));
	SetAngularVelocity(0.0f);
	SetRotation(0.0f);
	DisableSkill();
}

void Player::UpdateFlags()
{
	bGrounded = GetIsGrounded();

	if (bKicking)
	{
		kickFrameCount++;
		if (kickFrameCount > PLAYER_KICK_STARTUP_FRAME + PLAYER_KICK_RECOVERY_FRAME)
		{
			bKicking = false;
		}
		return;
	}
}

void Player::RecoverSkillGauge() {
	if (skillGauge < maxSkillGauge) {
		skillGauge += skillGaugeIncreaseRate;
		if (skillGauge > maxSkillGauge) skillGauge = maxSkillGauge;
	}
}

void Player::HandlePlayerX()
{
	FVector vel = GetVelocity();

	float targetSpeedX = 0.0f;
	float accel = PLAYER_ACCEL;
	dashTimer += Time::DeltaTime;

	if (bDashing)
	{
		if (dashTimer <= DASH_DURATION)
		{
			targetSpeedX = (vel.x >= 0.0f ? 1.0f : -1.0f) * DASH_POWER;
			accel = 9999.0f;
		}
		else
		{
			bDashing = false;
		}
	}

	if (!bDashing)
	{
		targetSpeedX = moveDir * moveSpeed;
		accel = (moveDir == 0.0f) ? PLAYER_DECEL : PLAYER_ACCEL;
	}

	vel.x = MoveToward(vel.x, targetSpeedX, accel * Time::DeltaTime);
	SetVelocity(vel);

	if (std::fabs(moveDir) > 0.0f)
		walkFrameCount++;
	else
		walkFrameCount = 0;

	// moveDir �� ����� �ڿ��� ���
	moveDir = 0.0f;
}

void Player::HandlePlayerY()
{
	// ���鿡 ���� ��� ���� ������ ������ �ʰ� ��ġ ����
	if (bGrounded)
	{
		//FVector position = GetPosition();
		//position.y = (std::max)(position.y, GROUND_HEIGHT + PLAYER_HEIGHT);
		//SetPosition(position);
	}
	// ���߿� ���� ��� �߷°��ӵ� ó��
	else
	{
		//FVector velocity = GetVelocity();
		//velocity.y -= GRAVITY;
		//SetVelocity(velocity);
	}
}

void Player::Render(URenderer* renderer)
{
	if (!renderer || !GetSprite()) return;
	IActor::Render(renderer);
}

void Player::Walk(float moveDir)
{
	FVector vel = GetVelocity();
	vel.x = moveDir * moveSpeed;
	walkFrameCount++;
	SetVelocity(vel);
}

void Player::StopWalk()
{
	walkFrameCount = 0;
}

void Player::OnHitBall(IActor* other)
{
	if (!other || !skillStandBy) {
		return;
	}
	std::cout << "[Player] OnHitBall\n";
	SkillContext ctx;
	ctx.owner = this;
	ctx.target = other;
	DisableSkill();
	//skillStandBy = false;
	//RenderParts[SKILL_AURA_RENDER_INDEX].bVisible = false; // Hide the skill aura sprite when the skill is activated
	skill->Activate(ctx);
}

void Player::OnHitFeet()
{
}

bool Player::GetIsGrounded()
{
	const float epsilon = 0.001f;

	for (int i = 0; i < GetColliderPartCount(); ++i)
	{
		const FColliderPart& collider = GetColliderPart(i);
		if (!collider.bEnabled)
			continue;

		const FVector center = GetColliderWorldCenter(i);

		float bottom = center.y;

		if (collider.Shape == ECollisionShape::Circle)
		{
			bottom -= collider.Radius;
		}
		else if (collider.Shape == ECollisionShape::AABB)
		{
			bottom -= collider.HalfExtent.y;
		}

		if (bottom <= GROUND_HEIGHT + epsilon)
			return true;
	}

	return false;
}

void Player::OnLeft(FInputState inputState)
{
	if (inputState.Down)
	{
		float currentTime = Time::TimeSinceStart;
		float timeSinceLastTap = currentTime - lastLeftInputTime;
		lastLeftInputTime = currentTime;
		lastRightInputTime = -1.0f;

		if (timeSinceLastTap <= DASH_TAP_THRESHOLD)
		{
			Dash(-1.0f);
			return;
		}
	}
	if (inputState.Hold)
		moveDir -= 1.0f;
}

void Player::OnRight(FInputState inputState)
{
	if (inputState.Down)
	{
		float currentTime = Time::TimeSinceStart;
		float timeSinceLastTap = currentTime - lastRightInputTime;
		lastRightInputTime = currentTime;
		lastLeftInputTime = -1.0f;

		if (timeSinceLastTap <= DASH_TAP_THRESHOLD)
		{
			Dash(1.0f);
			return;
		}
	}
	if (inputState.Hold)
		moveDir += 1.0f;
}

void Player::Dash(float dir)
{
	if (dashTimer < DASH_COOLDOWN) return;

	bDashing = true;
	dashTimer = 0;

	FVector dashVector = { dir * DASH_POWER, 0, 0 };
	SetVelocity(dashVector);
	// std::cout << "��� ����, ����:" << dir << "\n";
}

void Player::OnJump(FInputState inputState)
{
	if (inputState.Down && GetIsGrounded() && bControllable)
	{
		Jump();
		return;
	}
}

void Player::Jump()
{
	std::cout << "[Player] Jump\n";
	FVector vel = GetVelocity();
	vel.y = PLAYER_JUMP_POWER;
	SetVelocity(vel);
}

void Player::OnKick(FInputState inputState)
{
	if (inputState.Down && !bKicking && bControllable)
	{
		bKicking = true;
		kickFrameCount = 0;
	}
}

void Player::OnSkill(FInputState)
{
	if (skillGauge >= maxSkillGauge && bControllable) {
		skillStandBy = true;
		RenderParts[SKILL_AURA_RENDER_INDEX].bVisible = true; // Set the skill aura sprite visible
		skillGauge = 0;	// Consume the skill gauge when the skill is activated
	}
}

void Player::UpdateFeetPosition()
{
	//if (GetRenderPartCount() < 3 || GetColliderPartCount() < 3)
	//	return;

	const float facing = (playerID == 1) ? 1.0f : -1.0f;

	FVector localScale = { playerID == 1 ? 1.0f : -1.0f, 1.0f, 1.0f };

	if (bKicking)
	{
		// TODO: 2p ���̽� ó��
		bool bIsKickup = kickFrameCount <= PLAYER_KICK_STARTUP_FRAME;
		float ratio;
		if (bIsKickup)
		{
			ratio = (float)kickFrameCount / PLAYER_KICK_STARTUP_FRAME;
		}
		else
		{
			ratio = 1 - (float)(kickFrameCount - PLAYER_KICK_STARTUP_FRAME) / PLAYER_KICK_RECOVERY_FRAME;
		}
		float deltaAngle = ratio * PLAYER_KICK_ANGLE;
		float worldAngle = 270 + deltaAngle;
		float localX = std::cos(worldAngle * DEG2RAD) * PLAYER_LEG_LENGTH;
		float localY = std::sin(worldAngle * DEG2RAD) * PLAYER_LEG_LENGTH;

		FVector frontLocalPos = { localX * facing, localY, 0.0f };
		FVector backLocalPos = { 0.0f, -PLAYER_LEG_LENGTH, 0.0f };

		// ���� ��Ʈ ����
		GetRenderPart(FRONT_FOOT_RENDER_INDEX).LocalOffset = frontLocalPos;
		GetRenderPart(BACK_FOOT_RENDER_INDEX).LocalOffset = backLocalPos;
		GetRenderPart(FRONT_FOOT_RENDER_INDEX).RotationOffset = std::cos(worldAngle * DEG2RAD) * facing;

		// �ݶ��̴� ��Ʈ ����
		GetColliderPart(FRONT_FOOT_COLLIDER_INDEX).LocalOffset = frontLocalPos;
		GetColliderPart(BACK_FOOT_COLLIDER_INDEX).LocalOffset = backLocalPos;
	}
	else
	{
		FVector down = {
			(float)std::cos(270 * DEG2RAD),
			(float)std::sin(270 * DEG2RAD),
			0
		};

		FVector anchorLocal = down * PLAYER_LEG_LENGTH;

		float cycle = (float)walkFrameCount / PLAYER_WALK_FRAME;
		float dx = std::cos(2 * PI * cycle) * PLAYER_WALK_WIDTH * facing;
		FVector deltaPos = { dx, 0.0f, 0.0f };

		FVector frontLocalPos = anchorLocal + deltaPos;
		FVector backLocalPos = anchorLocal - deltaPos;

		// ���� ��Ʈ ����
		GetRenderPart(FRONT_FOOT_RENDER_INDEX).LocalOffset = frontLocalPos;
		GetRenderPart(BACK_FOOT_RENDER_INDEX).LocalOffset = backLocalPos;

		// �ݶ��̴� ��Ʈ ����
		GetColliderPart(FRONT_FOOT_COLLIDER_INDEX).LocalOffset = frontLocalPos;
		GetColliderPart(BACK_FOOT_COLLIDER_INDEX).LocalOffset = backLocalPos;
	}
}

void Player::SetBodySprite(ID3D11ShaderResourceView* newSprite)
{
	GetRenderPart(BODY_RENDER_INDEX).Sprite = newSprite;
}

void Player::SetFeetSprites(ID3D11ShaderResourceView* newSprite)
{
	GetRenderPart(BACK_FOOT_RENDER_INDEX).Sprite = newSprite;
	GetRenderPart(FRONT_FOOT_RENDER_INDEX).Sprite = newSprite;
}

void Player::SetAuraSprite(ID3D11ShaderResourceView* newSprite)
{
	GetRenderPart(SKILL_AURA_RENDER_INDEX).Sprite = newSprite;
}

void Player::SetCharacterData(const CharacterData* newCharacterData)
{
	CharacterInfo = newCharacterData;
}

const CharacterData* Player::GetCharacterData() const
{
	return CharacterInfo;
}

int Player::GetPlayerID() const
{
	return playerID;
}

float Player::GetSkillGauge() const
{
	return skillGauge;
}

void Player::AwardSkillGauge(float bonus) {
	skillGauge += bonus;
	if (skillGauge > maxSkillGauge) {
		skillGauge = maxSkillGauge;
	}
}

ISkill* Player::GetSkill() const
{
	return skill;
}

void Player::SetSkill(ISkill* newSkill)
{
	skill = newSkill;
}

void Player::UpdateSkill(float dt)
{
	if (skill)
	{
		skill->Update(dt);
	}
};

void Player::DisableSkill() {
	RenderParts[SKILL_AURA_RENDER_INDEX].bVisible = false;
	skillStandBy = false;
}