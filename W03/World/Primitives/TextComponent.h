#pragma once
#include "World/PrimitiveComponent.h"

class UTextComponent : public UPrimitiveComponent
{
private:
	std::wstring Text = L"쿼드";
	FVector Scale = { 0.0f, 1.0f, 1.0f };
	FVector4 Color = { 1, 1, 1, 1 };
	ETextAlignment Alignment = ETextAlignment::Center;

public:
	DECLARE_CLASS(UTextComponent, UPrimitiveComponent)
	UTextComponent();
	virtual void UpdateWorldAABB() override;
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult);
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Text;
	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }

	void Serialize(json::JSON& j) const override;
	void Deserialize(const json::JSON& j) override;

public:
	std::wstring const& GetText() const { return Text; }
	FVector const& GetTextScale() const { return Scale; }
	FVector4 const& GetColor() const { return Color; }
	ETextAlignment const& GetAlignment() const { return Alignment; }

	void SetText(std::wstring& newText) { Text = newText; }
	void SetTextScale(FVector& newScale) { Scale = newScale; }
	void SetTextColor(FVector4& newColor) { Color = newColor; }
	void SetTextAlignment(ETextAlignment& newAlignment) { Alignment = newAlignment; }
};
