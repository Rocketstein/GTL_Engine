#pragma once
#include "Constants.h"
#include <iostream>

class IActor;
class Player;

struct SkillContext {
	Player* owner;
	IActor* target;
};

class ISkill {
public:
	ISkill() = default;
	virtual ~ISkill() = default;
	virtual void Activate(SkillContext& ctx) = 0;
	virtual void Update(float dt) = 0;
	virtual void Launch() = 0;

	bool IsReadyToLaunch() const {
		return standByElapsed >= skillStandByDuration;	// Example: Skill is ready to launch after 1 second of standby
	}

protected:
	SkillContext context;
	float index = 0;
	float lifespan = 0;	// For over-time effect skills
	bool activated = false;
	bool recovering = false;
	float standByElapsed = 0;	// Skills require a delay before activation
	float recoveryTimer = 0;

	virtual void Recover() = 0;
};

class SkillA : public ISkill {
public:
	void Activate(SkillContext& ctx) override;
	void Update(float dt) override;
	void Launch() override;

protected:	
	void Recover() override;
};

class SkillB : public ISkill {
public:
	void Activate(SkillContext& ctx) override;
	void Update(float dt) override;
	void Launch() override;

protected:
	void Recover() override;
};