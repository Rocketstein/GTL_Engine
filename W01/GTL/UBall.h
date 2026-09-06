#pragma once
#include "UPrimitive.h"
#include "Structs.h"

class UBall : public UPrimitive
{
public:
	FVector Location;
	FVector Velocity;
	float Radius;
	float Mass;
	static int TotalNumBalls;

	int Index = 0;
	float Angle = 0.0f;
	float AngularVelocity = 0.0f;

public:
	UBall()
	{
		Reset();
		++TotalNumBalls;
	}

	~UBall() override
	{
		--TotalNumBalls;
	}

	void Reset()
	{
		Location.x = (float)(rand() % 2000) / 1000.0f - 1.0f;
		Location.y = (float)(rand() % 2000) / 1000.0f - 1.0f;
		Location.z = 0.0f;

		Velocity.x = ((float)(rand() % 100 - 50) * 0.001f);
		Velocity.y = ((float)(rand() % 100 - 50) * 0.001f);

		Radius = ((float)(rand() % 100) / 1000.0f + 0.001f);
		Mass = Radius * 10.0f;

		Angle = 0.0f;
		AngularVelocity = ((float)(rand() % 200 - 100)) * 0.001f;
	}

	void SetRadius(const float& size)
	{
		if (size > 0.0f) Radius = size;
	}

	static inline float Dot2(const float& ax, const float& ay, const float& bx, const float& by)
	{
		return ax * bx + ay * by;
	}

	static inline void AddCrossW_R(const float& w, const float& rx, const float& ry, float& outx, float& outy)
	{
		outx += -w * ry;
		outy += w * rx;
	}

	float GetInertia() const
	{
		return 0.5f * Mass * Radius * Radius;
	}

	void ApplyWallImpulse(const float& nx, const float& ny, const bool& enableFriction)
	{
		float v_n = Dot2(Velocity.x, Velocity.y, nx, ny);
		if (v_n >= 0.0f) return;

		float m = Mass;
		if (m <= 0.0f) return;

		float Jn = -(2.0f) * v_n * m;

		Velocity.x += (Jn / m) * nx;
		Velocity.y += (Jn / m) * ny;

		if (!enableFriction) return;

		const float mu = 0.40f;

		float tx = -ny;
		float ty = nx;

		float r = Radius;
		float I = GetInertia();
		if (I <= 0.0f) return;

		float rx = -nx * r;
		float ry = -ny * r;

		float vcx = Velocity.x;
		float vcy = Velocity.y;
		AddCrossW_R(AngularVelocity, rx, ry, vcx, vcy);

		float v_t = Dot2(vcx, vcy, tx, ty);

		float rxt = rx * ty - ry * tx;
		float k = (1.0f / m) + (rxt * rxt) / I;
		if (k <= 0.0f) return;

		float Jt = -v_t / k;

		float maxF = mu * fabsf(Jn);
		if (Jt > maxF) Jt = maxF;
		if (Jt < -maxF) Jt = -maxF;

		Velocity.x += (Jt / m) * tx;
		Velocity.y += (Jt / m) * ty;

		AngularVelocity += (Jt * rxt) / I;
	}

	void Move(const FWorldBounds& bounds, const bool& isGravity, const bool& isAngularVelocity) override
	{
		if (isGravity)
		{
			Velocity.y -= GRAVITY;
		}

		Location.x += Velocity.x;
		Location.y += Velocity.y;

		if (isAngularVelocity)
		{
			Angle += AngularVelocity;
			const float TWO_PI = 6.28318530718f;
			if (Angle > TWO_PI) Angle -= TWO_PI;
			if (Angle < 0.0f)   Angle += TWO_PI;
		}

		if (Location.x < bounds.left + Radius)
		{
			Location.x = bounds.left + Radius;
			ApplyWallImpulse(1.0f, 0.0f, isAngularVelocity);
		}
		else if (Location.x > bounds.right - Radius)
		{
			Location.x = bounds.right - Radius;
			ApplyWallImpulse(-1.0f, 0.0f, isAngularVelocity);
		}

		if (Location.y < bounds.top + Radius)
		{
			Location.y = bounds.top + Radius;
			ApplyWallImpulse(0.0f, 1.0f, isAngularVelocity);
		}
		else if (Location.y > bounds.bottom - Radius)
		{
			Location.y = bounds.bottom - Radius;
			ApplyWallImpulse(0.0f, -1.0f, isAngularVelocity);
		}
	}

	FVector4 GetPosRadius() override
	{
		return FVector4(Location.x, Location.y, Location.z, Radius);
	}

	float GetAngle() override
	{
		return isfinite(Angle) ? Angle : 0.0f;
	}
};
