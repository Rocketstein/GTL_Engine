#pragma once

#include "Core/Types/CoreTypes.h"

////////////////////////////////////////////
// 머티리얼 텍스처 슬롯 t0~t7. 셰이더 텍스처 바인딩 리플렉션(FShaderTextureBinding)과 register 기준으로 매칭된다.
enum class EMaterialTextureSlot : uint32
{
	Diffuse = 0,
	Normal = 1,
	OpacityMask = 2,
	Roughness = 2,
	Metallic = 3,
	Emissive = 4,
	AO = 5,
	Custom0 = 6,
	Custom1 = 7,
	Max = 8
};

namespace MaterialTextureSlot
{
	inline FString ToString(int32 SlotEnum)
	{
		switch (SlotEnum)
		{
		case (int)EMaterialTextureSlot::Diffuse:
			return FString("Diffuse");

		case (int)EMaterialTextureSlot::Normal:
			return FString("Normal");

		case (int)EMaterialTextureSlot::Roughness:
			return FString("Roughness");

		case (int)EMaterialTextureSlot::Metallic:
			return FString("Metallic");

		case (int)EMaterialTextureSlot::Emissive:
			return FString("Emissive");

		case (int)EMaterialTextureSlot::AO:
			return FString("AO");

		case (int)EMaterialTextureSlot::Custom0:
			return FString("Custom0");

		case (int)EMaterialTextureSlot::Custom1:
			return FString("Custom1");

		default:
			return FString(); // 알 수 없는 슬롯 — 빈 문자열 반환(렌더/직렬화 경로에서 throw 금지)
		}
	}

	// 색(sRGB) 텍스처 슬롯 판정 — Diffuse/Emissive/Custom* 은 sRGB, 그 외(Normal/Roughness/AO 등)는 Linear.
	// 텍스처 로드 시 색공간 결정의 단일 소스(엔진/에디터 공용).
	inline bool IsSRGBTextureSlot(const FString& SlotName)
	{
		return SlotName == "DiffuseTexture"
			|| SlotName == "EmissiveTexture"
			|| SlotName == "Custom0Texture"
			|| SlotName == "Custom1Texture";
	}

	inline bool TryGetFixedSlotForTextureName(const FString& SlotName, uint32& OutSlot)
	{
		if (SlotName == "DiffuseTexture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::Diffuse); return true; }
		if (SlotName == "NormalTexture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::Normal); return true; }
		if (SlotName == "OpacityMaskTexture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::OpacityMask); return true; }
		if (SlotName == "RoughnessTexture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::Roughness); return true; }
		if (SlotName == "MetallicTexture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::Metallic); return true; }
		if (SlotName == "EmissiveTexture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::Emissive); return true; }
		if (SlotName == "AOTexture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::AO); return true; }
		if (SlotName == "Custom0Texture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::Custom0); return true; }
		if (SlotName == "Custom1Texture") { OutSlot = static_cast<uint32>(EMaterialTextureSlot::Custom1); return true; }
		return false;
	}
}
