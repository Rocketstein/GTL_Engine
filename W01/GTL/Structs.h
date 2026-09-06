#pragma once
#include <string>

struct FWorldBounds
{
	float left, right, top, bottom;
};

struct FVertexSimple
{
	float x, y, z;
	float r, g, b, a;
};

struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

	FVector operator+(const FVector& ref) const
	{
		return { x + ref.x, y + ref.y, z + ref.z };
	}

	FVector operator-(const FVector& ref) const
	{
		return { x - ref.x, y - ref.y, z - ref.z };
	}

	FVector operator*(float val) const
	{
		return { x * val, y * val, z * val };
	}

	FVector operator/(float val) const
	{
		return { x / val, y / val, z / val };
	}

	static FVector scale(const FVector a, const FVector b)
	{
		return { a.x * b.x, a.y * b.y, a.z * b.z };
	}

	FVector scale(const FVector a)
	{
		return { x * a.x, y * a.y, z * a.z };
	}

	float squareMagnitude()
	{
		return x * x + y * y + z * z;
	}
};

struct FVector4
{
	float x, y, z, w;
	FVector4(float _x = 0, float _y = 0, float _z = 0, float _w = 0) : x(_x), y(_y), z(_z), w(_w) {}
};

struct FVertexTexture
{
	float x, y, z;
	float u, v;
};

//struct CharacterData
//{
//	int Id;
//	std::wstring Name;
//	std::wstring SpritePath;
//};