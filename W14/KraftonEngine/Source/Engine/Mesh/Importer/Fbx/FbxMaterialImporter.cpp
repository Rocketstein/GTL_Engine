#include "Mesh/Importer/Fbx/FbxMaterialImporter.h"
#include "Materials/MaterialManager.h"
#include "Materials/Material.h"
#include "Platform/Paths.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <system_error>

namespace
{
	namespace fs = std::filesystem;

	struct FTextureResolveContext
	{
		FString FbxSourcePath;
		FString EmbeddedTextureScratchDirectory;
		FString MaterialName;
	};

	FString NormalizeTexturePathSeparators(FString Path)
	{
		std::replace(Path.begin(), Path.end(), '\\', '/');
		return Path;
	}

	fs::path ToFilesystemPath(const FString& Path)
	{
		fs::path Result(FPaths::ToWide(NormalizeTexturePathSeparators(Path)));
		if (!Result.empty() && !Result.is_absolute())
		{
			Result = fs::path(FPaths::RootDir()) / Result;
		}
		return Result;
	}

	fs::path ToLexicallyNormalAbsolutePath(const fs::path& Path)
	{
		if (Path.empty())
		{
			return Path;
		}

		fs::path Result = Path;
		if (!Result.is_absolute())
		{
			Result = fs::path(FPaths::RootDir()) / Result;
		}
		return Result.lexically_normal();
	}

	std::wstring ToLower(std::wstring Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(), [](wchar_t Ch)
		{
			return static_cast<wchar_t>(std::towlower(Ch));
		});
		return Value;
	}

	FString ToLower(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});
		return Value;
	}

	bool IsSupportedTextureExtension(const fs::path& Path)
	{
		const std::wstring Ext = ToLower(Path.extension().wstring());
		return Ext == L".png" || Ext == L".jpg" || Ext == L".jpeg" || Ext == L".tga" || Ext == L".bmp" || Ext == L".dds";
	}

	bool HasUnsupportedTextureExtension(const fs::path& Path)
	{
		return !Path.extension().empty() && !IsSupportedTextureExtension(Path);
	}

	void LogUnsupportedTexture(const FTextureResolveContext& Context, const fs::path& TexturePath)
	{
		UE_LOG(
			"FBX texture unsupported: Material='%s' Texture='%s' Extension='%s'",
			Context.MaterialName.c_str(),
			FPaths::ToUtf8(TexturePath.generic_wstring()).c_str(),
			FPaths::ToUtf8(TexturePath.extension().wstring()).c_str()
		);
	}

	const TArray<std::wstring>& GetTextureExtensionFallbacks()
	{
		static const TArray<std::wstring> Extensions = {
			L".png", L".jpg", L".jpeg", L".tga", L".bmp", L".dds"
		};
		return Extensions;
	}

	bool TryFindCaseInsensitive(const fs::path& Candidate, fs::path& OutPath)
	{
		std::error_code Ec;
		if (fs::exists(Candidate, Ec) && fs::is_regular_file(Candidate, Ec))
		{
			OutPath = Candidate;
			return true;
		}

		const fs::path Parent = Candidate.parent_path();
		if (Parent.empty() || !fs::exists(Parent, Ec) || !fs::is_directory(Parent, Ec))
		{
			return false;
		}

		const std::wstring Wanted = ToLower(Candidate.filename().wstring());
		for (const fs::directory_entry& Entry : fs::directory_iterator(Parent, Ec))
		{
			if (Ec || !Entry.is_regular_file())
			{
				continue;
			}

			if (ToLower(Entry.path().filename().wstring()) == Wanted)
			{
				OutPath = Entry.path();
				return true;
			}
		}

		return false;
	}

	bool TryResolveTextureCandidate(const fs::path& Candidate, fs::path& OutPath)
	{
		if (Candidate.empty())
		{
			return false;
		}

		if (TryFindCaseInsensitive(Candidate, OutPath))
		{
			return true;
		}

		const fs::path Parent = Candidate.parent_path();
		const fs::path Stem = Candidate.stem();
		if (Parent.empty() || Stem.empty())
		{
			return false;
		}

		for (const std::wstring& Extension : GetTextureExtensionFallbacks())
		{
			fs::path Alternative = Parent / (Stem.wstring() + Extension);
			if (TryFindCaseInsensitive(Alternative, OutPath))
			{
				return true;
			}
		}

		return false;
	}

	void AddUniquePath(TArray<fs::path>& Paths, const fs::path& Path)
	{
		if (Path.empty())
		{
			return;
		}

		for (const fs::path& Existing : Paths)
		{
			if (Existing == Path)
			{
				return;
			}
		}
		Paths.push_back(Path);
	}

	TArray<fs::path> BuildTextureSearchDirectories(const FTextureResolveContext& ResolveContext)
	{
		TArray<fs::path> Directories;
		const fs::path FbxPath = ToFilesystemPath(ResolveContext.FbxSourcePath);
		const fs::path FbxDir = FbxPath.parent_path();
		const fs::path ParentDir = FbxDir.parent_path();

		if (!ResolveContext.EmbeddedTextureScratchDirectory.empty())
		{
			AddUniquePath(Directories, ToFilesystemPath(ResolveContext.EmbeddedTextureScratchDirectory));
		}
		AddUniquePath(Directories, FbxDir);
		AddUniquePath(Directories, FbxDir / L"tex");
		AddUniquePath(Directories, FbxDir / L"Tex");
		AddUniquePath(Directories, FbxDir / L"textures");
		AddUniquePath(Directories, FbxDir / L"Textures");
		AddUniquePath(Directories, ParentDir);
		AddUniquePath(Directories, ParentDir / L"tex");
		AddUniquePath(Directories, ParentDir / L"Tex");
		AddUniquePath(Directories, ParentDir / L"textures");
		AddUniquePath(Directories, ParentDir / L"Textures");
		return Directories;
	}

	TArray<fs::path> BuildTextureCandidates(const FString& RawTexturePath, const FTextureResolveContext& ResolveContext)
	{
		TArray<fs::path> Candidates;
		const FString NormalizedRaw = NormalizeTexturePathSeparators(RawTexturePath);
		const fs::path RawPath(FPaths::ToWide(NormalizedRaw));
		if (RawPath.empty())
		{
			return Candidates;
		}

		const fs::path FileName = RawPath.filename();
		const fs::path FbxPath = ToFilesystemPath(ResolveContext.FbxSourcePath);
		const fs::path FbxDir = FbxPath.parent_path();
		const fs::path ParentDir = FbxDir.parent_path();
		const fs::path EmbeddedDir = ResolveContext.EmbeddedTextureScratchDirectory.empty()
			? fs::path()
			: ToFilesystemPath(ResolveContext.EmbeddedTextureScratchDirectory);

		AddUniquePath(Candidates, RawPath);
		AddUniquePath(Candidates, FbxDir / RawPath);
		AddUniquePath(Candidates, FbxDir / FileName);
		AddUniquePath(Candidates, FbxDir / L"tex" / FileName);
		AddUniquePath(Candidates, FbxDir / L"Tex" / FileName);
		AddUniquePath(Candidates, FbxDir / L"textures" / FileName);
		AddUniquePath(Candidates, FbxDir / L"Textures" / FileName);
		AddUniquePath(Candidates, ParentDir / RawPath);
		AddUniquePath(Candidates, ParentDir / FileName);
		AddUniquePath(Candidates, ParentDir / L"tex" / FileName);
		AddUniquePath(Candidates, ParentDir / L"Tex" / FileName);
		AddUniquePath(Candidates, ParentDir / L"textures" / FileName);
		AddUniquePath(Candidates, ParentDir / L"Textures" / FileName);
		AddUniquePath(Candidates, EmbeddedDir / FileName);
		return Candidates;
	}

	TArray<fs::path> FindEmbeddedTextureMatches(const FString& RawTexturePath, const FTextureResolveContext& ResolveContext)
	{
		TArray<fs::path> Matches;
		if (ResolveContext.EmbeddedTextureScratchDirectory.empty())
		{
			return Matches;
		}

		const fs::path EmbeddedDir = ToFilesystemPath(ResolveContext.EmbeddedTextureScratchDirectory);
		std::error_code Ec;
		if (EmbeddedDir.empty() || !fs::exists(EmbeddedDir, Ec) || !fs::is_directory(EmbeddedDir, Ec))
		{
			return Matches;
		}

		const fs::path RawPath(FPaths::ToWide(NormalizeTexturePathSeparators(RawTexturePath)));
		const std::wstring WantedFileName = ToLower(RawPath.filename().wstring());
		const std::wstring WantedStem = ToLower(RawPath.stem().wstring());
		if (WantedFileName.empty() && WantedStem.empty())
		{
			return Matches;
		}

		for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(EmbeddedDir, Ec))
		{
			if (Ec || !Entry.is_regular_file())
			{
				continue;
			}

			const fs::path EntryPath = Entry.path();
			if (!IsSupportedTextureExtension(EntryPath))
			{
				continue;
			}

			const std::wstring EntryFileName = ToLower(EntryPath.filename().wstring());
			const std::wstring EntryStem = ToLower(EntryPath.stem().wstring());
			if ((!WantedFileName.empty() && EntryFileName == WantedFileName) ||
				(!WantedStem.empty() && EntryStem == WantedStem))
			{
				Matches.push_back(EntryPath);
			}
		}

		std::sort(Matches.begin(), Matches.end(), [](const fs::path& A, const fs::path& B)
		{
			return ToLower(A.generic_wstring()) < ToLower(B.generic_wstring());
		});
		return Matches;
	}

	FString CopyResolvedTextureToProject(const fs::path& FoundPath, const FTextureResolveContext& ResolveContext)
	{
		if (FoundPath.empty())
		{
			return FString();
		}

		if (!IsSupportedTextureExtension(FoundPath))
		{
			LogUnsupportedTexture(ResolveContext, FoundPath);
			return FString();
		}

		const fs::path FbxPath = ToFilesystemPath(ResolveContext.FbxSourcePath);
		const std::wstring SubFolder = FbxPath.stem().wstring();
		const fs::path DestRelDir = fs::path(L"Content") / L"Texture" / L"Auto" / SubFolder;
		const fs::path DestAbsDir = fs::path(FPaths::RootDir()) / DestRelDir;

		std::error_code Ec;
		fs::create_directories(DestAbsDir, Ec);

		const fs::path DestAbsPath = DestAbsDir / FoundPath.filename();
		if (ToLexicallyNormalAbsolutePath(FoundPath) == ToLexicallyNormalAbsolutePath(DestAbsPath))
		{
			const fs::path DestRelPath = DestRelDir / FoundPath.filename();
			return FPaths::ToUtf8(DestRelPath.generic_wstring());
		}

		fs::copy_file(FoundPath, DestAbsPath, fs::copy_options::overwrite_existing, Ec);
		if (Ec)
		{
			return FPaths::MakeProjectRelative(FPaths::ToUtf8(FoundPath.generic_wstring()));
		}

		const fs::path DestRelPath = DestRelDir / FoundPath.filename();
		return FPaths::ToUtf8(DestRelPath.generic_wstring());
	}

	FString ImportTextureToProject(const FString& RawTexturePath, const FTextureResolveContext& ResolveContext)
	{
		if (RawTexturePath.empty())
		{
			return FString();
		}

		fs::path FoundPath;
		for (const fs::path& Candidate : BuildTextureCandidates(RawTexturePath, ResolveContext))
		{
			if (TryResolveTextureCandidate(Candidate, FoundPath))
			{
				return CopyResolvedTextureToProject(FoundPath, ResolveContext);
			}
		}

		TArray<fs::path> EmbeddedMatches = FindEmbeddedTextureMatches(RawTexturePath, ResolveContext);
		if (!EmbeddedMatches.empty())
		{
			if (EmbeddedMatches.size() > 1)
			{
				UE_LOG(
					"FBX embedded texture duplicate match: Material='%s' Texture='%s' Count=%d Using='%s'",
					ResolveContext.MaterialName.c_str(),
					RawTexturePath.c_str(),
					static_cast<int32>(EmbeddedMatches.size()),
					FPaths::ToUtf8(EmbeddedMatches.front().generic_wstring()).c_str()
				);
			}
			return CopyResolvedTextureToProject(EmbeddedMatches.front(), ResolveContext);
		}

		const fs::path RawPath(FPaths::ToWide(NormalizeTexturePathSeparators(RawTexturePath)));
		if (HasUnsupportedTextureExtension(RawPath))
		{
			LogUnsupportedTexture(ResolveContext, RawPath);
			return FString();
		}

		// 실제 파일을 못 찾으면 기존 동작 유지 (경로만 정리)
		return FPaths::MakeProjectRelative(NormalizeTexturePathSeparators(RawTexturePath));
	}

	bool ContainsAnyToken(const FString& Text, std::initializer_list<const char*> Tokens)
	{
		for (const char* Token : Tokens)
		{
			if (Text.find(Token) != FString::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool EndsWithToken(const FString& Text, const char* Token)
	{
		const size_t TokenLength = std::strlen(Token);
		if (Text.size() < TokenLength)
		{
			return false;
		}

		return Text.compare(Text.size() - TokenLength, TokenLength, Token) == 0;
	}

	FString GetLowerTextureStem(const FString& TexturePath)
	{
		const fs::path Path(FPaths::ToWide(NormalizeTexturePathSeparators(TexturePath)));
		return ToLower(FPaths::ToUtf8(Path.stem().wstring()));
	}

	bool HasNormalTextureToken(const FString& Stem)
	{
		return EndsWithToken(Stem, "_n") ||
			EndsWithToken(Stem, "-n") ||
			ContainsAnyToken(Stem, { "_normal", "-normal", " normal", "normal", "_norm", "-norm", " norm", "_bump", "-bump", " bump", "bump" });
	}

	bool HasColorTextureToken(const FString& Stem)
	{
		return EndsWithToken(Stem, "_c") ||
			EndsWithToken(Stem, "-c") ||
			ContainsAnyToken(Stem, { "_diff", "-diff", " diff", "diffuse", "albedo", "basecolor", "_color", "-color", " color", "color", "_col", "-col", " col" });
	}

	bool HasPackedDataTextureToken(const FString& Stem)
	{
		return EndsWithToken(Stem, "_ro") ||
			EndsWithToken(Stem, "-ro") ||
		ContainsAnyToken(Stem, { "rough", "_spec", "-spec", " spec", "metal", "_ao", "-ao", " ao" });
	}

	bool HasOpacityMaskTextureToken(const FString& Stem)
	{
		return EndsWithToken(Stem, "_a") ||
			EndsWithToken(Stem, "-a") ||
			ContainsAnyToken(Stem, { "_mask", "-mask", " mask", "_alpha", "-alpha", " alpha", "_opacity", "-opacity", " opacity", "_trans", "-trans", " trans" });
	}

	bool IsLikelyNormalTexturePath(const FString& TexturePath)
	{
		const FString Stem = GetLowerTextureStem(TexturePath);
		return HasNormalTextureToken(Stem);
	}

	bool IsLikelyColorTexturePath(const FString& TexturePath)
	{
		const FString Stem = GetLowerTextureStem(TexturePath);
		return HasColorTextureToken(Stem);
	}

	int32 ScoreTextureForRole(const fs::path& TexturePath, const FString& MaterialName, bool bNormalMap)
	{
		const FString Stem = ToLower(FPaths::ToUtf8(TexturePath.stem().wstring()));
		const FString File = ToLower(FPaths::ToUtf8(TexturePath.filename().wstring()));
		const FString Mat  = ToLower(MaterialName);
		int32 Score = 0;

		if (Mat.find(Stem) != FString::npos || Stem.find(Mat) != FString::npos)
		{
			Score += 80;
		}

		if (bNormalMap)
		{
			if (HasNormalTextureToken(Stem)) Score += 120;
			if (HasColorTextureToken(Stem) || HasPackedDataTextureToken(Stem) || HasOpacityMaskTextureToken(Stem)) Score -= 100;
		}
		else
		{
			if (HasColorTextureToken(Stem)) Score += 120;
			if (HasNormalTextureToken(Stem) || HasPackedDataTextureToken(Stem) || HasOpacityMaskTextureToken(Stem)) Score -= 100;
		}

		if (File.find("car") != FString::npos && Mat.find("car") != FString::npos)
		{
			Score += 10;
		}
		return Score;
	}

	FString FindBestTextureByRole(const FTextureResolveContext& ResolveContext, bool bNormalMap)
	{
		fs::path BestPath;
		int32 BestScore = bNormalMap ? 30 : 30;
		for (const fs::path& Directory : BuildTextureSearchDirectories(ResolveContext))
		{
			std::error_code Ec;
			if (Directory.empty() || !fs::exists(Directory, Ec) || !fs::is_directory(Directory, Ec))
			{
				continue;
			}

			for (const fs::directory_entry& Entry : fs::directory_iterator(Directory, Ec))
			{
				if (Ec || !Entry.is_regular_file() || !IsSupportedTextureExtension(Entry.path()))
				{
					continue;
				}

				const FString Stem = ToLower(FPaths::ToUtf8(Entry.path().stem().wstring()));
				if (bNormalMap && !HasNormalTextureToken(Stem))
				{
					continue;
				}
				if (!bNormalMap && HasOpacityMaskTextureToken(Stem))
				{
					continue;
				}

				const int32 Score = ScoreTextureForRole(Entry.path(), ResolveContext.MaterialName, bNormalMap);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestPath = Entry.path();
				}
			}
		}

		return BestPath.empty() ? FString() : CopyResolvedTextureToProject(BestPath, ResolveContext);
	}

	FString FindCompanionOpacityMaskTexture(const FString& TexturePath, const FTextureResolveContext& ResolveContext)
	{
		if (TexturePath.empty())
		{
			return FString();
		}

		const fs::path SourcePath(FPaths::ToWide(NormalizeTexturePathSeparators(TexturePath)));
		const fs::path Stem = SourcePath.stem();
		if (Stem.empty())
		{
			return FString();
		}

		static const TArray<std::wstring> Suffixes = { L"_mask", L"-mask", L"_alpha", L"-alpha", L"_opacity", L"-opacity" };
		for (const fs::path& Directory : BuildTextureSearchDirectories(ResolveContext))
		{
			for (const std::wstring& Suffix : Suffixes)
			{
				for (const std::wstring& Extension : GetTextureExtensionFallbacks())
				{
					fs::path FoundPath;
					if (TryResolveTextureCandidate(Directory / (Stem.wstring() + Suffix + Extension), FoundPath))
					{
						return CopyResolvedTextureToProject(FoundPath, ResolveContext);
					}
				}
			}
		}

		return FString();
	}

	FString ReadFirstTextureFromProperty(const FbxProperty& Property, const FTextureResolveContext& ResolveContext)
	{
		if (!Property.IsValid())
		{
			return FString();
		}

		const int32 TextureCount = Property.GetSrcObjectCount<FbxTexture>();
		for (int32 TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
		{
			FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(TextureIndex);
			if (Texture)
			{
				const char* FileName = Texture->GetFileName();
				FString Imported = ImportTextureToProject(FileName ? FileName : "", ResolveContext);
				if (Imported.empty())
				{
					const char* RelativeFileName = Texture->GetRelativeFileName();
					if (RelativeFileName && RelativeFileName[0] != '\0' && (!FileName || std::strcmp(RelativeFileName, FileName) != 0))
					{
						Imported = ImportTextureToProject(RelativeFileName, ResolveContext);
					}
				}

				if (!Imported.empty())
				{
					return Imported;
				}
			}
			else if (FbxTexture* AnyTexture = Property.GetSrcObject<FbxTexture>(TextureIndex))
			{
				UE_LOG(
					"FBX texture unsupported: Material='%s' TextureObject='%s' Type='%s'",
					ResolveContext.MaterialName.c_str(),
					AnyTexture->GetName(),
					AnyTexture->GetClassId().GetName()
				);
			}
		}

		return FString();
	}
}

void FFbxMaterialImporter::CollectMaterials(FbxScene* Scene, FFbxImportContext& Context)
{
	Context.Materials.clear();
	Context.MaterialToSlotIndex.clear();

	if (!Scene)
	{
		return;
	}

	const int32 MaterialCount = Scene->GetMaterialCount();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		FbxSurfaceMaterial* Material = Scene->GetMaterial(MaterialIndex);
		if (!Material)
		{
			continue;
		}

		FFbxImportedMaterialInfo MaterialInfo;
		MaterialInfo.Name = Material->GetName();
		MaterialInfo.DiffuseColor = FVector(1.0f, 1.0f, 1.0f);

		FTextureResolveContext ResolveContext;
		ResolveContext.FbxSourcePath = Context.SourcePath;
		ResolveContext.EmbeddedTextureScratchDirectory = Context.EmbeddedTextureScratchDirectory;
		ResolveContext.MaterialName = MaterialInfo.Name;

		FbxProperty DiffuseProp = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
		if (DiffuseProp.IsValid())
		{
			FbxDouble3 Color = DiffuseProp.Get<FbxDouble3>();
			MaterialInfo.DiffuseColor = FVector(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]));
			const FString DiffuseTexturePath = ReadFirstTextureFromProperty(DiffuseProp, ResolveContext);
			if (!DiffuseTexturePath.empty())
			{
				if (IsLikelyNormalTexturePath(DiffuseTexturePath))
				{
					MaterialInfo.NormalTexturePath = DiffuseTexturePath;
				}
				else
				{
					MaterialInfo.DiffuseTexturePath = DiffuseTexturePath;
				}
			}
		}

		// Specular 강도 — FBX 의 SpecularFactor 를 그대로 머티리얼 SpecularStrength 로 사용.
		// (엔진은 Blinn-Phong specular 를 셰이더에서 계산하며, 이 값으로 highlight 세기를 제어한다.)
		FbxProperty SpecularFactorProp = Material->FindProperty(FbxSurfaceMaterial::sSpecularFactor);
		if (SpecularFactorProp.IsValid())
		{
			MaterialInfo.SpecularStrength = static_cast<float>(SpecularFactorProp.Get<FbxDouble>());
		}
		UE_LOG(
			"FBX material specular: Material='%s' SpecularStrength=%.3f",
			MaterialInfo.Name.c_str(),
			MaterialInfo.SpecularStrength
		);

		FbxProperty NormalProp = Material->FindProperty(FbxSurfaceMaterial::sNormalMap);
		const FString NormalTexturePath = ReadFirstTextureFromProperty(NormalProp, ResolveContext);
		if (!NormalTexturePath.empty())
		{
			if (IsLikelyColorTexturePath(NormalTexturePath) && MaterialInfo.DiffuseTexturePath.empty())
			{
				MaterialInfo.DiffuseTexturePath = NormalTexturePath;
			}
			else
			{
				MaterialInfo.NormalTexturePath = NormalTexturePath;
			}
		}

		if (MaterialInfo.NormalTexturePath.empty())
		{
			FbxProperty BumpProp = Material->FindProperty(FbxSurfaceMaterial::sBump);
			const FString BumpTexturePath = ReadFirstTextureFromProperty(BumpProp, ResolveContext);
			if (!BumpTexturePath.empty())
			{
				if (IsLikelyColorTexturePath(BumpTexturePath) && MaterialInfo.DiffuseTexturePath.empty())
				{
					MaterialInfo.DiffuseTexturePath = BumpTexturePath;
				}
				else
				{
					MaterialInfo.NormalTexturePath = BumpTexturePath;
				}
			}
		}

		if (MaterialInfo.DiffuseTexturePath.empty())
		{
			MaterialInfo.DiffuseTexturePath = FindBestTextureByRole(ResolveContext, false);
		}
		if (MaterialInfo.NormalTexturePath.empty())
		{
			MaterialInfo.NormalTexturePath = FindBestTextureByRole(ResolveContext, true);
		}
		if (MaterialInfo.OpacityMaskTexturePath.empty() && !MaterialInfo.DiffuseTexturePath.empty())
		{
			MaterialInfo.OpacityMaskTexturePath = FindCompanionOpacityMaskTexture(MaterialInfo.DiffuseTexturePath, ResolveContext);
		}

		const int32 GlobalIndex = static_cast<int32>(Context.Materials.size());
		Context.Materials.push_back(MaterialInfo);
		Context.MaterialToSlotIndex[Material] = GlobalIndex;
	}
}

int32 FFbxMaterialImporter::GetMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
{
	FbxLayerElementMaterial* LayerElementMaterial = Mesh ? Mesh->GetElementMaterial() : nullptr;
	if (!LayerElementMaterial)
	{
		return -1;
	}

	FbxLayerElementArrayTemplate<int32>& MaterialIndices = LayerElementMaterial->GetIndexArray();
	switch (LayerElementMaterial->GetMappingMode())
	{
	case FbxLayerElement::eAllSame:
		return MaterialIndices[0];
	case FbxLayerElement::eByPolygon:
		return MaterialIndices[PolygonIndex];
	default:
		return 0;
	}
}

void FFbxMaterialImporter::BuildStaticMaterials(const FFbxImportContext& Context, TArray<FStaticMaterial>& OutMaterials)
{
	OutMaterials.clear();
	OutMaterials.reserve(Context.Materials.size());

	for (const FFbxImportedMaterialInfo& MaterialInfo : Context.Materials)
	{
		FStaticMaterial NewMaterial;
		NewMaterial.MaterialSlotName = MaterialInfo.Name;
		NewMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(CreateOrUpdateMaterialAsset(MaterialInfo));
		OutMaterials.push_back(NewMaterial);
	}
}

void FFbxMaterialImporter::BuildSkeletalMaterials(const FFbxImportContext& Context, const TArray<FSkeletalMeshSection>& Sections, TArray<FSkeletalMaterial>& OutMaterials, TArray<FSkeletalMeshSection>& InOutSections)
{
	OutMaterials.clear();
	OutMaterials.reserve(Context.Materials.size());

	for (const FFbxImportedMaterialInfo& MaterialInfo : Context.Materials)
	{
		const FString MaterialPath = CreateOrUpdateMaterialAsset(MaterialInfo);
		UMaterial* MaterialObject = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);

		FSkeletalMaterial NewMaterial;
		NewMaterial.MaterialInterface = MaterialObject;
		NewMaterial.MaterialSlotName = MaterialInfo.Name;
		NewMaterial.MaterialPath = MaterialPath;
		OutMaterials.push_back(NewMaterial);
	}

	bool bNeedsNoneSlot = OutMaterials.empty();
	for (const FSkeletalMeshSection& Section : Sections)
	{
		if (Section.MaterialSlotName == "None")
		{
			bNeedsNoneSlot = true;
			break;
		}
	}

	if (bNeedsNoneSlot)
	{
		FSkeletalMaterial DefaultMaterial;
		DefaultMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");
		DefaultMaterial.MaterialSlotName = "None";
		DefaultMaterial.MaterialPath = DefaultMaterial.MaterialInterface
			? DefaultMaterial.MaterialInterface->GetAssetPathFileName()
			: FString();
		OutMaterials.push_back(DefaultMaterial);

		const int32 NoneMaterialIndex = static_cast<int32>(OutMaterials.size()) - 1;
		for (FSkeletalMeshSection& Section : InOutSections)
		{
			if (Section.MaterialSlotName == "None")
			{
				Section.MaterialIndex = NoneMaterialIndex;
			}
		}
	}
}

FString FFbxMaterialImporter::CreateOrUpdateMaterialAsset(const FFbxImportedMaterialInfo& MaterialInfo)
{
	const FString UassetPath = "Content/Material/Auto/" + MaterialInfo.Name + ".uasset";

	std::filesystem::create_directories(FPaths::ToWide("Content/Material/Auto"));

	const FVector4 SectionColor = MaterialInfo.DiffuseTexturePath.empty()
		? FVector4(MaterialInfo.DiffuseColor.X, MaterialInfo.DiffuseColor.Y, MaterialInfo.DiffuseColor.Z, 1.0f)
		: FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	const FString DiffuseTex = MaterialInfo.DiffuseTexturePath.empty() ? FString() : FPaths::MakeProjectRelative(MaterialInfo.DiffuseTexturePath);
	const FString NormalTex  = MaterialInfo.NormalTexturePath.empty()  ? FString() : FPaths::MakeProjectRelative(MaterialInfo.NormalTexturePath);
	const FString OpacityMaskTex = MaterialInfo.OpacityMaskTexturePath.empty() ? FString() : FPaths::MakeProjectRelative(MaterialInfo.OpacityMaskTexturePath);

	// JSON 없이 머티리얼을 직접 빌드해 .uasset(바이너리)으로 저장.
	FMaterialManager::Get().CreateImportedMaterialAsset(UassetPath, SectionColor, DiffuseTex, NormalTex, OpacityMaskTex, MaterialInfo.SpecularStrength);
	return UassetPath;
}
