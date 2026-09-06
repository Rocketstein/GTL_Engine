#include "Engine/Platform/ScriptPaths.h"

#include "Engine/Platform/Paths.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iterator>

namespace
{
	// DirectoryWatcher prefix, 직렬화 문자열, 실제 폴더 이름을 모두 같은 표기로 고정한다.
	constexpr const wchar_t* ScriptsDirectoryName = L"Scripts";
}

bool FScriptPaths::IsScriptsRootPath(const std::filesystem::path& Path)
{
	const auto FirstSegment = Path.begin();
	if (FirstSegment == Path.end())
	{
		return false;
	}

	std::wstring First = FirstSegment->wstring();
	std::transform(
		First.begin(),
		First.end(),
		First.begin(),
		[](wchar_t Character)
		{
			return static_cast<wchar_t>(std::towlower(Character));
		});
	return First == L"scripts";
}

std::filesystem::path FScriptPaths::MakeCanonicalScriptsPath(const std::filesystem::path& Path)
{
	if (!IsScriptsRootPath(Path))
	{
		return Path;
	}

	// 첫 번째 세그먼트는 항상 "Scripts"로 되돌려
	// callers가 scripts/, SCRIPTS/처럼 넘겨도 내부 저장값이 흔들리지 않게 한다.
	std::filesystem::path CanonicalPath(ScriptsDirectoryName);
	auto Segment = Path.begin();
	++Segment;
	for (; Segment != Path.end(); ++Segment)
	{
		CanonicalPath /= *Segment;
	}

	return CanonicalPath;
}

std::filesystem::path FScriptPaths::NormalizeScriptRelativePath(const FString& ScriptPath)
{
	std::filesystem::path Path(FPaths::ToWide(ScriptPath));
	if (Path.empty())
	{
		return {};
	}

	Path = Path.lexically_normal();
	if (Path.is_absolute())
	{
		// 절대 경로가 들어와도 프로젝트 루트 하위면 다시 상대 경로로 되돌린다.
		// 이렇게 해야 내부 저장값과 DirectoryWatcher 콜백 값이 같은 형식으로 맞춰진다.
		const std::filesystem::path RootPath(FPaths::RootDir());
		const std::filesystem::path RelativePath = Path.lexically_relative(RootPath);
		const auto FirstSegment = RelativePath.begin();
		if (!RelativePath.empty()
			&& (FirstSegment == RelativePath.end() || FirstSegment->wstring() != L".."))
		{
			Path = RelativePath;
		}
	}

	if (Path.is_absolute())
	{
		// 프로젝트 루트 밖 절대 경로면 억지로 Scripts/ 아래로 끌어오지 않는다.
		return Path.lexically_normal();
	}

	if (!IsScriptsRootPath(Path))
	{
		// 상대 경로는 항상 Scripts/ 기준으로 해석한다.
		Path = std::filesystem::path(ScriptsDirectoryName) / Path;
	}

	return MakeCanonicalScriptsPath(Path).lexically_normal();
}

FString FScriptPaths::NormalizeScriptPath(const FString& ScriptPath)
{
	// 내부 저장 경로는 UTF-8 + 슬래시 표기로 통일해서
	// 직렬화 문자열과 DirectoryWatcher 경로 비교가 운영체제 구분자에 흔들리지 않게 한다.
	const std::filesystem::path NormalizedPath = NormalizeScriptRelativePath(ScriptPath);
	return NormalizedPath.empty() ? FString() : FPaths::ToUtf8(NormalizedPath.generic_wstring());
}

std::filesystem::path FScriptPaths::ResolveScriptPath(const FString& ScriptPath)
{
	std::filesystem::path Path = NormalizeScriptRelativePath(ScriptPath);
	if (Path.empty())
	{
		// 빈 스크립트 경로를 파일로 간주하지는 않지만,
		// 호출자가 Scripts 루트 자체를 기준 경로로 쓰는 경우를 위해 폴더 경로를 돌려준다.
		return std::filesystem::path(FPaths::ScriptsDir()).lexically_normal();
	}

	if (Path.is_relative())
	{
		Path = std::filesystem::path(FPaths::RootDir()) / Path;
	}

	return Path.lexically_normal();
}

// Function : Read Lua script file from normalized Scripts path
// input : ScriptPath, OutScriptText, OutError
// ScriptPath : script path under Scripts, .lua extension is optional
// OutScriptText : loaded script source
// OutError : failure reason when script cannot be loaded
bool FScriptPaths::ReadScriptFile(const FString& ScriptPath, FString& OutScriptText, FString& OutError)
{
	if (ScriptPath.empty())
	{
		OutError = "Lua script path is empty.";
		return false;
	}

	std::filesystem::path ResolvedPath = ResolveScriptPath(ScriptPath);
	if (!std::filesystem::exists(ResolvedPath) && ResolvedPath.extension().empty())
	{
		ResolvedPath += L".lua";
	}

	if (!std::filesystem::exists(ResolvedPath))
	{
		// 호출부는 경로 정책을 모르게 하고, 사용자에게 보여줄 에러 문자열만 받는다.
		OutError = "Lua script not found: " + FPaths::ToUtf8(ResolvedPath.generic_wstring());
		return false;
	}

	std::ifstream FileStream(ResolvedPath, std::ios::binary);
	if (!FileStream.is_open())
	{
		OutError = "Failed to open Lua script: " + FPaths::ToUtf8(ResolvedPath.generic_wstring());
		return false;
	}

	OutScriptText.assign(
		std::istreambuf_iterator<char>(FileStream),
		std::istreambuf_iterator<char>());
	return true;
}
