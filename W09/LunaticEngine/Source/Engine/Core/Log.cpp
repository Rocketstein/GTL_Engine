#include "Core/Log.h"
#include "Platform/Paths.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>

namespace
{
	constexpr const char* GLogLevelLabels[] = { "VERBOSE", "DEBUG", "INFO", "WARNING", "ERROR" };
}

const char* GetLogLevelLabel(ELogLevel Level)
{
	const uint32 Index = static_cast<uint32>(Level);
	return Index < std::size(GLogLevelLabels) ? GLogLevelLabels[Index] : "Unknown";
}

// ============================================================
// FDebugOutputDevice — VS 출력창 (OutputDebugStringA)
// ============================================================
class FDebugOutputDevice : public ILogOutputDevice
{
public:
	void Log(ELogLevel, const char*, const char*, const char* FormattedMessage) override
	{
		OutputDebugStringA(FormattedMessage != nullptr ? FormattedMessage : "");
		OutputDebugStringA("\n");
	}
};

// ============================================================
// FFileOutputDevice — 파일 기록 (Logs/Engine.log)
// ============================================================
class FFileOutputDevice : public ILogOutputDevice
{
public:
	FFileOutputDevice()
	{
		std::wstring LogPath = FPaths::LogDir() + L"Engine.log";
		FPaths::CreateDir(FPaths::LogDir());
		_wfopen_s(&File, LogPath.c_str(), L"w");
	}

	~FFileOutputDevice() override
	{
		if (File)
		{
			fclose(File);
			File = nullptr;
		}
	}

	void Log(ELogLevel, const char*, const char*, const char* FormattedMessage) override
	{
		if (!File) return;
		fprintf(File, "%s\n", FormattedMessage != nullptr ? FormattedMessage : "");
		fflush(File);
	}

private:
	FILE* File = nullptr;
};

// ============================================================
// FLogManager
// ============================================================

void FLogManager::Initialize()
{
	std::lock_guard<std::mutex> Lock(Mutex);
	if (bInitialized)
	{
		return;
	}

	DebugOutputDevice = new FDebugOutputDevice();
	FileOutputDevice = new FFileOutputDevice();
	OutputDevices.push_back(DebugOutputDevice);
	OutputDevices.push_back(FileOutputDevice);
	bInitialized = true;
}

void FLogManager::Shutdown()
{
	std::lock_guard<std::mutex> Lock(Mutex);
	if (!bInitialized)
	{
		return;
	}

	OutputDevices.clear();
	BufferedEntries.clear();

	delete FileOutputDevice;
	FileOutputDevice = nullptr;

	delete DebugOutputDevice;
	DebugOutputDevice = nullptr;
	bInitialized = false;
}

void FLogManager::AddOutputDevice(ILogOutputDevice* Device)
{
	if (!Device) return;
	std::lock_guard<std::mutex> Lock(Mutex);
	OutputDevices.push_back(Device);
	for (const FBufferedLogEntry& Entry : BufferedEntries)
	{
		Device->Log(Entry.Level, Entry.Category.c_str(), Entry.Message.c_str(), Entry.FormattedMessage.c_str());
	}
}

void FLogManager::RemoveOutputDevice(ILogOutputDevice* Device)
{
	if (!Device) return;
	std::lock_guard<std::mutex> Lock(Mutex);
	auto It = std::find(OutputDevices.begin(), OutputDevices.end(), Device);
	if (It != OutputDevices.end())
	{
		OutputDevices.erase(It);
	}
}

void FLogManager::SetMinimumLogLevel(ELogLevel InLevel)
{
	std::lock_guard<std::mutex> Lock(Mutex);
	MinimumLogLevel = InLevel;
}

ELogLevel FLogManager::GetMinimumLogLevel() const
{
	return MinimumLogLevel;
}

bool FLogManager::ShouldLog(ELogLevel InLevel) const
{
	return static_cast<uint8>(InLevel) >= static_cast<uint8>(MinimumLogLevel);
}

void FLogManager::LogMessage(ELogLevel Level, const char* Category, const char* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	LogMessageV(Level, Category, Fmt, Args);
	va_end(Args);
}

void FLogManager::LogMessageV(ELogLevel Level, const char* Category, const char* Fmt, va_list Args)
{
	if (!ShouldLog(Level) || Fmt == nullptr)
	{
		return;
	}

	char Message[2048];
	vsnprintf(Message, sizeof(Message), Fmt, Args);

	char FormattedMessage[2304];
	snprintf(FormattedMessage, sizeof(FormattedMessage), "[%s] %s: %s",
		GetLogLevelLabel(Level),
		(Category != nullptr && Category[0] != '\0') ? Category : "Log",
		Message);

	std::lock_guard<std::mutex> Lock(Mutex);
	FBufferedLogEntry Entry;
	Entry.Level = Level;
	Entry.Category = (Category != nullptr) ? Category : "Log";
	Entry.Message = Message;
	Entry.FormattedMessage = FormattedMessage;
	BufferedEntries.push_back(std::move(Entry));
	constexpr size_t MaxBufferedEntries = 2048;
	if (BufferedEntries.size() > MaxBufferedEntries)
	{
		BufferedEntries.erase(BufferedEntries.begin());
	}

	for (ILogOutputDevice* Device : OutputDevices)
	{
		if (Device)
		{
			Device->Log(Level, Category, Message, FormattedMessage);
		}
	}
}
