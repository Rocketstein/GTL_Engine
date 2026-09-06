#include "Viewport/ViewportCameraController.h"
#include "Core/Math/MathUtility.h"
#include <cmath>

namespace
{
    static constexpr float GPi = 3.14159265358979323846f;

    static float ToRadians(float Degrees) { return Degrees * (GPi / 180.0f); }

    static float Dot(const FVector3 &A, const FVector3 &B)
    {
        return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
    }

    static FVector3 Cross(const FVector3 &A, const FVector3 &B)
    {
        return FVector3(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X);
    }

    static float LengthSq(const FVector3 &V) { return Dot(V, V); }

    static FVector3 NormalizeSafe(const FVector3 &V)
    {
        const float LenSq = LengthSq(V);
        if (LenSq <= 1e-8f)
        {
            return FVector3(0.0f, 0.0f, 0.0f);
        }

        const float InvLen = 1.0f / std::sqrt(LenSq);
        return FVector3(V.X * InvLen, V.Y * InvLen, V.Z * InvLen);
    }
} // namespace

// Replace old member-function math usage with the helpers above.
// Example:
// const float PitchRad = ToRadians(PitchDegrees);
// const FVector3 Right = NormalizeSafe(Cross(Forward, Up));

void FViewportCameraController::Tick(float DeltaTime, const FCameraInput &Input)
{
    // 1. ?뚯쟾 泥섎━ (?고겢由?留덉슦???대룞)
    if (Input.YawDelta != 0.0f || Input.PitchDelta != 0.0f)
    {
        FRotator Rot = Transform.GetRotation();
        // Config.RotationSpeed ?곸슜 (湲곗〈 0.18f ? ?좎궗?섍쾶 議곗젙)
        Rot.Yaw += Input.YawDelta * 0.18f;
        Rot.Pitch = FMath::Clamp(Rot.Pitch - Input.PitchDelta * 0.18f, Config.MinPitchDegrees,
                                 Config.MaxPitchDegrees);
        Transform.SetRotation(Rot);
    }

    FVector3       Location = Transform.GetLocation();
    const FVector3 Forward = Transform.GetRotation().Vector().GetSafeNormal();
    FVector3       Right = FVector3::CrossProduct(FVector3::UpVector, Forward).GetSafeNormal();
    if (Right.IsNearlyZero())
    {
        Right = FVector3::RightVector;
    }

    // 2. ?대룞 泥섎━ (WASD / EQ) - 利됱떆 ?대룞 諛⑹떇
    const float MoveSpeed = 100.0f * DeltaTime; // (?꾩슂 ??Config.TranslationAcceleration ?ъ슜)
    Location = Location + Forward * (Input.MoveForward * MoveSpeed);
    Location = Location + Right * (Input.MoveRight * MoveSpeed);
    Location = Location + FVector3::UpVector * (Input.MoveUpWorld * MoveSpeed);

    // 3. ?⑤떇 泥섎━ (???대┃ ?대룞)
    if (Input.PanX != 0.0f || Input.PanY != 0.0f)
    {
        Location = Location - Right * (Input.PanX * Config.PanSpeed); // PanSpeed 1.5f 濡??ㅼ젙 沅뚯옣
        Location = Location + FVector3::UpVector * (Input.PanY * Config.PanSpeed);
    }

    // 4. 以?泥섎━ (留덉슦????援대━湲?
    if (Input.Zoom != 0.0f)
    {
        Location = Location + Forward * (Input.Zoom * 120.0f);
    }

    Transform.SetLocation(Location);
}
