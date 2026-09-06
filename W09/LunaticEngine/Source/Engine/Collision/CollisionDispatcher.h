#pragma once
#include "Core/Singleton.h"
#include "Collision/OverlapInfo.h"
#include "Component/Shape/ShapeComponent.h"

class FCollisionDispatcher : public TSingleton<FCollisionDispatcher> {
	friend class TSingleton<FCollisionDispatcher>;
	using ShapeCollisionFunc = bool(*)(const UShapeComponent*, const UShapeComponent*, FOverlapInfo&);

public:
	void Init();

	void Register(FString Shape0, FString Shape1, ShapeCollisionFunc Func) {
		CollisionMatrix[Shape0 + "::" + Shape1] = Func;
		CollisionMatrix[Shape1 + "::" + Shape0] = Func;
	}

	bool CheckCollision(const UShapeComponent* ShapeA, const UShapeComponent* ShapeB, FOverlapInfo& OutInfo) const {
		FString Key = FString(ShapeA->GetClass()->GetName()) + "::" + ShapeB->GetClass()->GetName();
		auto it = CollisionMatrix.find(Key);
		if (it != CollisionMatrix.end()) {
			return it->second(ShapeA, ShapeB, OutInfo);
		}
		return false;
	}

	bool CheckCollision(const UShapeComponent* ShapeA, const UShapeComponent* ShapeB) const {
		FOverlapInfo Info;
		return CheckCollision(ShapeA, ShapeB, Info);
	}

private:
	TMap<FString, ShapeCollisionFunc> CollisionMatrix;
};
