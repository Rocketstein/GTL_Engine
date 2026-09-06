#include "IActor.h"

class GoalTrigger : public IActor
{
public:
	virtual void OnTrigger(IActor* Other);
	void SetPushDirection(float newDirection);
	float GetPushDirection() const;
	void SetPushPower(float newPower);
	float GetPushPower() const;

protected:
	float Direction = 0.0f;
	float power = 1.0f;
};