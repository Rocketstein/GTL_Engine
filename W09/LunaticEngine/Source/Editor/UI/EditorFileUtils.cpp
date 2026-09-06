#include "Editor/UI/EditorFileUtils.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <filesystem>
#include <vector>

#include "Engine/Platform/Paths.h"

namespace
{
	std::vector<wchar_t> BuildFileBuffer(const wchar_t* DefaultFileName)
	{
		std::vector<wchar_t> Buffer(32768, L'\0');
		if (!DefaultFileName)
		{
			return Buffer;
		}

		wcsncpy_s(Buffer.data(), Buffer.size(), DefaultFileName, _TRUNCATE);
		return Buffer;
	}

	FString RunFileDialog(const FEditorFileDialogOptions& InOptions, bool bOpenDialog)
	{
		std::vector<wchar_t> FileBuffer = BuildFileBuffer(InOptions.DefaultFileName);
		OPENFILENAMEW DialogOptions{};
		DialogOptions.lStructSize = sizeof(DialogOptions);
		DialogOptions.hwndOwner = static_cast<HWND>(InOptions.OwnerWindowHandle);
		DialogOptions.lpstrFilter = InOptions.Filter;
		DialogOptions.nFilterIndex = 1;
		DialogOptions.lpstrFile = FileBuffer.data();
		DialogOptions.nMaxFile = static_cast<DWORD>(FileBuffer.size());
		DialogOptions.lpstrTitle = InOptions.Title;
		DialogOptions.lpstrDefExt = InOptions.DefaultExtension;
		DialogOptions.lpstrInitialDir = InOptions.InitialDirectory;
		DialogOptions.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR;
		if (InOptions.bPathMustExist)
		{
			DialogOptions.Flags |= OFN_PATHMUSTEXIST;
		}
		if (InOptions.bFileMustExist)
		{
			DialogOptions.Flags |= OFN_FILEMUSTEXIST;
		}
		if (InOptions.bPromptOverwrite)
		{
			DialogOptions.Flags |= OFN_OVERWRITEPROMPT;
		}

		const BOOL bSucceeded = bOpenDialog
			? GetOpenFileNameW(&DialogOptions)
			: GetSaveFileNameW(&DialogOptions);
		if (!bSucceeded)
		{
			return FString();
		}

		const std::filesystem::path AbsolutePath = std::filesystem::path(FileBuffer.data()).lexically_normal();
		if (InOptions.bReturnRelativeToProjectRoot)
		{
			const std::filesystem::path ProjectRoot(FPaths::RootDir());
			return FPaths::ToUtf8(AbsolutePath.lexically_relative(ProjectRoot).generic_wstring());
		}

		return FPaths::ToUtf8(AbsolutePath.generic_wstring());
	}
}

namespace FEditorFileUtils
{
	FString OpenFileDialog(const FEditorFileDialogOptions& InOptions)
	{
		return RunFileDialog(InOptions, true);
	}

	FString SaveFileDialog(const FEditorFileDialogOptions& InOptions)
	{
		return RunFileDialog(InOptions, false);
	}

	bool OpenPath(const std::filesystem::path& InPath)
	{
		const std::filesystem::path NormalizedPath = InPath.lexically_normal();
		if (!std::filesystem::exists(NormalizedPath))
		{
			return false;
		}

		const HINSTANCE Result = ShellExecuteW(
			nullptr,
			L"open",
			NormalizedPath.c_str(),
			nullptr,
			nullptr,
			SW_SHOWNORMAL);

		return reinterpret_cast<INT_PTR>(Result) > 32;
	}

	bool RevealInExplorer(const std::filesystem::path& InPath)
	{
		const std::filesystem::path NormalizedPath = InPath.lexically_normal();
		if (!std::filesystem::exists(NormalizedPath))
		{
			return false;
		}

		const std::wstring Parameters = L"/select,\"" + NormalizedPath.wstring() + L"\"";
		const HINSTANCE Result = ShellExecuteW(
			nullptr,
			L"open",
			L"explorer.exe",
			Parameters.c_str(),
			nullptr,
			SW_SHOWNORMAL);

		return reinterpret_cast<INT_PTR>(Result) > 32;
	}

	bool DeletePath(const std::filesystem::path& InPath)
	{
		const std::filesystem::path NormalizedPath = InPath.lexically_normal();
		if (!std::filesystem::exists(NormalizedPath))
		{
			return false;
		}

		std::error_code ErrorCode;
		if (std::filesystem::is_directory(NormalizedPath))
		{
			std::filesystem::remove_all(NormalizedPath, ErrorCode);
		}
		else
		{
			std::filesystem::remove(NormalizedPath, ErrorCode);
		}

		return !ErrorCode && !std::filesystem::exists(NormalizedPath);
	}
}
