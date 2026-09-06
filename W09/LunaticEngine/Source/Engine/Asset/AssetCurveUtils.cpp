#include "Engine/Asset/AssetCurveUtils.h"

#include "Engine/Math/CurveFloat.h"
#include "Object/ObjectFactory.h"

namespace FAssetCurveUtils
{
    UCurveFloat* MakeCurveFromBezier(const FAssetBezierCurve& Bezier, float Amplitude)
    {
        UCurveFloat* Curve = UObjectManager::Get().CreateObject<UCurveFloat>();
        if (!Curve)
        {
            return nullptr;
        }

        Curve->Curve.clear();

        constexpr int32 SampleCount = 16;
        for (int32 Index = 0; Index <= SampleCount; ++Index)
        {
            const float T = static_cast<float>(Index) / static_cast<float>(SampleCount);
            Curve->AddKey(T, Bezier.Evaluate(T) * Amplitude);
        }

        return Curve;
    }
}
