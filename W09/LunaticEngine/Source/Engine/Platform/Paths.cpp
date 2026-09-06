#include "Engine/Platform/Paths.h"

#include <filesystem>

namespace
{
	bool IsProjectRootCandidate(const std::filesystem::path& Path)
	{
		return std::filesystem::exists(Path / L"Shaders")
			&& std::filesystem::exists(Path / L"Settings")
			&& std::filesystem::exists(Path / L"Asset");
	}

	std::filesystem::path FindProjectRoot(const std::filesystem::path& StartPath)
	{
		if (StartPath.empty())
		{
			return {};
		}

		std::filesystem::path Current = StartPath;
		if (!std::filesystem::is_directory(Current))
		{
			Current = Current.parent_path();
		}

		while (!Current.empty())
		{
			if (IsProjectRootCandidate(Current))
			{
				return Current;
			}

			const std::filesystem::path Parent = Current.parent_path();
			if (Parent == Current)
			{
				break;
			}

			Current = Parent;
		}

		return {};
	}
}

std::wstring FPaths::RootDir()
{
	static std::wstring Cached;
	if (Cached.empty())
	{
		WCHAR Buffer[MAX_PATH];
		GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
		const std::filesystem::path ExePath(Buffer);

		std::filesystem::path Root = FindProjectRoot(ExePath);
		if (Root.empty())
		{
			Root = FindProjectRoot(std::filesystem::current_path());
		}
		if (Root.empty())
		{
			Root = ExePath.parent_path();
		}

		Cached = Root.lexically_normal().wstring() + L"\\";
	}
	return Cached;
}

std::wstring FPaths::ShaderDir()   { return RootDir() + L"Shaders\\"; }
std::wstring FPaths::AssetDir()    { return RootDir() + L"Asset\\"; }
std::wstring FPaths::ContentDir()  { return RootDir() + L"Asset\\Content\\"; }
std::wstring FPaths::SceneDir()    { return RootDir() + L"Asset\\Content\\Scene\\"; }
std::wstring FPaths::DataDir()     { return ContentDir(); }
std::wstring FPaths::SaveDir()     { return RootDir() + L"Saves\\"; }
std::wstring FPaths::DumpDir()     { return RootDir() + L"Saves\\Dump\\"; }
std::wstring FPaths::LogDir()      { return RootDir() + L"Saves\\Logs\\"; }
std::wstring FPaths::SettingsDir() { return RootDir() + L"Settings\\"; }
// 스크립트는 프로젝트 루트 아래 한 곳에 고정해 둬야
// 에디터 생성 파일, 런타임 로드, 디렉터리 감시가 모두 같은 위치를 바라본다.
std::wstring FPaths::ScriptsDir()  { return RootDir() + L"Scripts\\"; }

std::wstring FPaths::SettingsFilePath() { return RootDir() + L"Settings\\Editor.ini"; }
std::wstring FPaths::ResourceFilePath() { return DefaultContentResourceFilePath(); }
std::wstring FPaths::ResourceSettingsDir() { return RootDir() + L"Settings\\Resource\\"; }
std::wstring FPaths::EditorResourceFilePath() { return ResourceSettingsDir() + L"EditorResources.ini"; }
std::wstring FPaths::DefaultContentResourceFilePath() { return ResourceSettingsDir() + L"DefaultContentResources.ini"; }
std::wstring FPaths::ProjectResourcePathsFilePath() { return ResourceSettingsDir() + L"ProjectResourcePaths.ini"; }
std::wstring FPaths::ProjectSettingsFilePath() { return RootDir() + L"Settings\\ProjectSettings.ini"; }

std::wstring FPaths::ProjectDir() { return RootDir(); }
std::wstring FPaths::ProjectContentDir() { return ContentDir(); }
std::wstring FPaths::ProjectConfigDir() { return SettingsDir(); }
std::wstring FPaths::ProjectSavedDir() { return SaveDir(); }

std::string FPaths::ConvertRelativePathToFull(const std::string& RelativePath)
{
	std::filesystem::path Path(ToWide(RelativePath));
	if (Path.is_absolute())
	{
		return NormalizePath(RelativePath);
	}

	return ToUtf8((std::filesystem::path(RootDir()) / Path).lexically_normal().wstring());
}

std::string FPaths::NormalizePath(const std::string& Path)
{
	std::filesystem::path FsPath(ToWide(Path));
	return ToUtf8(FsPath.lexically_normal().generic_wstring());
}

std::wstring FPaths::Combine(const std::wstring& Base, const std::wstring& Child)
{
	std::filesystem::path Result(Base);
	Result /= Child;
	return Result.wstring();
}

void FPaths::CreateDir(const std::wstring& Path)
{
	std::filesystem::create_directories(Path);
}

std::wstring FPaths::ToWide(const std::string& Utf8Str)
{
	if (Utf8Str.empty()) return {};
	int32_t Size = MultiByteToWideChar(CP_UTF8, 0, Utf8Str.c_str(), -1, nullptr, 0);
	std::wstring Result(Size - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, Utf8Str.c_str(), -1, &Result[0], Size);
	return Result;
}

std::string FPaths::ToUtf8(const std::wstring& WideStr)
{
	if (WideStr.empty()) return {};
	int32_t Size = WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string Result(Size - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, &Result[0], Size, nullptr, nullptr);
	return Result;
}

std::string FPaths::ResolveAssetPath(const std::string& BaseFilePath, const std::string& TargetPath)
{
	// 1. 기준 파일(OBJ 또는 MTL)의 폴더 경로 추출
	std::filesystem::path FileDir(ToWide(BaseFilePath));
	FileDir = FileDir.parent_path();

	// 2. 타겟 파일 경로(텍스처나 MTL 이름)
	std::filesystem::path Target(ToWide(TargetPath));

	// 3. 두 경로 합치기 및 정규화
	std::filesystem::path FullPath = (FileDir / Target).lexically_normal();
	std::filesystem::path ProjectRoot(RootDir());

	std::filesystem::path RelativePath;

	// 4. 절대/상대 경로 분기 처리
	if (FullPath.is_absolute())
	{
		RelativePath = FullPath.lexically_relative(ProjectRoot);
		if (RelativePath.empty())
		{
			// 드라이브가 다르거나 계산이 불가능한 경우 최후의 수단으로 파일명만 추출
			RelativePath = FullPath.filename();
		}
	}
	else
	{
		RelativePath = FullPath;
	}

	// 5. 엔진에서 사용하는 UTF-8 포맷으로 반환 (Windows 백슬래시를 슬래시로 통일)
	return ToUtf8(RelativePath.generic_wstring());
}
