#pragma once

#include <vector>

#include "URenderer.h"
#include "Structs.h"

struct FColliderPart
{
	ECollisionShape Shape = ECollisionShape::Circle;
	ECollisionMode Mode = ECollisionMode::Block;

	FVector LocalOffset{ 0.0f, 0.0f, 0.0f };

	float Radius = 0.0f;                     // Circle
	FVector HalfExtent{ 0.0f, 0.0f, 0.0f }; // AABB

	FVector PrevLocalOffset = FVector(0.0f, 0.0f, 0.0f);
	FVector ExtraVelocity = FVector(0.0f, 0.0f, 0.0f);
	bool bHasPrevLocalOffset = false;

	bool bEnabled = true;
};

struct FRenderPart
{
	ID3D11ShaderResourceView* Sprite = nullptr;

	FVector LocalOffset{ 0.0f, 0.0f, 0.0f };
	float RotationOffset = 0.0f;
	float Scale = 0.1f;

	bool bVisible = true;
};

class IActor
{
public:
	virtual ~IActor();
	virtual void Update();
	virtual void Reset() {};
	virtual void Render(URenderer* renderer);
	virtual void OnTrigger(IActor* Other) {}

	FVector GetPosition() const;
	void SetPosition(FVector newPosition);

	float GetRotation() const;
	void SetRotation(float newRotation);

	FVector GetVelocity() const;
	void SetVelocity(FVector newVelocity);

	float GetScale() const;   // root circle radius / default render scale
	void SetScale(float newScale);

	FVector GetHalfExtent() const;
	void SetHalfExtent(FVector newHalfExtent);

	float GetAngularVelocity() const;
	void SetAngularVelocity(float newAngularVelocity);

	float GetMass() const;
	void SetMass(float newMass);

	float GetRestitution() const;
	void SetRestitution(float newRestitution);

	EBodyType GetBodyType() const;
	void SetBodyType(EBodyType newBodyType);

	bool GetApplyRotation() const;
	void SetApplyRotation(bool newApplyRotation);

	bool GetApplyGravity() const;
	void SetApplyGravity(bool newApplyGravity);

	void SetSprite(ID3D11ShaderResourceView* newSprite);
	ID3D11ShaderResourceView* GetSprite() const;

	ECollisionShape GetCollisionShape() const;
	void SetCollisionShape(ECollisionShape newCollisionShape);

	ECollisionMode GetCollisionMode() const;
	void SetCollisionMode(ECollisionMode newCollisionMode);

	int AddRenderPart(
		float scale,
		const FVector& localOffset,
		ID3D11ShaderResourceView* sprite = nullptr,
		float rotationOffset = 0.0f);

	void ClearRenderParts();
	int GetRenderPartCount() const;

	FRenderPart& GetRenderPart(int index);
	const FRenderPart& GetRenderPart(int index) const;

	void SetRenderPartSprite(int index, ID3D11ShaderResourceView* sprite);
	void SetAllRenderPartSprites(ID3D11ShaderResourceView* sprite);

	FVector GetRenderWorldPosition(int index) const;

	int AddCircleColliderPart(
		float radius,
		const FVector& localOffset,
		ECollisionMode mode = ECollisionMode::Block);

	int AddAABBColliderPart(
		const FVector& halfExtent,
		const FVector& localOffset,
		ECollisionMode mode = ECollisionMode::Block);

	void ClearColliderParts();
	int GetColliderPartCount() const;

	FColliderPart& GetColliderPart(int index);
	const FColliderPart& GetColliderPart(int index) const;

	FVector GetColliderWorldCenter(int index) const;

	bool GetControllable() const;
	bool GetInteractable() const;
	void SetControllable(bool newControllable);
	void SetInteractable(bool newInteractable);

	virtual void OnHitWall() { }

protected:
	void DrawRenderParts(URenderer* renderer) const;
	static FVector Rotate2D(const FVector& value, float angleRad);

protected:
	FVector Position{ 0.0f, 0.0f, 0.0f };
	float Rotation = 0.0f;

	FVector Velocity{ 0.0f, 0.0f, 0.0f };
	float Scale = 0.1f;
	FVector HalfExtent{ 0.0f, 0.0f, 0.0f };

	float AngularVelocity = 0.0f;
	float Mass = 1.0f;

	float Restitution = 0.4f;

	bool bApplyRotation = false;
	bool bApplyGravity = false;
	bool bControllable = true;
	bool bInteractable = true;
	EBodyType BodyType = EBodyType::Dynamic;

	ID3D11ShaderResourceView* Sprite = nullptr;
	ECollisionShape CollisionShape = ECollisionShape::Circle;
	ECollisionMode CollisionMode = ECollisionMode::Block;

	std::vector<FRenderPart> RenderParts;
	std::vector<FColliderPart> ColliderParts;
};