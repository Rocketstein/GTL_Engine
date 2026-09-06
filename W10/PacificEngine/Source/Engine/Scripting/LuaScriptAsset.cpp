#include "LuaScriptAsset.h"

#include <fstream>
#include <sstream>
#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"

namespace
{
bool IsReservedLuaPropertyName(const FString& Name)
{
	return Name.empty() ||
		Name == "properties" ||
		Name == "BeginPlay" ||
		Name == "Tick" ||
		Name == "EndPlay" ||
		Name == "StartCoroutine" ||
		Name == "StopCoroutine" ||
		Name == "StopAllCoroutines";
}
} // namespace

FLuaScriptAsset::FLuaScriptAsset(FString InRelativePath, FString InFullPath)
	: RelativePath(std::move(InRelativePath))
	, FullPath(std::move(InFullPath))
{
}

bool FLuaScriptAsset::Load(sol::state& Lua)
{
	const bool bResult = LoadInternal(Lua);
	if (!bResult)
	{
		UE_LOG([Lua], Error, "Failed to load Lua script '%s': %s",
			FullPath.c_str(), LastError.c_str());
	}

	return bResult;
}

bool FLuaScriptAsset::ReloadIfChange(sol::state& Lua)
{
	if (!HasFileChanged())
	{
		return State == ELuaScriptAssetState::Loaded;
	}

	const bool bResult = LoadInternal(Lua);
	if (!bResult)
	{
		UE_LOG([Lua], Error, "Failed to reload Lua script '%s' : %s",
			FullPath.c_str(), LastError.c_str());
	}

	return bResult;
}

bool FLuaScriptAsset::HasFileChanged() const
{
	std::error_code Ec;
	const auto CurrentWriteTime = std::filesystem::last_write_time(FPaths::ToPath(FullPath), Ec);
	const bool bCurrentFileExists = !Ec;

	if (!bHasObservedFileState)
	{
		return true;
	}

	if (bCurrentFileExists != bLastFileExists)
	{
		return true;
	}

	if (!bCurrentFileExists)
	{
		return false;
	}

	return CurrentWriteTime != LastWriteTime;
}

sol::table FLuaScriptAsset::CreateInstance(sol::state& Lua, const TArray<FLuaScriptPropertyOverride>* PropertyOverrides) const
{
	sol::table Instance = Lua.create_table();

	if (!Prototype.valid())
	{
		return Instance;
	}

	for (const auto& Pair : Prototype)
	{
		Instance[Pair.first] = Pair.second;
	}

	for (const FLuaScriptPropertyDesc& Desc : PropertyDescriptors)
	{
		SetLuaScriptTableValue(Lua, Instance, Desc.Name, Desc.DefaultValue);
	}

	if (PropertyOverrides)
	{
		for (const FLuaScriptPropertyOverride& Override : *PropertyOverrides)
		{
			SetLuaScriptTableValue(Lua, Instance, Override.Name, Override.Value);
		}
	}

	Instance["__assetPath"] = RelativePath;
	Instance["__assetFullPath"] = FullPath;
	Instance["__assetVersion"] = Version;

	return Instance;
}

bool FLuaScriptAsset::LoadInternal(sol::state& Lua)
{
	LastError.clear();

	const std::filesystem::path ScriptFilePath = FPaths::ToPath(FullPath);
	std::ifstream File(ScriptFilePath, std::ios::binary);
	if (!File.is_open())
	{
		LastError = "Failed to open Lua script";
		std::error_code ExistsEc;
		const bool bExists = std::filesystem::exists(ScriptFilePath, ExistsEc);
		State = (!ExistsEc && !bExists) ? ELuaScriptAssetState::Missing : ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	FString NewSourceCode = Buffer.str();

	const FString ChunkName = "@" + RelativePath;

	sol::environment Env(Lua, sol::create, Lua.globals());
	sol::load_result Loaded = Lua.load(NewSourceCode, ChunkName);

	if (!Loaded.valid())
	{
		sol::error Err = Loaded;
		LastError = Err.what();
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::protected_function Function = Loaded;
	
	if (!sol::set_environment(Env, Function))
	{
		LastError = "Failed to set Lua environment";
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::protected_function_result Result = Function();

	if (!Result.valid())
	{
		sol::error Err = Result;
		LastError = Err.what();
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::object Returned = Result.get<sol::object>();
	if (!Returned.is<sol::table>())
	{
		LastError = "Lua script must return a table";
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::table NewPrototype = Returned.as<sol::table>();
	TArray<FLuaScriptPropertyDesc> NewPropertyDescriptors;
	if (!ParsePropertyDescriptors(NewPrototype, NewPropertyDescriptors))
	{
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	SourceCode = std::move(NewSourceCode);
	Prototype = NewPrototype;
	PropertyDescriptors = std::move(NewPropertyDescriptors);
	UpdateObservedFileState();
	++Version;
	State = ELuaScriptAssetState::Loaded;

	UE_LOG([Lua], Info, "Loaded Lua script: %s", RelativePath.c_str());
	return true;
}

bool FLuaScriptAsset::UpdateObservedFileState()
{
	std::error_code Ec;
	LastWriteTime = std::filesystem::last_write_time(FPaths::ToPath(FullPath), Ec);
	bHasObservedFileState = true;
	bLastFileExists = !Ec;
	if (!bLastFileExists)
	{
		LastWriteTime = {};
	}
	return bLastFileExists;
}

bool FLuaScriptAsset::ParsePropertyDescriptors(sol::table ScriptTable, TArray<FLuaScriptPropertyDesc>& OutDescriptors)
{
	OutDescriptors.clear();

	sol::object PropertiesObject = ScriptTable["properties"];
	if (!PropertiesObject.valid() || PropertiesObject == sol::nil)
	{
		return true;
	}

	if (!PropertiesObject.is<sol::table>())
	{
		LastError = "Lua script 'properties' must be a table";
		return false;
	}

	sol::table PropertiesTable = PropertiesObject.as<sol::table>();
	for (const auto& Pair : PropertiesTable)
	{
		sol::object KeyObject = Pair.first;
		if (!KeyObject.is<FString>())
		{
			LastError = "Lua script property names must be strings";
			return false;
		}

		FLuaScriptPropertyDesc Desc;
		Desc.Name = KeyObject.as<FString>();
		if (IsReservedLuaPropertyName(Desc.Name))
		{
			LastError = "Lua script property name is reserved: " + Desc.Name;
			return false;
		}

		sol::object DescriptorObject = Pair.second;
		if (DescriptorObject.is<sol::table>())
		{
			sol::table DescriptorTable = DescriptorObject.as<sol::table>();

			sol::object TypeObject = DescriptorTable["type"];
			sol::object DefaultObject = DescriptorTable["default"];
			if (TypeObject.valid() && TypeObject != sol::nil)
			{
				if (!TypeObject.is<FString>() || !ParseLuaScriptPropertyTypeName(TypeObject.as<FString>(), Desc.Type))
				{
					LastError = "Lua script property has unsupported type: " + Desc.Name;
					return false;
				}
			}
			else if (!InferLuaScriptPropertyType(DefaultObject, Desc.Type))
			{
				LastError = "Lua script property needs a type or default value: " + Desc.Name;
				return false;
			}

			if (!ReadLuaScriptValue(DefaultObject, Desc.Type, Desc.DefaultValue))
			{
				LastError = "Lua script property default value does not match type: " + Desc.Name;
				return false;
			}

			sol::object MinObject = DescriptorTable["min"];
			sol::object MaxObject = DescriptorTable["max"];
			sol::object SpeedObject = DescriptorTable["speed"];
			if (MinObject.valid() && MinObject.get_type() == sol::type::number)
			{
				Desc.Min = MinObject.as<float>();
			}
			if (MaxObject.valid() && MaxObject.get_type() == sol::type::number)
			{
				Desc.Max = MaxObject.as<float>();
			}
			if (SpeedObject.valid() && SpeedObject.get_type() == sol::type::number)
			{
				Desc.Speed = SpeedObject.as<float>();
			}
		}
		else
		{
			if (!InferLuaScriptPropertyType(DescriptorObject, Desc.Type))
			{
				LastError = "Lua script property descriptor must be a supported value or table: " + Desc.Name;
				return false;
			}
			if (!ReadLuaScriptValue(DescriptorObject, Desc.Type, Desc.DefaultValue))
			{
				LastError = "Lua script property value is invalid: " + Desc.Name;
				return false;
			}
		}

		OutDescriptors.push_back(std::move(Desc));
	}

	return true;
}
