#include "Engine/Scripting/LuaScriptTypes.h"

#include "Serialization/Archive.h"

#include <algorithm>
#include <cctype>

namespace
{
FString ToLowerAscii(FString Value)
{
    std::ranges::transform(Value, Value.begin(),
                           [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
    return Value;
}
}

FArchive& operator<<(FArchive& Ar, FLuaScriptValue& Value)
{
    // 새 Lua 프로퍼티 타입이 값을 저장하는 필드를 추가했다면 여기에도 반드시 직렬화해야 합니다.
    Ar << Value.Type;
    Ar << Value.BoolValue;
    Ar << Value.IntValue;
    Ar << Value.FloatValue;
    Ar << Value.StringValue;
    Ar << Value.Vec3Value;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FLuaScriptPropertyOverride& Override)
{
    Ar << Override.Name;
    Ar << Override.Value;
    return Ar;
}

bool ParseLuaScriptPropertyTypeName(const FString& TypeName, ELuaScriptPropertyType& OutType)
{
    // Lua properties 테이블의 type 문자열을 C++ enum으로 바꾸는 확장 지점입니다.
    const FString LowerType = ToLowerAscii(TypeName);
    if (LowerType == "bool" || LowerType == "boolean")
    {
        OutType = ELuaScriptPropertyType::Bool;
        return true;
    }
    if (LowerType == "int" || LowerType == "integer")
    {
        OutType = ELuaScriptPropertyType::Int;
        return true;
    }
    if (LowerType == "float" || LowerType == "number")
    {
        OutType = ELuaScriptPropertyType::Float;
        return true;
    }
    if (LowerType == "string")
    {
        OutType = ELuaScriptPropertyType::String;
        return true;
    }
    if (LowerType == "vec3" || LowerType == "vector3")
    {
        OutType = ELuaScriptPropertyType::Vec3;
        return true;
    }
    return false;
}

bool InferLuaScriptPropertyType(const sol::object& ValueObject, ELuaScriptPropertyType& OutType)
{
    // 값만 보고 오해 없이 판별 가능한 타입만 자동 추론합니다. table 기반 타입은 명시적인 type 선언을 권장합니다.
    switch (ValueObject.get_type())
    {
    case sol::type::boolean:
        OutType = ELuaScriptPropertyType::Bool;
        return true;
    case sol::type::number:
        OutType = ELuaScriptPropertyType::Float;
        return true;
    case sol::type::string:
        OutType = ELuaScriptPropertyType::String;
        return true;
    default:
        return false;
    }
}

FLuaScriptValue MakeDefaultLuaScriptValue(ELuaScriptPropertyType Type)
{
    FLuaScriptValue Value;
    Value.Type = Type;
    return Value;
}

bool ReadLuaVec3(const sol::object& ValueObject, FVector& OutValue)
{
    if (ValueObject.is<FVector>())
    {
        OutValue = ValueObject.as<FVector>();
        return true;
    }

    if (!ValueObject.is<sol::table>())
    {
        return false;
    }

    sol::table Table = ValueObject.as<sol::table>();
    sol::object XObject = Table["x"];
    sol::object YObject = Table["y"];
    sol::object ZObject = Table["z"];

    if (!XObject.valid() || XObject == sol::nil)
    {
        XObject = Table["X"];
    }
    if (!YObject.valid() || YObject == sol::nil)
    {
        YObject = Table["Y"];
    }
    if (!ZObject.valid() || ZObject == sol::nil)
    {
        ZObject = Table["Z"];
    }
    if (!XObject.valid() || XObject == sol::nil)
    {
        XObject = Table[1];
    }
    if (!YObject.valid() || YObject == sol::nil)
    {
        YObject = Table[2];
    }
    if (!ZObject.valid() || ZObject == sol::nil)
    {
        ZObject = Table[3];
    }

    if (XObject.get_type() != sol::type::number ||
        YObject.get_type() != sol::type::number ||
        ZObject.get_type() != sol::type::number)
    {
        return false;
    }

    OutValue.X = XObject.as<float>();
    OutValue.Y = YObject.as<float>();
    OutValue.Z = ZObject.as<float>();
    return true;
}

sol::table MakeLuaVec3(sol::state_view Lua, const FVector& Value)
{
    sol::table Vec = Lua.create_table();
    Vec[1] = Value.X;
    Vec[2] = Value.Y;
    Vec[3] = Value.Z;
    Vec["x"] = Value.X;
    Vec["y"] = Value.Y;
    Vec["z"] = Value.Z;
    return Vec;
}

bool ReadLuaVec4(const sol::object& ValueObject, FVector4& OutValue)
{
    if (ValueObject.is<FVector4>())
    {
        OutValue = ValueObject.as<FVector4>();
        return true;
    }

    if (!ValueObject.is<sol::table>())
    {
        return false;
    }

    sol::table Table = ValueObject.as<sol::table>();
    sol::object XObject = Table["x"];
    sol::object YObject = Table["y"];
    sol::object ZObject = Table["z"];
    sol::object WObject = Table["w"];

    if (!XObject.valid() || XObject == sol::nil) XObject = Table["X"];
    if (!YObject.valid() || YObject == sol::nil) YObject = Table["Y"];
    if (!ZObject.valid() || ZObject == sol::nil) ZObject = Table["Z"];
    if (!WObject.valid() || WObject == sol::nil) WObject = Table["W"];

    if (!XObject.valid() || XObject == sol::nil) XObject = Table[1];
    if (!YObject.valid() || YObject == sol::nil) YObject = Table[2];
    if (!ZObject.valid() || ZObject == sol::nil) ZObject = Table[3];
    if (!WObject.valid() || WObject == sol::nil) WObject = Table[4];

    if (!XObject.valid() || XObject == sol::nil || XObject.get_type() != sol::type::number ||
        !YObject.valid() || YObject == sol::nil || YObject.get_type() != sol::type::number ||
        !ZObject.valid() || ZObject == sol::nil || ZObject.get_type() != sol::type::number ||
        !WObject.valid() || WObject == sol::nil || WObject.get_type() != sol::type::number)
    {
        return false;
    }

    OutValue.X = XObject.as<float>();
    OutValue.Y = YObject.as<float>();
    OutValue.Z = ZObject.as<float>();
    OutValue.W = WObject.as<float>();
    return true;
}

sol::table MakeLuaVec4(sol::state_view Lua, const FVector4& Value)
{
    sol::table Vec = Lua.create_table();
    Vec[1] = Value.X;
    Vec[2] = Value.Y;
    Vec[3] = Value.Z;
    Vec[4] = Value.W;
    Vec["x"] = Value.X;
    Vec["y"] = Value.Y;
    Vec["z"] = Value.Z;
    Vec["w"] = Value.W;
    return Vec;
}

bool ReadLuaVec2(const sol::object& ValueObject, FVector2& OutValue)
{
    if (ValueObject.is<FVector2>())
    {
        OutValue = ValueObject.as<FVector2>();
        return true;
    }

    if (!ValueObject.is<sol::table>())
    {
        return false;
    }

    sol::table Table = ValueObject.as<sol::table>();
    sol::object XObject = Table["x"];
    sol::object YObject = Table["y"];

    if (!XObject.valid() || XObject == sol::nil)
    {
        XObject = Table["X"];
    }
    if (!YObject.valid() || YObject == sol::nil)
    {
        YObject = Table["Y"];
    }
    if (!XObject.valid() || XObject == sol::nil)
    {
        XObject = Table[1];
    }
    if (!YObject.valid() || YObject == sol::nil)
    {
        YObject = Table[2];
    }

    if (XObject.get_type() != sol::type::number ||
        YObject.get_type() != sol::type::number)
    {
        return false;
    }

    OutValue.X = XObject.as<float>();
    OutValue.Y = YObject.as<float>();
    return true;
}

sol::table MakeLuaVec2(sol::state_view Lua, const FVector2& Value)
{
    sol::table Vec = Lua.create_table();
    Vec[1] = Value.X;
    Vec[2] = Value.Y;
    Vec["x"] = Value.X;
    Vec["y"] = Value.Y;
    return Vec;
}

bool ReadLuaScriptValue(const sol::object& ValueObject, ELuaScriptPropertyType Type, FLuaScriptValue& OutValue)
{
    // Lua default 값을 FLuaScriptValue에 저장하는 확장 지점입니다.
    OutValue = MakeDefaultLuaScriptValue(Type);

    if (!ValueObject.valid() || ValueObject == sol::nil)
    {
        return true;
    }

    switch (Type)
    {
    case ELuaScriptPropertyType::Bool:
        if (ValueObject.get_type() != sol::type::boolean)
        {
            return false;
        }
        OutValue.BoolValue = ValueObject.as<bool>();
        return true;
    case ELuaScriptPropertyType::Int:
        if (ValueObject.get_type() != sol::type::number)
        {
            return false;
        }
        OutValue.IntValue = ValueObject.as<int32>();
        return true;
    case ELuaScriptPropertyType::Float:
        if (ValueObject.get_type() != sol::type::number)
        {
            return false;
        }
        OutValue.FloatValue = ValueObject.as<float>();
        return true;
    case ELuaScriptPropertyType::String:
        if (ValueObject.get_type() != sol::type::string)
        {
            return false;
        }
        OutValue.StringValue = ValueObject.as<FString>();
        return true;
    case ELuaScriptPropertyType::Vec3:
    {
        if (!ReadLuaVec3(ValueObject, OutValue.Vec3Value))
        {
            return false;
        }
        return true;
    }
    default:
        return false;
    }
}

void SetLuaScriptTableValue(sol::state& Lua, sol::table Table, const FString& Name, const FLuaScriptValue& Value)
{
    // ScriptComponent 인스턴스에 주입되는 최종 Lua 값 형태를 정하는 확장 지점입니다.
    switch (Value.Type)
    {
    case ELuaScriptPropertyType::Bool:
        Table[Name] = Value.BoolValue;
        break;
    case ELuaScriptPropertyType::Int:
        Table[Name] = Value.IntValue;
        break;
    case ELuaScriptPropertyType::Float:
        Table[Name] = Value.FloatValue;
        break;
    case ELuaScriptPropertyType::String:
        Table[Name] = Value.StringValue;
        break;
    case ELuaScriptPropertyType::Vec3:
    {
        Table[Name] = MakeLuaVec3(Lua, Value.Vec3Value);
        break;
    }
    default:
        break;
    }
}

EPropertyType GetLuaScriptEditorPropertyType(ELuaScriptPropertyType Type)
{
    // Lua 프로퍼티 타입을 Details 패널의 에디터 위젯 타입으로 연결하는 확장 지점입니다.
    switch (Type)
    {
    case ELuaScriptPropertyType::Bool:
        return EPropertyType::Bool;
    case ELuaScriptPropertyType::Int:
        return EPropertyType::Int;
    case ELuaScriptPropertyType::Float:
        return EPropertyType::Float;
    case ELuaScriptPropertyType::String:
        return EPropertyType::String;
    case ELuaScriptPropertyType::Vec3:
        return EPropertyType::Vec3;
    default:
        return EPropertyType::String;
    }
}

void* GetLuaScriptValuePtr(FLuaScriptValue& Value)
{
    // Details 패널이 직접 수정할 C++ 값의 주소를 넘기는 확장 지점입니다.
    switch (Value.Type)
    {
    case ELuaScriptPropertyType::Bool:
        return &Value.BoolValue;
    case ELuaScriptPropertyType::Int:
        return &Value.IntValue;
    case ELuaScriptPropertyType::Float:
        return &Value.FloatValue;
    case ELuaScriptPropertyType::String:
        return &Value.StringValue;
    case ELuaScriptPropertyType::Vec3:
        return Value.Vec3Value.Data;
    default:
        return nullptr;
    }
}
