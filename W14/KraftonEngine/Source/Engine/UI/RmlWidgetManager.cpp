#include "UI/RmlWidgetManager.h"

#include "Object/Object.h"
#include "Platform/Paths.h"
#include "UI/RmlWidgetAsset.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
	FString ToLowerCopy(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(),
			[](unsigned char Ch)
			{
				return static_cast<char>(std::tolower(Ch));
			});
		return Value;
	}

	std::filesystem::path ResolveProjectPath(const FString& Path)
	{
		std::filesystem::path Result(FPaths::ToWide(Path));
		if (!Result.is_absolute())
		{
			Result = std::filesystem::path(FPaths::RootDir()) / Result;
		}
		return Result.lexically_normal();
	}

	FString MakeProjectRelativePath(const std::filesystem::path& Path)
	{
		return FPaths::MakeProjectRelative(FPaths::ToUtf8(Path.generic_wstring()));
	}

	FString ReadTextFile(const std::filesystem::path& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		if (!File.is_open())
		{
			return FString();
		}

		std::ostringstream Buffer;
		Buffer << File.rdbuf();
		return Buffer.str();
	}

	FString FindLinkedStyleSheet(const std::filesystem::path& DocumentPath)
	{
		const FString Text = ReadTextFile(DocumentPath);
		if (Text.empty())
		{
			return FString();
		}

		size_t SearchPos = 0;
		while (true)
		{
			const size_t LinkPos = Text.find("<link", SearchPos);
			if (LinkPos == FString::npos)
			{
				return FString();
			}

			const size_t LinkEnd = Text.find('>', LinkPos);
			if (LinkEnd == FString::npos)
			{
				return FString();
			}

			const FString LinkText = Text.substr(LinkPos, LinkEnd - LinkPos + 1);
			const FString LowerLinkText = ToLowerCopy(LinkText);
			if (LowerLinkText.find("text/rcss") != FString::npos)
			{
				const size_t HrefPos = LowerLinkText.find("href");
				if (HrefPos != FString::npos)
				{
					const size_t QuoteStart = LinkText.find_first_of("\"'", HrefPos);
					if (QuoteStart != FString::npos)
					{
						const char Quote = LinkText[QuoteStart];
						const size_t QuoteEnd = LinkText.find(Quote, QuoteStart + 1);
						if (QuoteEnd != FString::npos)
						{
							const FString Href = LinkText.substr(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
							std::filesystem::path StylePath(FPaths::ToWide(Href));
							if (StylePath.is_relative())
							{
								StylePath = DocumentPath.parent_path() / StylePath;
							}
							return MakeProjectRelativePath(StylePath.lexically_normal());
						}
					}
				}
			}

			SearchPos = LinkEnd + 1;
		}
	}

	bool DocumentLinksStyleSheet(
		const std::filesystem::path& DocumentPath,
		const std::filesystem::path& ExpectedStylePath)
	{
		const FString Text = ReadTextFile(DocumentPath);
		if (Text.empty())
		{
			return false;
		}

		const FString ExpectedPath = ToLowerCopy(
			FPaths::ToUtf8(ExpectedStylePath.lexically_normal().generic_wstring()));
		size_t SearchPos = 0;
		while (true)
		{
			const size_t LinkPos = Text.find("<link", SearchPos);
			if (LinkPos == FString::npos)
			{
				return false;
			}

			const size_t LinkEnd = Text.find('>', LinkPos);
			if (LinkEnd == FString::npos)
			{
				return false;
			}

			const FString LinkText = Text.substr(LinkPos, LinkEnd - LinkPos + 1);
			const FString LowerLinkText = ToLowerCopy(LinkText);
			if (LowerLinkText.find("text/rcss") != FString::npos)
			{
				const size_t HrefPos = LowerLinkText.find("href");
				const size_t QuoteStart = HrefPos == FString::npos
					? FString::npos
					: LinkText.find_first_of("\"'", HrefPos);
				if (QuoteStart != FString::npos)
				{
					const char Quote = LinkText[QuoteStart];
					const size_t QuoteEnd = LinkText.find(Quote, QuoteStart + 1);
					if (QuoteEnd != FString::npos)
					{
						std::filesystem::path StylePath(
							FPaths::ToWide(LinkText.substr(QuoteStart + 1, QuoteEnd - QuoteStart - 1)));
						if (StylePath.is_relative())
						{
							StylePath = DocumentPath.parent_path() / StylePath;
						}

						const FString LinkedPath = ToLowerCopy(
							FPaths::ToUtf8(StylePath.lexically_normal().generic_wstring()));
						if (LinkedPath == ExpectedPath)
						{
							return true;
						}
					}
				}
			}

			SearchPos = LinkEnd + 1;
		}
	}

	std::filesystem::path FindDocumentForStyleSheet(const std::filesystem::path& StylePath)
	{
		std::filesystem::path CandidateDocument = StylePath;
		CandidateDocument.replace_extension(L".rml");
		if (std::filesystem::exists(CandidateDocument))
		{
			return CandidateDocument;
		}

		std::error_code Error;
		for (const std::filesystem::directory_entry& Entry :
			std::filesystem::directory_iterator(StylePath.parent_path(), Error))
		{
			if (Error || !Entry.is_regular_file())
			{
				continue;
			}

			const std::filesystem::path& DocumentPath = Entry.path();
			const FString Extension = ToLowerCopy(FPaths::ToUtf8(DocumentPath.extension().wstring()));
			if ((Extension == ".rml" || Extension == ".html" || Extension == ".htm") &&
				DocumentLinksStyleSheet(DocumentPath, StylePath))
			{
				return DocumentPath;
			}
		}

		return {};
	}

	bool IsRmlDocumentExtension(const std::filesystem::path& Path)
	{
		const FString Ext = ToLowerCopy(FPaths::ToUtf8(Path.extension().wstring()));
		return Ext == ".rml" || Ext == ".html" || Ext == ".htm";
	}

	bool IsRcssExtension(const std::filesystem::path& Path)
	{
		return ToLowerCopy(FPaths::ToUtf8(Path.extension().wstring())) == ".rcss";
	}
}

URmlWidgetAsset* FRmlWidgetManager::Load(const FString& Path)
{
	const std::filesystem::path SourcePath = ResolveProjectPath(Path);
	const FString NormalizedSourcePath = MakeProjectRelativePath(SourcePath);

	auto It = LoadedWidgets.find(NormalizedSourcePath);
	if (It != LoadedWidgets.end())
	{
		if (IsValid(It->second))
		{
			return It->second;
		}
		LoadedWidgets.erase(It);
	}

	if (!std::filesystem::exists(SourcePath) || !std::filesystem::is_regular_file(SourcePath))
	{
		return nullptr;
	}

	FString DocumentPath;
	FString StyleSheetPath;
	if (IsRmlDocumentExtension(SourcePath))
	{
		DocumentPath = NormalizedSourcePath;
		StyleSheetPath = FindLinkedStyleSheet(SourcePath);
	}
	else if (IsRcssExtension(SourcePath))
	{
		StyleSheetPath = NormalizedSourcePath;

		const std::filesystem::path LinkedDocument = FindDocumentForStyleSheet(SourcePath);
		if (!LinkedDocument.empty())
		{
			DocumentPath = MakeProjectRelativePath(LinkedDocument);
		}
	}
	else
	{
		return nullptr;
	}

	if (StyleSheetPath.empty())
	{
		return nullptr;
	}

	URmlWidgetAsset* Asset = UObjectManager::Get().CreateObject<URmlWidgetAsset>();
	Asset->SetSourcePath(NormalizedSourcePath);
	Asset->SetDocumentPath(DocumentPath);
	Asset->SetStyleSheetPath(StyleSheetPath);
	Asset->SetFName(FName(FPaths::ToUtf8(SourcePath.stem().wstring())));

	LoadedWidgets.emplace(NormalizedSourcePath, Asset);
	return Asset;
}

URmlWidgetAsset* FRmlWidgetManager::Find(const FString& Path) const
{
	const FString NormalizedPath = MakeProjectRelativePath(ResolveProjectPath(Path));
	auto It = LoadedWidgets.find(NormalizedPath);
	if (It == LoadedWidgets.end())
	{
		return nullptr;
	}
	return IsValid(It->second) ? It->second : nullptr;
}

void FRmlWidgetManager::ClearCache()
{
	LoadedWidgets.clear();
}

void FRmlWidgetManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Pair : LoadedWidgets)
	{
		Collector.AddReferencedObject(Pair.second, "FRmlWidgetManager.LoadedWidgets");
	}
}
