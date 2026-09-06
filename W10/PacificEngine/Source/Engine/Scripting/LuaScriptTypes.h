#pragma once

#include "Core/CoreTypes.h"
#include "Core/PropertyTypes.h"
#include "Math/Vector.h"
#include "sol.hpp"

class FArchive;

/*
 * Lua 스크립트에 노출할 프로퍼티 자료형을 추가하는 방법
 *
 * 이 파일은 Lua 프로퍼티 자료형 확장 포인트를 한곳에 모아두기 위한 파일입니다.
 * 새 타입을 추가할 때 LuaScriptAsset.cpp나 ScriptComponent.cpp에 타입별 switch를 새로 만들지 말고,
 * 아래 체크리스트를 따라 LuaScriptTypes.h/.cpp 안에서 먼저 처리하세요.
 *
 * 예를 들어 Color 타입을 추가한다면 다음 순서로 작업합니다.
 *
 * 1. ELuaScriptPropertyType에 새 enum 값을 추가합니다.
 *    예: Color
 *
 * 2. FLuaScriptValue에 값을 저장할 필드를 추가합니다.
 *    예: FVector4 ColorValue;
 *    지금 구조는 std::variant가 아니라 타입별 필드를 모두 들고 있는 방식입니다.
 *
 * 3. LuaScriptTypes.cpp의 operator<<에 새 필드를 직렬화합니다.
 *    이 작업을 빼먹으면 씬 저장, PIE 복제, 에디터 복원 경로에서 값이 사라질 수 있습니다.
 *
 * 4. ParseLuaScriptPropertyTypeName()에 Lua에서 사용할 타입 이름을 등록합니다.
 *    예: "color", "color4"
 *
 * 5. InferLuaScriptPropertyType()에 자동 추론이 가능한 경우만 추가합니다.
 *    bool, number, string처럼 값만 보고 안전하게 알 수 있는 타입만 추론하는 편이 좋습니다.
 *    Vec3처럼 table 기반 타입은 모호하므로 명시적인 type 선언을 요구하는 것이 안전합니다.
 *
 * 6. ReadLuaScriptValue()에 Lua default 값을 C++ 값으로 읽는 코드를 추가합니다.
 *    여기서 Lua 문법을 정합니다.
 *    예: vec3는 { 1, 2, 3 } 형태를 읽습니다.
 *
 * 7. SetLuaScriptTableValue()에 C++ 값을 Lua instance table에 넣는 코드를 추가합니다.
 *    Lua 스크립트가 self.MyValue로 읽는 최종 값은 여기서 결정됩니다.
 *
 * 8. GetLuaScriptEditorPropertyType()에 에디터 Details 패널 타입을 매핑합니다.
 *    이미 존재하는 EPropertyType을 재사용할 수 있으면 여기만 추가하면 됩니다.
 *    새 에디터 위젯이 필요하면 Core/PropertyTypes.h와 EditorDetailsPanel.cpp도 확장해야 합니다.
 *
 * 9. GetLuaScriptValuePtr()에 Details 패널이 수정할 실제 값 포인터를 반환합니다.
 *    Vec3처럼 float 배열을 요구하는 UI는 Vec3Value.Data처럼 UI가 기대하는 메모리 형태를 넘겨야 합니다.
 *
 * 10. Template.lua나 테스트용 Lua 파일에 예시를 추가해서 실제 로드, Details 표시, 저장/로드를 확인합니다.
 *
 * 새 타입을 추가했는데 LuaScriptAsset.cpp나 ScriptComponent.cpp를 고치고 싶어진다면,
 * 먼저 이 파일에 공통 함수로 모을 수 없는지 확인하세요. 이 구조의 목표는 타입별 지식을 여기로 모으는 것입니다.
 */
enum class ELuaScriptPropertyType : uint8
{
    Bool,
    Int,
    Float,
    String,
    Vec3,
};

struct FLuaScriptValue
{
    ELuaScriptPropertyType Type = ELuaScriptPropertyType::Float;
    bool BoolValue = false;
    int32 IntValue = 0;
    float FloatValue = 0.0f;
    FString StringValue;
    FVector Vec3Value;
};

struct FLuaScriptPropertyDesc
{
    FString Name;
    ELuaScriptPropertyType Type = ELuaScriptPropertyType::Float;
    FLuaScriptValue DefaultValue;
    float Min = 0.0f;
    float Max = 0.0f;
    float Speed = 0.1f;
};

struct FLuaScriptPropertyOverride
{
    FString Name;
    FLuaScriptValue Value;
};

FArchive& operator<<(FArchive& Ar, FLuaScriptValue& Value);
FArchive& operator<<(FArchive& Ar, FLuaScriptPropertyOverride& Override);

bool ParseLuaScriptPropertyTypeName(const FString& TypeName, ELuaScriptPropertyType& OutType);
bool InferLuaScriptPropertyType(const sol::object& ValueObject, ELuaScriptPropertyType& OutType);
FLuaScriptValue MakeDefaultLuaScriptValue(ELuaScriptPropertyType Type);
bool ReadLuaVec3(const sol::object& ValueObject, FVector& OutValue);
sol::table MakeLuaVec3(sol::state_view Lua, const FVector& Value);
bool ReadLuaVec4(const sol::object& ValueObject, FVector4& OutValue);
sol::table MakeLuaVec4(sol::state_view Lua, const FVector4& Value);
bool ReadLuaVec2(const sol::object& ValueObject, FVector2& OutValue);
sol::table MakeLuaVec2(sol::state_view Lua, const FVector2& Value);
bool ReadLuaScriptValue(const sol::object& ValueObject, ELuaScriptPropertyType Type, FLuaScriptValue& OutValue);
void SetLuaScriptTableValue(sol::state& Lua, sol::table Table, const FString& Name, const FLuaScriptValue& Value);
EPropertyType GetLuaScriptEditorPropertyType(ELuaScriptPropertyType Type);
void* GetLuaScriptValuePtr(FLuaScriptValue& Value);
