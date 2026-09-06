#pragma once

class UPrimitiveComponent;

struct FOverlapInfo
{
    UPrimitiveComponent* OverlappedComponent = nullptr;

	FOverlapInfo() = default;

	explicit FOverlapInfo(UPrimitiveComponent* InOverlappedComponent)
        : OverlappedComponent(InOverlappedComponent) {}

	bool operator==(const UPrimitiveComponent* Other) const
	{
        return OverlappedComponent == Other;
	}
};