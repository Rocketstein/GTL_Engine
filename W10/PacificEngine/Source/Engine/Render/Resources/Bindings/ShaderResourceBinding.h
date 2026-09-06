#pragma once

#include "Core/CoreTypes.h"

struct ID3D11ShaderResourceView;

struct FShaderResourceBinding
{
    uint32 SlotIndex = 0;
    ID3D11ShaderResourceView* SRV = nullptr;
};

inline constexpr uint32 GMaxTrackedShaderResourceSlots = 64;
