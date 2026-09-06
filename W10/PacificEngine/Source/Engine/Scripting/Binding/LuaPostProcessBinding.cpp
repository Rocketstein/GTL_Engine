#include "LuaBindingInternal.h"
#include "Scripting/LuaEngineBinding.h"
#include "Render/PostProcess/PostProcessController.h"
#include "Scripting/LuaScriptTypes.h"

#include <algorithm>
#include <cctype>

namespace
{
FString NormalizeFadeCurveName(FString Value)
{
    Value.erase(
        std::remove_if(
            Value.begin(),
            Value.end(),
            [](unsigned char Ch)
            {
                return Ch == '_' || Ch == '-' || std::isspace(Ch);
            }),
        Value.end());

    std::transform(
        Value.begin(),
        Value.end(),
        Value.begin(),
        [](unsigned char Ch)
        {
            return static_cast<char>(std::tolower(Ch));
        });

    return Value;
}

EFadeCurve ParseFadeCurve(const FString& CurveName, EFadeCurve Fallback)
{
    const FString Curve = NormalizeFadeCurveName(CurveName);
    if (Curve == "linear")
    {
        return EFadeCurve::Linear;
    }
    if (Curve == "easein")
    {
        return EFadeCurve::EaseIn;
    }
    if (Curve == "easeout")
    {
        return EFadeCurve::EaseOut;
    }
    if (Curve == "easeinout" || Curve == "smoothstep")
    {
        return EFadeCurve::EaseInOut;
    }

    return Fallback;
}

void ReadFadeOptions(const sol::variadic_args& Args, FVector& OutColor, EFadeCurve& OutCurve)
{
    for (const sol::object& Arg : Args)
    {
        FVector Color;
        if (ReadLuaVec3(Arg, Color))
        {
            OutColor = Color;
            continue;
        }

        if (Arg.is<FString>())
        {
            OutCurve = ParseFadeCurve(Arg.as<FString>(), OutCurve);
        }
    }
}

FVector ReadOptionalColor(const sol::variadic_args& Args, const FVector& Fallback)
{
    FVector Color = Fallback;
    for (const sol::object& Arg : Args)
    {
        ReadLuaVec3(Arg, Color);
    }
    return Color;
}
} // namespace

FLuaPostProcessHandle::FLuaPostProcessHandle(FPostProcessController* InController)
    : Controller(InController)
{
}

bool FLuaPostProcessHandle::IsValid() const
{
    return Controller != nullptr;
}

bool FLuaPostProcessHandle::FadeIn(float Duration, const sol::variadic_args& Args) const
{
    if (!Controller)
    {
        return false;
    }

    FFadeController& FadeController = Controller->GetFadeController();
    FVector Color = FadeController.GetColor();
    EFadeCurve Curve = EFadeCurve::EaseInOut;
    ReadFadeOptions(Args, Color, Curve);

    FadeController.FadeIn(Duration, Color, Curve);
    return true;
}

bool FLuaPostProcessHandle::FadeOut(float Duration, const sol::variadic_args& Args) const
{
    if (!Controller)
    {
        return false;
    }

    FFadeController& FadeController = Controller->GetFadeController();
    FVector Color = FadeController.GetColor();
    EFadeCurve Curve = EFadeCurve::EaseInOut;
    ReadFadeOptions(Args, Color, Curve);

    FadeController.FadeOut(Duration, Color, Curve);
    return true;
}

bool FLuaPostProcessHandle::FadeTo(float TargetAlpha, float Duration, const sol::variadic_args& Args) const
{
    if (!Controller)
    {
        return false;
    }

    FFadeController& FadeController = Controller->GetFadeController();
    FVector Color = FadeController.GetColor();
    EFadeCurve Curve = EFadeCurve::EaseInOut;
    ReadFadeOptions(Args, Color, Curve);

    FadeController.FadeTo(TargetAlpha, Duration, Color, Curve);
    return true;
}

bool FLuaPostProcessHandle::SetFadeAlpha(float Alpha, const sol::variadic_args& Args) const
{
    if (!Controller)
    {
        return false;
    }

    FFadeController& FadeController = Controller->GetFadeController();
    FVector Color = FadeController.GetColor();
    EFadeCurve IgnoredCurve = EFadeCurve::EaseInOut;
    ReadFadeOptions(Args, Color, IgnoredCurve);

    FadeController.SetAlpha(Alpha, Color);
    return true;
}

void FLuaPostProcessHandle::StopFade() const
{
    if (Controller)
    {
        Controller->GetFadeController().Stop();
    }
}

void FLuaPostProcessHandle::ClearFade() const
{
    if (Controller)
    {
        Controller->GetFadeController().Reset();
    }
}

bool FLuaPostProcessHandle::IsFadePlaying() const
{
    return Controller ? Controller->GetFadeController().IsPlaying() : false;
}

float FLuaPostProcessHandle::GetFadeAlpha() const
{
    return Controller ? Controller->GetFadeController().GetAlpha() : 0.0f;
}

sol::table FLuaPostProcessHandle::GetFadeColor(sol::this_state State) const
{
    sol::state_view Lua(State);
    return MakeLuaVec3(
        Lua,
        Controller ? Controller->GetFadeController().GetColor() : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaPostProcessHandle::SetVignettingEnabled(bool bEnabled) const
{
    if (!Controller)
    {
        return false;
    }

    Controller->SetVignettingEnabled(bEnabled);
    return true;
}

bool FLuaPostProcessHandle::SetVignetting(float Intensity, float Radius, float Softness, const sol::variadic_args& Args) const
{
    if (!Controller)
    {
        return false;
    }

    const FVignettingSettings& Settings = Controller->GetVignettingSettings();
    const FVector Color = ReadOptionalColor(Args, Settings.Color);
    Controller->SetVignetting(Intensity, Radius, Softness, Color);
    return true;
}

bool FLuaPostProcessHandle::IsVignettingEnabled() const
{
    return Controller ? Controller->GetVignettingSettings().bEnabled : false;
}

float FLuaPostProcessHandle::GetVignettingIntensity() const
{
    return Controller ? Controller->GetVignettingSettings().Intensity : 0.0f;
}

float FLuaPostProcessHandle::GetVignettingRadius() const
{
    return Controller ? Controller->GetVignettingSettings().Radius : 0.0f;
}

float FLuaPostProcessHandle::GetVignettingSoftness() const
{
    return Controller ? Controller->GetVignettingSettings().Softness : 0.0f;
}

sol::table FLuaPostProcessHandle::GetVignettingColor(sol::this_state State) const
{
    sol::state_view Lua(State);
    return MakeLuaVec3(
        Lua,
        Controller ? Controller->GetVignettingSettings().Color : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaPostProcessHandle::SetGammaCorrectionEnabled(bool bEnabled) const
{
    if (!Controller)
    {
        return false;
    }

    Controller->SetGammaCorrectionEnabled(bEnabled);
    return true;
}

bool FLuaPostProcessHandle::SetDisplayGamma(float DisplayGamma) const
{
    if (!Controller)
    {
        return false;
    }

    Controller->SetDisplayGamma(DisplayGamma);
    return true;
}

bool FLuaPostProcessHandle::IsGammaCorrectionEnabled() const
{
    return Controller ? Controller->GetGammaCorrectionSettings().bEnabled : false;
}

float FLuaPostProcessHandle::GetDisplayGamma() const
{
    return Controller ? Controller->GetGammaCorrectionSettings().DisplayGamma : 1.0f;
}

bool FLuaPostProcessHandle::SetLetterboxEnabled(bool bEnabled) const
{
    if (!Controller)
    {
        return false;
    }

    Controller->SetLetterboxEnabled(bEnabled);
    return true;
}

bool FLuaPostProcessHandle::SetLetterbox(float TargetAspectRatio, float Opacity, const sol::variadic_args& Args) const
{
    if (!Controller)
    {
        return false;
    }

    const FLetterboxSettings& Settings = Controller->GetLetterboxSettings();
    const FVector Color = ReadOptionalColor(Args, Settings.Color);
    Controller->SetLetterbox(TargetAspectRatio, Opacity, Color);
    return true;
}

bool FLuaPostProcessHandle::IsLetterboxEnabled() const
{
    return Controller ? Controller->GetLetterboxSettings().bEnabled : false;
}

float FLuaPostProcessHandle::GetLetterboxTargetAspectRatio() const
{
    return Controller ? Controller->GetLetterboxSettings().TargetAspectRatio : 0.0f;
}

float FLuaPostProcessHandle::GetLetterboxOpacity() const
{
    return Controller ? Controller->GetLetterboxSettings().Opacity : 0.0f;
}

sol::table FLuaPostProcessHandle::GetLetterboxColor(sol::this_state State) const
{
    sol::state_view Lua(State);
    return MakeLuaVec3(
        Lua,
        Controller ? Controller->GetLetterboxSettings().Color : FVector(0.0f, 0.0f, 0.0f));
}

namespace LuaBinding
{
    void RegisterPostProcess(sol::state& Lua)
    {
        Lua.new_usertype<FLuaPostProcessHandle>(
            "PostProcess",
            sol::no_constructor,
            "IsValid", &FLuaPostProcessHandle::IsValid,
            "FadeIn", &FLuaPostProcessHandle::FadeIn,
            "FadeOut", &FLuaPostProcessHandle::FadeOut,
            "FadeTo", &FLuaPostProcessHandle::FadeTo,
            "SetFadeAlpha", &FLuaPostProcessHandle::SetFadeAlpha,
            "StopFade", &FLuaPostProcessHandle::StopFade,
            "ClearFade", &FLuaPostProcessHandle::ClearFade,
            "IsFadePlaying", &FLuaPostProcessHandle::IsFadePlaying,
            "GetFadeAlpha", &FLuaPostProcessHandle::GetFadeAlpha,
            "GetFadeColor", &FLuaPostProcessHandle::GetFadeColor,
            "SetVignettingEnabled", &FLuaPostProcessHandle::SetVignettingEnabled,
            "SetVignetting", &FLuaPostProcessHandle::SetVignetting,
            "IsVignettingEnabled", &FLuaPostProcessHandle::IsVignettingEnabled,
            "GetVignettingIntensity", &FLuaPostProcessHandle::GetVignettingIntensity,
            "GetVignettingRadius", &FLuaPostProcessHandle::GetVignettingRadius,
            "GetVignettingSoftness", &FLuaPostProcessHandle::GetVignettingSoftness,
            "GetVignettingColor", &FLuaPostProcessHandle::GetVignettingColor,
            "SetGammaCorrectionEnabled", &FLuaPostProcessHandle::SetGammaCorrectionEnabled,
            "SetDisplayGamma", &FLuaPostProcessHandle::SetDisplayGamma,
            "IsGammaCorrectionEnabled", &FLuaPostProcessHandle::IsGammaCorrectionEnabled,
            "GetDisplayGamma", &FLuaPostProcessHandle::GetDisplayGamma,
            "SetLetterboxEnabled", &FLuaPostProcessHandle::SetLetterboxEnabled,
            "SetLetterbox", &FLuaPostProcessHandle::SetLetterbox,
            "IsLetterboxEnabled", &FLuaPostProcessHandle::IsLetterboxEnabled,
            "GetLetterboxTargetAspectRatio", &FLuaPostProcessHandle::GetLetterboxTargetAspectRatio,
            "GetLetterboxOpacity", &FLuaPostProcessHandle::GetLetterboxOpacity,
            "GetLetterboxColor", &FLuaPostProcessHandle::GetLetterboxColor);
    }
}
