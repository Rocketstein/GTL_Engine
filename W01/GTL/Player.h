#pragma once
#include "IActor.h"
#include "GameTypes.h"
#include "Constants.h"
#include "Particles.h"
#include "skills.h"

class Player : public IActor, public ParticleEmitter
{
public:
	Player(int playerID);
	~Player();
	void Update();
	virtual void Reset();
	void Render(URenderer* renderer) override;
	void OnHitBall(IActor *other);
	void OnHitFeet();
	bool GetIsGrounded();

	void OnLeft(FInputState);
	void OnRight(FInputState);
	void OnJump(FInputState);
	void OnKick(FInputState);
	void OnSkill(FInputState);

	int GetPlayerID() const;
	float GetSkillGauge() const;
	void AwardSkillGauge(float bonus);
	ISkill* GetSkill() const;
	void SetSkill(ISkill* newSkill);
	void UpdateSkill(float dt);

	void SetCharacterData(const CharacterData* newCharacterData);
	const CharacterData* GetCharacterData() const;
	void SetBodySprite(ID3D11ShaderResourceView* newSprite);
	void SetFeetSprites(ID3D11ShaderResourceView* newSprite);
	void SetAuraSprite(ID3D11ShaderResourceView* newSprite);

	void DisableSkill();
private:
	ISkill* skill;
	int playerID;	// 1p or 2p
	float moveSpeed;
	bool bGrounded;
	bool bKicking;
	bool bDashing;
	int kickFrameCount;
	int walkFrameCount;
	float dashTimer;
	float lastLeftInputTime;
	float lastRightInputTime;
	float moveDir;
	void HandlePlayerX();
	void Walk(float dx);
	void StopWalk();
	void Dash(float dir);
	void HandlePlayerY();
	void Jump();
	void UpdateFlags();
	void UpdateFeetPosition();
	void RecoverSkillGauge();

	float skillGauge = 0;		// Maxxed out for testing purposes, but should be gated behind gameplay requirements
	bool skillStandBy = false;

	const CharacterData* CharacterInfo = nullptr;
};

