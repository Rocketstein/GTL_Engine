#pragma once

#include "Engine/Asset/AssetData.h"

class UCurveFloat;

namespace FAssetCurveUtils
{
    UCurveFloat* MakeCurveFromBezier(const FAssetBezierCurve& Bezier, float Amplitude);
}
