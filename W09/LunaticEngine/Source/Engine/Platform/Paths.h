#pragma once

#include <string>
#include <Windows.h>

// ?붿쭊 ?꾩뿭 寃쎈줈瑜?愿由ы빀?덈떎.
// 紐⑤뱺 寃쎈줈???ㅽ뻾 ?뚯씪 湲곗? ?곷? 寃쎈줈?대ŉ, ?쒓? 寃쎈줈瑜??꾪빐 wstring 湲곕컲?낅땲??
class FPaths
{
public:
	// ?꾨줈?앺듃 猷⑦듃 (?ㅽ뻾 ?뚯씪???덈뒗 ?붾젆?곕━)
	static std::wstring RootDir();

	// 二쇱슂 ?붾젆?곕━
	static std::wstring ShaderDir();      // Shaders/
	static std::wstring AssetDir();       // Asset/
	static std::wstring ContentDir();     // Asset/Content/
	static std::wstring SceneDir();       // Asset/Content/Scene/
	static std::wstring DataDir();        // Legacy alias of Asset/Content/
	static std::wstring SaveDir();        // Saves/
	static std::wstring DumpDir();        // Saves/Dump/
	static std::wstring LogDir();         // Saves/Logs/
	static std::wstring SettingsDir();    // Settings/
	static std::wstring ScriptsDir();     // Scripts/

	// 二쇱슂 ?뚯씪 寃쎈줈
	static std::wstring SettingsFilePath();         // Settings/Editor.ini
	static std::wstring ResourceFilePath();         // Legacy alias
	static std::wstring ResourceSettingsDir();      // Settings/Resource/
	static std::wstring EditorResourceFilePath();   // Settings/Resource/EditorResources.ini
	static std::wstring DefaultContentResourceFilePath(); // Settings/Resource/DefaultContentResources.ini
	static std::wstring ProjectResourcePathsFilePath();  // Settings/Resource/ProjectResourcePaths.ini
	static std::wstring ProjectSettingsFilePath();  // Settings/ProjectSettings.ini

	static std::wstring ProjectDir();
	static std::wstring ProjectContentDir();
	static std::wstring ProjectConfigDir();
	static std::wstring ProjectSavedDir();

	// Path Utilities
	static std::string ConvertRelativePathToFull(const std::string& RelativePath);
	static std::string NormalizePath(const std::string& Path);

	//  FPaths::Combine(L"Asset/Content/Scene", L"Default.Scene")
	static std::wstring Combine(const std::wstring& Base, const std::wstring& Child);


	static void CreateDir(const std::wstring& Path);


	static std::wstring ToWide(const std::string& Utf8Str);
	static std::string ToUtf8(const std::wstring& WideStr);

	static std::string ResolveAssetPath(const std::string& BaseFilePath, const std::string& TargetPath);
};
