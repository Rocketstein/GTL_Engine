#include "IActor.h"

#include <cmath>
IActor::~IActor()
{
	if (Sprite)
	{
		Sprite->Release();
		Sprite = nullptr;
	}
}

void IActor::Update() { }

void IActor::Render(URenderer* renderer)
{
	if (!renderer)
		return;

	if (RenderParts.empty())
	{
		if (Sprite)
		{
			renderer->DrawSprite(Sprite, Position, Rotation, Scale, false);
		}
		return;
	}

	DrawRenderParts(renderer);
}

FVector IActor::GetPosition() const
{
	return Position;
}

void IActor::SetPosition(FVector newPosition)
{
	Position = newPosition;
	Position.z = 0.0f;
}

float IActor::GetRotation() const
{
	return Rotation;
}

void IActor::SetRotation(float newRotation)
{
	Rotation = newRotation;
}

FVector IActor::GetVelocity() const
{
	return Velocity;
}

void IActor::SetVelocity(FVector newVelocity)
{
	Velocity = newVelocity;
	Velocity.z = 0.0f;
}

float IActor::GetScale() const
{
	return Scale;
}

void IActor::SetScale(float newScale)
{
	Scale = newScale;
}

FVector IActor::GetHalfExtent() const
{
	return HalfExtent;
}

void IActor::SetHalfExtent(FVector newHalfExtent)
{
	HalfExtent = newHalfExtent;
}

float IActor::GetAngularVelocity() const
{
	return AngularVelocity;
}

void IActor::SetAngularVelocity(float newAngularVelocity)
{
	AngularVelocity = newAngularVelocity;
}

float IActor::GetMass() const
{
	return Mass;
}

void IActor::SetMass(float newMass)
{
	Mass = newMass;
}

float IActor::GetRestitution() const
{
	return Restitution;
}

void IActor::SetRestitution(float newRestitution)
{
	Restitution = newRestitution;
}

EBodyType IActor::GetBodyType() const
{
	return BodyType;
}

void IActor::SetBodyType(EBodyType newBodyType)
{
	BodyType = newBodyType;
}

bool IActor::GetApplyRotation() const
{
	return bApplyRotation;
}

void IActor::SetApplyRotation(bool newApplyRotation)
{
	bApplyRotation = newApplyRotation;
}

bool IActor::GetApplyGravity() const
{
	return bApplyGravity;
}

void IActor::SetApplyGravity(bool newApplyGravity)
{
	bApplyGravity = newApplyGravity;
}

void IActor::SetSprite(ID3D11ShaderResourceView* newSprite)
{
	Sprite = newSprite;

	if (!RenderParts.empty())
	{
		SetAllRenderPartSprites(newSprite);
	}
}

ID3D11ShaderResourceView* IActor::GetSprite() const
{
	if (Sprite)
		return Sprite;

	if (!RenderParts.empty())
		return RenderParts[0].Sprite;

	return nullptr;
}

ECollisionShape IActor::GetCollisionShape() const
{
	return CollisionShape;
}

void IActor::SetCollisionShape(ECollisionShape newCollisionShape)
{
	CollisionShape = newCollisionShape;
}

ECollisionMode IActor::GetCollisionMode() const
{
	return CollisionMode;
}

void IActor::SetCollisionMode(ECollisionMode newCollisionMode)
{
	for (int i = 0; i < GetColliderPartCount(); ++i)
	{
		GetColliderPart(i).Mode = newCollisionMode;
	}
}

int IActor::AddRenderPart(
	float scale,
	const FVector& localOffset,
	ID3D11ShaderResourceView* sprite,
	float rotationOffset)
{
	FRenderPart part;
	part.Sprite = sprite;
	part.LocalOffset = localOffset;
	part.RotationOffset = rotationOffset;
	part.Scale = scale;

	RenderParts.push_back(part);
	return static_cast<int>(RenderParts.size()) - 1;
}

void IActor::ClearRenderParts()
{
	RenderParts.clear();
}

int IActor::GetRenderPartCount() const
{
	return static_cast<int>(RenderParts.size());
}

FRenderPart& IActor::GetRenderPart(int index)
{
	return RenderParts[index];
}

const FRenderPart& IActor::GetRenderPart(int index) const
{
	return RenderParts[index];
}

void IActor::SetRenderPartSprite(int index, ID3D11ShaderResourceView* sprite)
{
	if (index < 0 || index >= static_cast<int>(RenderParts.size()))
		return;

	RenderParts[index].Sprite = sprite;
}

void IActor::SetAllRenderPartSprites(ID3D11ShaderResourceView* sprite)
{
	for (FRenderPart& part : RenderParts)
	{
		part.Sprite = sprite;
	}
}

FVector IActor::GetRenderWorldPosition(int index) const
{
	const FRenderPart& part = RenderParts[index];
	const FVector rotated = Rotate2D(part.LocalOffset, Rotation);

	return FVector(
		Position.x + rotated.x,
		Position.y + rotated.y,
		0.0f);
}

int IActor::AddCircleColliderPart(
	float radius,
	const FVector& localOffset,
	ECollisionMode mode)
{
	FColliderPart part;
	part.Shape = ECollisionShape::Circle;
	part.Mode = mode;
	part.LocalOffset = localOffset;
	part.Radius = radius;

	ColliderParts.push_back(part);
	return static_cast<int>(ColliderParts.size()) - 1;
}

int IActor::AddAABBColliderPart(
	const FVector& halfExtent,
	const FVector& localOffset,
	ECollisionMode mode)
{
	FColliderPart part;
	part.Shape = ECollisionShape::AABB;
	part.Mode = mode;
	part.LocalOffset = localOffset;
	part.HalfExtent = halfExtent;

	ColliderParts.push_back(part);
	return static_cast<int>(ColliderParts.size()) - 1;
}

void IActor::ClearColliderParts()
{
	ColliderParts.clear();
}

int IActor::GetColliderPartCount() const
{
	return static_cast<int>(ColliderParts.size());
}

FColliderPart& IActor::GetColliderPart(int index)
{
	return ColliderParts[index];
}

const FColliderPart& IActor::GetColliderPart(int index) const
{
	return ColliderParts[index];
}

FVector IActor::GetColliderWorldCenter(int index) const
{
	const FColliderPart& part = ColliderParts[index];
	const FVector rotated = Rotate2D(part.LocalOffset, Rotation);

	return FVector(
		Position.x + rotated.x,
		Position.y + rotated.y,
		0.0f);
}

void IActor::DrawRenderParts(URenderer* renderer) const
{
	for (int i = 0; i < static_cast<int>(RenderParts.size()); ++i)
	{
		const FRenderPart& part = RenderParts[i];
		if (!part.bVisible || !part.Sprite)
			continue;

		const FVector worldPos = GetRenderWorldPosition(i);
		renderer->DrawSprite(
			part.Sprite,
			worldPos,
			Rotation + part.RotationOffset,
			part.Scale,
			false);
	}
}

FVector IActor::Rotate2D(const FVector& value, float angleRad)
{
	const float c = std::cos(angleRad);
	const float s = std::sin(angleRad);

	return FVector(
		value.x * c - value.y * s,
		value.x * s + value.y * c,
		0.0f);
}

bool IActor::GetControllable() const
{
	return bControllable;
}

bool IActor::GetInteractable() const
{
	return bInteractable;
}

void IActor::SetControllable(bool newControllable)
{
	bControllable = newControllable;
}

void IActor::SetInteractable(bool newInteractable)
{
	bInteractable = newInteractable;
}