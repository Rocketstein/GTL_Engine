#include "Resource/ResourceManager.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <d3d11.h>
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "Core/AsciiUtils.h"
#include "Core/Log.h"
#include "Profiling/MemoryStats.h"
#include "Engine/Texture/Texture2D.h"
#include "Materials/MaterialManager.h"


// SEH-guarded WIC 호출 (C++ 객체 unwind가 없는 별도 함수여야 __try/__except 사용 가능).
static HRESULT SafeCreateWICTextureFromFile(
	ID3D11Device* Device,
	const wchar_t* FullPath,
	ID3D11ShaderResourceView** OutSRV)
{
	HRESULT hr = E_FAIL;
	__try
	{
		hr = DirectX::CreateWICTextureFromFileEx(
			Device,
			FullPath,
			0,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_SHADER_RESOURCE,
			0, 0,
			DirectX::WIC_LOADER_FORCE_RGBA32,
			nullptr,
			OutSRV);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		hr = E_FAIL;
		if (OutSRV) { *OutSRV = nullptr; }
	}
	return hr;
}

namespace ResourceKey
{
	constexpr const char* Font     = "Font";
	constexpr const char* Particle = "Particle";
	constexpr const char* Texture  = "Texture";
	constexpr const char* Mesh     = "Mesh";
	constexpr const char* Sound    = "Sound";
	constexpr const char* Material = "Material";
	constexpr const char* PathMap  = "Path";
	constexpr const char* Path     = "Path";
	constexpr const char* Category = "Category";
	constexpr const char* Columns  = "Columns";
	constexpr const char* Rows     = "Rows";
}

namespace
{
	bool SafeJsonBool(const json::JSON& Value, bool Fallback = false)
	{
		bool bBool = false;
		const bool BoolValue = Value.ToBool(bBool);
		return bBool ? BoolValue : Fallback;
	}

	uint32 SafeJsonUInt(const json::JSON& Value, uint32 Fallback = 0)
	{
		bool bInt = false;
		const long IntValue = Value.ToInt(bInt);
		if (bInt)
		{
			return static_cast<uint32>(IntValue);
		}

		bool bFloat = false;
		const double FloatValue = Value.ToFloat(bFloat);
		if (bFloat)
		{
			return static_cast<uint32>(FloatValue);
		}

		return Fallback;
	}

	float SafeJsonFloat(const json::JSON& Value, float Fallback = 0.0f)
	{
		bool bFloat = false;
		const double FloatValue = Value.ToFloat(bFloat);
		if (bFloat)
		{
			return static_cast<float>(FloatValue);
		}

		bool bInt = false;
		const long IntValue = Value.ToInt(bInt);
		if (bInt)
		{
			return static_cast<float>(IntValue);
		}

		return Fallback;
	}

	FString ToLowerCopy(FString Value)
	{
		AsciiUtils::ToLowerInPlace(Value);
		return Value;
	}

	FString NormalizeGenericPath(FString Value)
	{
		for (char& C : Value)
		{
			if (C == '\\')
			{
				C = '/';
			}
		}
		return ToLowerCopy(Value);
	}

	FString SanitizeResourceToken(const FString& Value)
	{
		FString Result;
		Result.reserve(Value.size());

		bool bLastWasDot = false;
		for (const char C : Value)
		{
			if (AsciiUtils::IsAlnum(C))
			{
				Result.push_back(C);
				bLastWasDot = false;
			}
			else if (!bLastWasDot)
			{
				Result.push_back('.');
				bLastWasDot = true;
			}
		}

		while (!Result.empty() && Result.front() == '.')
		{
			Result.erase(Result.begin());
		}
		while (!Result.empty() && Result.back() == '.')
		{
			Result.pop_back();
		}

		return Result;
	}

	FString MakeGeneratedResourceName(const FString& ResourceType, const FString& RuleName, const FString& RelativePathWithoutExtension)
	{
		const FString SanitizedRelativePath = SanitizeResourceToken(RelativePathWithoutExtension);
		if (!SanitizedRelativePath.empty())
		{
			return ResourceType + "." + RuleName + "." + SanitizedRelativePath;
		}

		return ResourceType + "." + RuleName;
	}

	std::filesystem::path ToLexicalPath(const FString& Utf8Path)
	{
		return std::filesystem::path(FPaths::ToWide(Utf8Path)).lexically_normal();
	}

	FString MakeRelativeUtf8Path(const std::filesystem::path& FullPath, const std::filesystem::path& RootPath)
	{
		return FPaths::ToUtf8(FullPath.lexically_normal().lexically_relative(RootPath).generic_wstring());
	}

	ESoundCategory InferSoundCategory(const FString& ResourceName, const FString& ResourcePath)
	{
		const FString LowerName = ToLowerCopy(ResourceName);
		const FString LowerPath = ToLowerCopy(ResourcePath);
		if (LowerName.find("background") != FString::npos
			|| LowerName.find("bgm") != FString::npos
			|| LowerPath.find("/background/") != FString::npos
			|| LowerPath.find("\\background\\") != FString::npos)
		{
			return ESoundCategory::Background;
		}

		return ESoundCategory::SFX;
	}
}

void FResourceManager::LoadFromFile(const FString& Path, ID3D11Device* InDevice)
{
	using namespace json;

	UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] Resource file load begin: %s", Path.c_str());

	const size_t PrevFontCount = FontResources.size();
	const size_t PrevParticleCount = ParticleResources.size();
	const size_t PrevTextureCount = TextureResources.size();
	const size_t PrevMeshCount = MeshResources.size();
	const size_t PrevSoundCount = SoundResources.size();
	const size_t PrevMaterialCount = MaterialResources.size();
	const size_t PrevPathCount = PathResources.size();

	const std::filesystem::path NormalizedInputPath = std::filesystem::path(FPaths::ToWide(Path)).lexically_normal();
	const std::filesystem::path NormalizedEditorResourcePath = std::filesystem::path(FPaths::EditorResourceFilePath()).lexically_normal();
	const bool bIsEditorResourceFile = (NormalizedInputPath == NormalizedEditorResourcePath);

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		UE_LOG_CATEGORY(ResourceManager, Warning, "[INIT] Resource file open failed: %s", Path.c_str());
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	// Font — { "Name": { "Path": "...", "Columns": 16, "Rows": 16 } }
	if (Root.hasKey(ResourceKey::Font))
	{
		JSON FontSection = Root[ResourceKey::Font];
		for (auto& Pair : FontSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FFontResource Resource;
			Resource.Name    = FName(Pair.first.c_str());
			Resource.Path    = Entry[ResourceKey::Path].ToString();
			Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
			Resource.Rows    = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
			Resource.SRV     = nullptr;
			FontResources[Pair.first] = Resource;
		}
	}

	// Particle — { "Name": { "Path": "...", "Columns": 6, "Rows": 6 } }
	if (Root.hasKey(ResourceKey::Particle))
	{
		JSON ParticleSection = Root[ResourceKey::Particle];
		for (auto& Pair : ParticleSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FParticleResource Resource;
			Resource.Name    = FName(Pair.first.c_str());
			Resource.Path    = Entry[ResourceKey::Path].ToString();
			Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
			Resource.Rows    = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
			Resource.SRV     = nullptr;
			ParticleResources[Pair.first] = Resource;
		}
	}

	// Texture — { "Name": { "Path": "..." } }  (Columns/Rows는 항상 1)
	if (Root.hasKey(ResourceKey::Texture))
	{
		JSON TextureSection = Root[ResourceKey::Texture];
		for (auto& Pair : TextureSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FTextureResource Resource;
			Resource.Name    = FName(Pair.first.c_str());
			Resource.Path    = Entry[ResourceKey::Path].ToString();
			Resource.Columns = 1;
			Resource.Rows    = 1;
			Resource.SRV     = nullptr;
			Resource.bEditorResource = bIsEditorResourceFile;
			TextureResources[Pair.first] = Resource;
		}
	}

	// Mesh — { "Name": { "Path": "..." } }  (경로 레지스트리 전용)
	if (Root.hasKey(ResourceKey::Mesh))
	{
		JSON MeshSection = Root[ResourceKey::Mesh];
		for (auto& Pair : MeshSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FMeshResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			MeshResources[Pair.first] = Resource;
		}
	}

	// Sound — { "Name": { "Path": "..." } }  (경로 레지스트리 전용)
	if (Root.hasKey(ResourceKey::Sound))
	{
		JSON SoundSection = Root[ResourceKey::Sound];
		for (auto& Pair : SoundSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FSoundResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			Resource.Category = InferSoundCategory(Pair.first, Resource.Path);
			if (Entry.hasKey(ResourceKey::Category))
			{
				const FString CategoryText = ToLowerCopy(Entry[ResourceKey::Category].ToString());
				if (CategoryText == "background" || CategoryText == "bgm")
				{
					Resource.Category = ESoundCategory::Background;
				}
				else if (CategoryText == "sfx")
				{
					Resource.Category = ESoundCategory::SFX;
				}
			}
			SoundResources[Pair.first] = Resource;
		}
	}

	// Material — { "Name": { "Path": "..." } }  (경로 레지스트리 + 기본 프리로드)
	if (Root.hasKey(ResourceKey::Material))
	{
		JSON MaterialSection = Root[ResourceKey::Material];
		for (auto& Pair : MaterialSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FMaterialResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			MaterialResources[Pair.first] = Resource;

			FGenericPathResource PathResource;
			PathResource.Name = Resource.Name;
			PathResource.Path = Resource.Path;
			PathResources[Pair.first] = PathResource;
		}
	}

	if (Root.hasKey(ResourceKey::PathMap))
	{
		JSON PathSection = Root[ResourceKey::PathMap];
		for (auto& Pair : PathSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FGenericPathResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			PathResources[Pair.first] = Resource;
		}
	}

	DiscoverBitmapFonts("Asset/Content/Font");

	UE_LOG_CATEGORY(
		ResourceManager,
		Info,
		"[INIT] Parsed resource file: %s | +Font=%zu +Particle=%zu +Texture=%zu +Mesh=%zu +Sound=%zu +Material=%zu +Path=%zu | Total Font=%zu Particle=%zu Texture=%zu Mesh=%zu Sound=%zu Material=%zu Path=%zu",
		Path.c_str(),
		FontResources.size() - PrevFontCount,
		ParticleResources.size() - PrevParticleCount,
		TextureResources.size() - PrevTextureCount,
		MeshResources.size() - PrevMeshCount,
		SoundResources.size() - PrevSoundCount,
		MaterialResources.size() - PrevMaterialCount,
		PathResources.size() - PrevPathCount,
		FontResources.size(),
		ParticleResources.size(),
		TextureResources.size(),
		MeshResources.size(),
		SoundResources.size(),
		MaterialResources.size(),
		PathResources.size());

	if (LoadGPUResources(InDevice))
	{
		UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] GPU resource upload complete for: %s", Path.c_str());
	}
	else
	{
		UE_LOG_CATEGORY(ResourceManager, Error, "[INIT] GPU resource upload failed for: %s", Path.c_str());
	}

	size_t PreloadedMaterialCount = 0;
	for (const auto& [Key, Resource] : MaterialResources)
	{
		FMaterialManager::Get().GetOrCreateMaterial(Resource.Path);
		++PreloadedMaterialCount;
	}

	UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] Material preload complete: %zu material(s)", PreloadedMaterialCount);
}

void FResourceManager::DiscoverBitmapFonts(const FString& DirectoryPath)
{
	const std::filesystem::path RootPath(FPaths::RootDir());
	const std::filesystem::path FontRoot = RootPath / FPaths::ToWide(DirectoryPath);
	if (!std::filesystem::exists(FontRoot))
	{
		UE_LOG_CATEGORY(ResourceManager, Warning, "[INIT] Bitmap font directory not found: %s", DirectoryPath.c_str());
		return;
	}

	size_t DiscoveredFontCount = 0;
	for (const auto& Entry : std::filesystem::recursive_directory_iterator(FontRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		FString Extension = Entry.path().extension().string();
		AsciiUtils::ToLowerInPlace(Extension);
		if (Extension != ".json")
		{
			continue;
		}

		FFontResource Resource;
		if (!LoadBitmapFontMetadata(FPaths::ToUtf8(Entry.path().lexically_normal().wstring()), Resource))
		{
			continue;
		}

		const FString FontKey = Resource.Name.ToString();
		if (FontKey.empty() || FontResources.find(FontKey) != FontResources.end())
		{
			continue;
		}

		FontResources[FontKey] = std::move(Resource);
		++DiscoveredFontCount;
	}

	UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] Bitmap font discovery complete: dir=%s discovered=%zu total=%zu",
		DirectoryPath.c_str(),
		DiscoveredFontCount,
		FontResources.size());
}

bool FResourceManager::LoadBitmapFontMetadata(const FString& JsonPath, FFontResource& OutResource) const
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(JsonPath)));
	if (!File.is_open())
	{
		return false;
	}

	FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(Content);
	if (!Root.hasKey("common") || !Root.hasKey("pages") || !Root.hasKey("chars"))
	{
		return false;
	}

	JSON Common = Root["common"];
	JSON Pages = Root["pages"];
	bool bHasPage = false;
	FString PageFile;
	for (auto& PageValue : Pages.ArrayRange())
	{
		PageFile = PageValue.ToString();
		bHasPage = true;
		break;
	}
	if (!bHasPage)
	{
		return false;
	}

	FString FontName = std::filesystem::path(JsonPath).stem().string();
	if (Root.hasKey("info") && Root["info"].hasKey("face"))
	{
		FontName = Root["info"]["face"].ToString();
	}

	const std::filesystem::path JsonFilePath(FPaths::ToWide(JsonPath));
	const std::filesystem::path ImagePath = JsonFilePath.parent_path() / FPaths::ToWide(PageFile);
	if (!std::filesystem::exists(ImagePath))
	{
		return false;
	}

	const std::filesystem::path RootPath(FPaths::RootDir());
	const std::filesystem::path RelativeImagePath = ImagePath.lexically_normal().lexically_relative(RootPath);

	OutResource = {};
	OutResource.Name = FName(FontName);
	OutResource.Path = FPaths::ToUtf8(RelativeImagePath.generic_wstring());
	OutResource.Columns = 1;
	OutResource.Rows = 1;
	OutResource.AtlasWidth = SafeJsonUInt(Common["scaleW"]);
	OutResource.AtlasHeight = SafeJsonUInt(Common["scaleH"]);
	OutResource.LineHeight = SafeJsonFloat(Common["lineHeight"], 1.0f);
	OutResource.Base = SafeJsonFloat(Common["base"], OutResource.LineHeight);
	OutResource.bHasGlyphMetrics = OutResource.AtlasWidth > 0 && OutResource.AtlasHeight > 0;

	if (!OutResource.bHasGlyphMetrics)
	{
		return false;
	}

	for (auto& CharValue : Root["chars"].ArrayRange())
	{
		const uint32 Codepoint = SafeJsonUInt(CharValue["id"]);
		FFontGlyph Glyph;
		const float X = SafeJsonFloat(CharValue["x"]);
		const float Y = SafeJsonFloat(CharValue["y"]);
		Glyph.Width = SafeJsonFloat(CharValue["width"]);
		Glyph.Height = SafeJsonFloat(CharValue["height"]);
		Glyph.XOffset = SafeJsonFloat(CharValue["xoffset"]);
		Glyph.YOffset = SafeJsonFloat(CharValue["yoffset"]);
		Glyph.XAdvance = SafeJsonFloat(CharValue["xadvance"]);
		Glyph.U0 = X / static_cast<float>(OutResource.AtlasWidth);
		Glyph.V0 = Y / static_cast<float>(OutResource.AtlasHeight);
		Glyph.U1 = (X + Glyph.Width) / static_cast<float>(OutResource.AtlasWidth);
		Glyph.V1 = (Y + Glyph.Height) / static_cast<float>(OutResource.AtlasHeight);
		OutResource.Glyphs[Codepoint] = Glyph;
	}

	if (Root.hasKey("kernings"))
	{
		for (auto& KerningValue : Root["kernings"].ArrayRange())
		{
			const uint32 First = SafeJsonUInt(KerningValue["first"]);
			const uint32 Second = SafeJsonUInt(KerningValue["second"]);
			const uint64 Key = (static_cast<uint64>(First) << 32) | static_cast<uint64>(Second);
			OutResource.Kernings[Key] = SafeJsonFloat(KerningValue["amount"]);
		}
	}

	return !OutResource.Glyphs.empty();
}

void FResourceManager::LoadFromDirectory(const FString& Path, ID3D11Device* InDevice)
{
	const std::filesystem::path RootPath(FPaths::RootDir());
	UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] Directory texture scan begin: %s", Path.c_str());

	size_t ScannedPngCount = 0;
	size_t LoadedTextureCount = 0;

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(FPaths::ToWide(Path)))
	{
		FString Extension = Entry.path().extension().string();
		AsciiUtils::ToLowerInPlace(Extension);
		if (Extension != ".png")
			continue;

		++ScannedPngCount;
		const FString RelativePath = FPaths::ToUtf8(Entry.path().lexically_normal().lexically_relative(RootPath).generic_wstring());
		if (UTexture2D* Texture = UTexture2D::LoadFromFile(RelativePath, InDevice))
		{
			LoadedResource[RelativePath] = Texture->GetSRV();
			++LoadedTextureCount;
		}
	}

	UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] Directory texture scan complete: scanned=%zu loaded=%zu cached=%zu",
		ScannedPngCount,
		LoadedTextureCount,
		LoadedResource.size());
}

void FResourceManager::LoadFromScanFile(const FString& Path, ID3D11Device* InDevice)
{
	using namespace json;

	UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] Resource scan file load begin: %s", Path.c_str());

	const size_t PrevFontCount = FontResources.size();
	const size_t PrevParticleCount = ParticleResources.size();
	const size_t PrevTextureCount = TextureResources.size();
	const size_t PrevMeshCount = MeshResources.size();
	const size_t PrevSoundCount = SoundResources.size();
	const size_t PrevMaterialCount = MaterialResources.size();
	const size_t PrevPathCount = PathResources.size();

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		UE_LOG_CATEGORY(ResourceManager, Warning, "[INIT] Resource scan file open failed: %s", Path.c_str());
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(Content);
	if (!Root.hasKey("ScanRules"))
	{
		UE_LOG_CATEGORY(ResourceManager, Warning, "[INIT] Resource scan file missing ScanRules: %s", Path.c_str());
		return;
	}

	bool bRecursive = true;
	bool bNormalizeExtensionToLower = true;
	bool bUseRelativePathFromRoot = true;
	if (Root.hasKey("Options"))
	{
		JSON Options = Root["Options"];
		if (Options.hasKey("Recursive"))
		{
			bRecursive = SafeJsonBool(Options["Recursive"], true);
		}
		if (Options.hasKey("NormalizeExtensionToLower"))
		{
			bNormalizeExtensionToLower = SafeJsonBool(Options["NormalizeExtensionToLower"], true);
		}
		if (Options.hasKey("UseRelativePathFromRoot"))
		{
			bUseRelativePathFromRoot = SafeJsonBool(Options["UseRelativePathFromRoot"], true);
		}
	}

	const std::filesystem::path RootPath(FPaths::RootDir());
	JSON ScanRules = Root["ScanRules"];
	for (auto& TypePair : ScanRules.ObjectRange())
	{
		const FString ResourceType = TypePair.first;
		JSON RuleSection = TypePair.second;
		for (auto& RulePair : RuleSection.ObjectRange())
		{
			const FString RuleName = RulePair.first;
			JSON Rule = RulePair.second;
			if (!Rule.hasKey("Directory") || !Rule.hasKey("Extensions"))
			{
				continue;
			}

			const FString DirectoryUtf8 = Rule["Directory"].ToString();
			const std::filesystem::path RuleRootPath = RootPath / ToLexicalPath(DirectoryUtf8);
			if (!std::filesystem::exists(RuleRootPath) || !std::filesystem::is_directory(RuleRootPath))
			{
				UE_LOG_CATEGORY(ResourceManager, Warning, "[INIT] Resource scan directory missing: type=%s rule=%s dir=%s",
					ResourceType.c_str(),
					RuleName.c_str(),
					DirectoryUtf8.c_str());
				continue;
			}

			std::unordered_set<FString> AllowedExtensions;
			for (auto& ExtValue : Rule["Extensions"].ArrayRange())
			{
				FString Extension = ExtValue.ToString();
				if (bNormalizeExtensionToLower)
				{
					Extension = ToLowerCopy(Extension);
				}
				AllowedExtensions.insert(Extension);
			}

			std::unordered_map<FString, FString> Aliases;
			if (Rule.hasKey("Aliases"))
			{
				for (auto& AliasPair : Rule["Aliases"].ObjectRange())
				{
					Aliases[NormalizeGenericPath(AliasPair.first)] = AliasPair.second.ToString();
				}
			}

			ESoundCategory SoundCategory = ESoundCategory::SFX;
			if (Rule.hasKey("Category"))
			{
				const FString CategoryText = ToLowerCopy(Rule["Category"].ToString());
				if (CategoryText == "background" || CategoryText == "bgm")
				{
					SoundCategory = ESoundCategory::Background;
				}
			}

			std::filesystem::directory_options IteratorOptions = std::filesystem::directory_options::skip_permission_denied;
			if (bRecursive)
			{
				for (const auto& Entry : std::filesystem::recursive_directory_iterator(RuleRootPath, IteratorOptions))
				{
					if (!Entry.is_regular_file())
					{
						continue;
					}

					FString Extension = Entry.path().extension().string();
					if (bNormalizeExtensionToLower)
					{
						Extension = ToLowerCopy(Extension);
					}
					if (AllowedExtensions.find(Extension) == AllowedExtensions.end())
					{
						continue;
					}

					const std::filesystem::path RelativeToRule = Entry.path().lexically_normal().lexically_relative(RuleRootPath);
					const std::filesystem::path RelativeNoExtension = RelativeToRule.parent_path() / RelativeToRule.stem();
					const FString RelativeAliasKey = NormalizeGenericPath(RelativeNoExtension.generic_string());
					const FString RelativePathUtf8 = bUseRelativePathFromRoot
						? MakeRelativeUtf8Path(Entry.path(), RootPath)
						: FPaths::ToUtf8(Entry.path().lexically_normal().generic_wstring());

					FString ResourceName;
					if (const auto AliasIt = Aliases.find(RelativeAliasKey); AliasIt != Aliases.end())
					{
						ResourceName = AliasIt->second;
					}
					else
					{
						ResourceName = MakeGeneratedResourceName(ResourceType, RuleName, RelativeNoExtension.generic_string());
					}

					if (ResourceType == ResourceKey::Font)
					{
						if (Extension == ".json")
						{
							FFontResource Resource;
							if (!LoadBitmapFontMetadata(FPaths::ToUtf8(Entry.path().lexically_normal().wstring()), Resource))
							{
								continue;
							}
							if (!ResourceName.empty())
							{
								Resource.Name = FName(ResourceName);
							}
							FontResources[Resource.Name.ToString()] = std::move(Resource);
						}
						else
						{
							RegisterPath(FName(ResourceName), RelativePathUtf8);
						}
					}
					else if (ResourceType == ResourceKey::Particle)
					{
						RegisterParticle(FName(ResourceName), RelativePathUtf8);
					}
					else if (ResourceType == ResourceKey::Texture)
					{
						RegisterTexture(FName(ResourceName), RelativePathUtf8);
					}
					else if (ResourceType == ResourceKey::Mesh)
					{
						RegisterMesh(FName(ResourceName), RelativePathUtf8);
					}
					else if (ResourceType == ResourceKey::Sound)
					{
						RegisterSound(FName(ResourceName), RelativePathUtf8, SoundCategory);
					}
					else if (ResourceType == ResourceKey::Material)
					{
						RegisterMaterial(FName(ResourceName), RelativePathUtf8);
					}
					else
					{
						RegisterPath(FName(ResourceName), RelativePathUtf8);
					}
				}
			}
			else
			{
				for (const auto& Entry : std::filesystem::directory_iterator(RuleRootPath, IteratorOptions))
				{
					if (!Entry.is_regular_file())
					{
						continue;
					}

					FString Extension = Entry.path().extension().string();
					if (bNormalizeExtensionToLower)
					{
						Extension = ToLowerCopy(Extension);
					}
					if (AllowedExtensions.find(Extension) == AllowedExtensions.end())
					{
						continue;
					}

					const std::filesystem::path RelativeToRule = Entry.path().lexically_normal().lexically_relative(RuleRootPath);
					const std::filesystem::path RelativeNoExtension = RelativeToRule.parent_path() / RelativeToRule.stem();
					const FString RelativeAliasKey = NormalizeGenericPath(RelativeNoExtension.generic_string());
					const FString RelativePathUtf8 = bUseRelativePathFromRoot
						? MakeRelativeUtf8Path(Entry.path(), RootPath)
						: FPaths::ToUtf8(Entry.path().lexically_normal().generic_wstring());

					FString ResourceName;
					if (const auto AliasIt = Aliases.find(RelativeAliasKey); AliasIt != Aliases.end())
					{
						ResourceName = AliasIt->second;
					}
					else
					{
						ResourceName = MakeGeneratedResourceName(ResourceType, RuleName, RelativeNoExtension.generic_string());
					}

					if (ResourceType == ResourceKey::Particle)
					{
						RegisterParticle(FName(ResourceName), RelativePathUtf8);
					}
					else if (ResourceType == ResourceKey::Texture)
					{
						RegisterTexture(FName(ResourceName), RelativePathUtf8);
					}
					else if (ResourceType == ResourceKey::Mesh)
					{
						RegisterMesh(FName(ResourceName), RelativePathUtf8);
					}
					else if (ResourceType == ResourceKey::Sound)
					{
						RegisterSound(FName(ResourceName), RelativePathUtf8, SoundCategory);
					}
					else if (ResourceType == ResourceKey::Material)
					{
						RegisterMaterial(FName(ResourceName), RelativePathUtf8);
					}
					else
					{
						RegisterPath(FName(ResourceName), RelativePathUtf8);
					}
				}
			}
		}
	}

	UE_LOG_CATEGORY(
		ResourceManager,
		Info,
		"[INIT] Parsed resource scan file: %s | +Font=%zu +Particle=%zu +Texture=%zu +Mesh=%zu +Sound=%zu +Material=%zu +Path=%zu | Total Font=%zu Particle=%zu Texture=%zu Mesh=%zu Sound=%zu Material=%zu Path=%zu",
		Path.c_str(),
		FontResources.size() - PrevFontCount,
		ParticleResources.size() - PrevParticleCount,
		TextureResources.size() - PrevTextureCount,
		MeshResources.size() - PrevMeshCount,
		SoundResources.size() - PrevSoundCount,
		MaterialResources.size() - PrevMaterialCount,
		PathResources.size() - PrevPathCount,
		FontResources.size(),
		ParticleResources.size(),
		TextureResources.size(),
		MeshResources.size(),
		SoundResources.size(),
		MaterialResources.size(),
		PathResources.size());

	if (LoadGPUResources(InDevice))
	{
		UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] GPU resource upload complete for scan file: %s", Path.c_str());
	}
	else
	{
		UE_LOG_CATEGORY(ResourceManager, Error, "[INIT] GPU resource upload failed for scan file: %s", Path.c_str());
	}

	size_t PreloadedMaterialCount = 0;
	for (const auto& [Key, Resource] : MaterialResources)
	{
		FMaterialManager::Get().GetOrCreateMaterial(Resource.Path);
		++PreloadedMaterialCount;
	}

	UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] Material preload complete after scan file: %zu material(s)", PreloadedMaterialCount);
}

bool FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
	if (!Device)
	{
		UE_LOG_CATEGORY(ResourceManager, Error, "[INIT] LoadGPUResources failed: device is null");
		return false;
	}

	auto LoadSRV = [&](FTextureAtlasResource& Resource) -> bool
	{
		if (Resource.SRV)
		{
			if (Resource.TrackedMemoryBytes > 0)
			{
				MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
				Resource.TrackedMemoryBytes = 0;
			}
			Resource.SRV->Release();
			Resource.SRV = nullptr;
		}

		std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Resource.Path));

		// 확장자에 따라 DDS / WIC 로더 분기
		std::filesystem::path Ext = std::filesystem::path(Resource.Path).extension();
		FString ExtStr = Ext.string();
		AsciiUtils::ToLowerInPlace(ExtStr);

		HRESULT hr;
		if (ExtStr == ".dds")
		{
			hr = DirectX::CreateDDSTextureFromFileEx(
				Device,
				FullPath.c_str(),
				0,
				D3D11_USAGE_IMMUTABLE,
				D3D11_BIND_SHADER_RESOURCE,
				0, 0,
				DirectX::DDS_LOADER_DEFAULT,
				nullptr,
				&Resource.SRV
			);
		}
		else
		{
			// .png/.jpg/.bmp/.tga 등 — WIC 경유 (WIC 팩토리 손상 시 AV가 나도 게임 전체가 죽지 않도록 SEH 가드)
			hr = SafeCreateWICTextureFromFile(Device, FullPath.c_str(), &Resource.SRV);
			if (FAILED(hr))
			{
				UE_LOG_CATEGORY(ResourceManager, Error, "[INIT] WIC load failed (path=%s)", Resource.Path.c_str());
			}
		}
		if (FAILED(hr) || !Resource.SRV)
		{
			return false;
		}

		ID3D11Resource* TextureResource = nullptr;
		Resource.SRV->GetResource(&TextureResource);
		Resource.TrackedMemoryBytes = MemoryStats::CalculateTextureMemory(TextureResource);
		if (TextureResource)
		{
			TextureResource->Release();
		}

		if (Resource.TrackedMemoryBytes > 0)
		{
			MemoryStats::AddTextureMemory(Resource.TrackedMemoryBytes);
		}

		return true;
	};

	size_t UploadedFontCount = 0;
	for (auto& [Key, Resource] : FontResources)
	{
		if (!LoadSRV(Resource))
		{
			UE_LOG_CATEGORY(ResourceManager, Error, "[INIT] Font GPU upload failed: key=%s path=%s", Key.c_str(), Resource.Path.c_str());
			return false;
		}
		++UploadedFontCount;
	}

	size_t UploadedParticleCount = 0;
	for (auto& [Key, Resource] : ParticleResources)
	{
		if (!LoadSRV(Resource))
		{
			UE_LOG_CATEGORY(ResourceManager, Error, "[INIT] Particle GPU upload failed: key=%s path=%s", Key.c_str(), Resource.Path.c_str());
			return false;
		}
		++UploadedParticleCount;
	}

	size_t UploadedTextureCount = 0;
	for (auto& [Key, Resource] : TextureResources)
	{
		if (!LoadSRV(Resource))
		{
			UE_LOG_CATEGORY(ResourceManager, Error, "[INIT] Texture GPU upload failed: key=%s path=%s", Key.c_str(), Resource.Path.c_str());
			return false;
		}
		++UploadedTextureCount;
	}

	UE_LOG_CATEGORY(ResourceManager, Info, "[INIT] GPU upload summary: Font=%zu Particle=%zu Texture=%zu",
		UploadedFontCount,
		UploadedParticleCount,
		UploadedTextureCount);

	return true;
}

void FResourceManager::ReleaseGPUResources()
{
	for (auto& [Key, Resource] : FontResources)
	{
		if (Resource.TrackedMemoryBytes > 0)
		{
			MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
			Resource.TrackedMemoryBytes = 0;
		}
		if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	}
	for (auto& [Key, Resource] : ParticleResources)
	{
		if (Resource.TrackedMemoryBytes > 0)
		{
			MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
			Resource.TrackedMemoryBytes = 0;
		}
		if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	}
	for (auto& [Key, Resource] : TextureResources)
	{
		if (Resource.TrackedMemoryBytes > 0)
		{
			MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
			Resource.TrackedMemoryBytes = 0;
		}
		if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	}
	LoadedResource.clear();
}

// --- Font ---
FFontResource* FResourceManager::FindFont(const FName& FontName)
{
	const FString LookupKey = FontName.ToString();
	auto It = FontResources.find(LookupKey);
	if (It != FontResources.end())
	{
		return &It->second;
	}

	const FString LowerLookupKey = ToLowerCopy(LookupKey);
	for (auto& [Key, Resource] : FontResources)
	{
		if (ToLowerCopy(Key) == LowerLookupKey)
		{
			return &Resource;
		}
	}

	return nullptr;
}

const FFontResource* FResourceManager::FindFont(const FName& FontName) const
{
	const FString LookupKey = FontName.ToString();
	auto It = FontResources.find(LookupKey);
	return (It != FontResources.end()) ? &It->second : nullptr;

	const FString LowerLookupKey = ToLowerCopy(LookupKey);
	for (const auto& [Key, Resource] : FontResources)
	{
		if (ToLowerCopy(Key) == LowerLookupKey)
		{
			return &Resource;
		}
	}

	return nullptr;
}

void FResourceManager::RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FFontResource Resource;
	Resource.Name    = FontName;
	Resource.Path    = InPath;
	Resource.Columns = Columns;
	Resource.Rows    = Rows;
	Resource.SRV     = nullptr;
	FontResources[FontName.ToString()] = Resource;
}

// --- Particle ---
FParticleResource* FResourceManager::FindParticle(const FName& ParticleName)
{
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : nullptr;
}

const FParticleResource* FResourceManager::FindParticle(const FName& ParticleName) const
{
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FParticleResource Resource;
	Resource.Name    = ParticleName;
	Resource.Path    = InPath;
	Resource.Columns = Columns;
	Resource.Rows    = Rows;
	Resource.SRV     = nullptr;
	ParticleResources[ParticleName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetFontNames() const
{
	TArray<FString> Names;
	Names.reserve(FontResources.size());
	for (const auto& [Key, _] : FontResources)
	{
		Names.push_back(Key);
	}
	std::sort(Names.begin(), Names.end());
	return Names;
}

TArray<FString> FResourceManager::GetParticleNames() const
{
	TArray<FString> Names;
	Names.reserve(ParticleResources.size());
	for (const auto& [Key, _] : ParticleResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

// --- Texture ---
FTextureResource* FResourceManager::FindTexture(const FName& TextureName)
{
	auto It = TextureResources.find(TextureName.ToString());
	return (It != TextureResources.end()) ? &It->second : nullptr;
}

const FTextureResource* FResourceManager::FindTexture(const FName& TextureName) const
{
	auto It = TextureResources.find(TextureName.ToString());
	return (It != TextureResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterTexture(const FName& TextureName, const FString& InPath)
{
	FTextureResource Resource;
	Resource.Name    = TextureName;
	Resource.Path    = InPath;
	Resource.Columns = 1;
	Resource.Rows    = 1;
	Resource.SRV     = nullptr;
	TextureResources[TextureName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetTextureNames(bool bIncludeEditorResources) const
{
	TArray<FString> Names;
	Names.reserve(TextureResources.size());
	for (const auto& [Key, Resource] : TextureResources)
	{
		if (!bIncludeEditorResources && Resource.bEditorResource)
		{
			continue;
		}
		Names.push_back(Key);
	}
	return Names;
}

FMeshResource* FResourceManager::FindMesh(const FName& MeshName)
{
	auto It = MeshResources.find(MeshName.ToString());
	return (It != MeshResources.end()) ? &It->second : nullptr;
}

const FMeshResource* FResourceManager::FindMesh(const FName& MeshName) const
{
	auto It = MeshResources.find(MeshName.ToString());
	return (It != MeshResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterMesh(const FName& MeshName, const FString& InPath)
{
	FMeshResource Resource;
	Resource.Name = MeshName;
	Resource.Path = InPath;
	MeshResources[MeshName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetMeshNames() const
{
	TArray<FString> Names;
	Names.reserve(MeshResources.size());
	for (const auto& [Key, _] : MeshResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

FSoundResource* FResourceManager::FindSound(const FName& SoundName)
{
	auto It = SoundResources.find(SoundName.ToString());
	return (It != SoundResources.end()) ? &It->second : nullptr;
}

const FSoundResource* FResourceManager::FindSound(const FName& SoundName) const
{
	auto It = SoundResources.find(SoundName.ToString());
	return (It != SoundResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterSound(const FName& SoundName, const FString& InPath, ESoundCategory Category)
{
	FSoundResource Resource;
	Resource.Name = SoundName;
	Resource.Path = InPath;
	Resource.Category = Category;
	SoundResources[SoundName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetSoundNames() const
{
	TArray<FString> Names;
	Names.reserve(SoundResources.size());
	for (const auto& [Key, _] : SoundResources)
	{
		Names.push_back(Key);
	}
	std::sort(Names.begin(), Names.end());
	return Names;
}

TArray<FString> FResourceManager::GetSoundNames(ESoundCategory Category) const
{
	TArray<FString> Names;
	for (const auto& [Key, Resource] : SoundResources)
	{
		if (Resource.Category == Category)
		{
			Names.push_back(Key);
		}
	}
	std::sort(Names.begin(), Names.end());
	return Names;
}

FMaterialResource* FResourceManager::FindMaterial(const FName& MaterialName)
{
	auto It = MaterialResources.find(MaterialName.ToString());
	return (It != MaterialResources.end()) ? &It->second : nullptr;
}

const FMaterialResource* FResourceManager::FindMaterial(const FName& MaterialName) const
{
	auto It = MaterialResources.find(MaterialName.ToString());
	return (It != MaterialResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterMaterial(const FName& MaterialName, const FString& InPath)
{
	FMaterialResource Resource;
	Resource.Name = MaterialName;
	Resource.Path = InPath;
	MaterialResources[MaterialName.ToString()] = Resource;

	RegisterPath(MaterialName, InPath);
}

TArray<FString> FResourceManager::GetMaterialNames() const
{
	TArray<FString> Names;
	Names.reserve(MaterialResources.size());
	for (const auto& [Key, _] : MaterialResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

FGenericPathResource* FResourceManager::FindPath(const FName& ResourceName)
{
	auto It = PathResources.find(ResourceName.ToString());
	return (It != PathResources.end()) ? &It->second : nullptr;
}

const FGenericPathResource* FResourceManager::FindPath(const FName& ResourceName) const
{
	auto It = PathResources.find(ResourceName.ToString());
	return (It != PathResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterPath(const FName& ResourceName, const FString& InPath)
{
	FGenericPathResource Resource;
	Resource.Name = ResourceName;
	Resource.Path = InPath;
	PathResources[ResourceName.ToString()] = Resource;
}

FString FResourceManager::ResolvePath(const FName& ResourceName, const FString& Fallback) const
{
	const FGenericPathResource* Resource = FindPath(ResourceName);
	return Resource ? Resource->Path : Fallback;
}

TArray<FString> FResourceManager::GetPathNames() const
{
	TArray<FString> Names;
	Names.reserve(PathResources.size());
	for (const auto& [Key, _] : PathResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FResourceManager::FindLoadedTexture(FString InPath)
{
	auto It = LoadedResource.find(InPath);
	return (It != LoadedResource.end()) ? It->second : nullptr;
}

