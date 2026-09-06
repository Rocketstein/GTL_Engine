#include "LuaBindingInternal.h"
#include "Math/Vector.h"

namespace LuaBinding
{
    void RegisterCommon(sol::state& Lua)
    {
        Lua.new_usertype<FVector>(
            "FVector",
            sol::constructors<FVector(), FVector(float, float, float)>(),

            "X", &FVector::X,
            "Y", &FVector::Y,
            "Z", &FVector::Z,

            sol::meta_function::addition, [](const FVector& A, const FVector& B)
            { return A + B; },
            sol::meta_function::subtraction, [](const FVector& A, const FVector& B)
            { return A - B; },
            sol::meta_function::multiplication, sol::overload([](const FVector& V, float Scalar)
                                                              { return V * Scalar; }, [](float Scalar, const FVector& V)
                                                              { return V * Scalar; }));

        Lua.new_usertype<FVector2>(
            "FVector2",
            sol::constructors<FVector2(), FVector2(float, float)>(),

            "X", &FVector2::X,
            "Y", &FVector2::Y,

            sol::meta_function::addition, [](const FVector2& A, const FVector2& B)
            { return A + B; },
            sol::meta_function::subtraction, [](const FVector2& A, const FVector2& B)
            { return A - B; },
            sol::meta_function::multiplication, sol::overload([](const FVector2& V, float Scalar)
                                                              { return V * Scalar; }, [](float Scalar, const FVector2& V)
                                                              { return V * Scalar; }));
    }
}
