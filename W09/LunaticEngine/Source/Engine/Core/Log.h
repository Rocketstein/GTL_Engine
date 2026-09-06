#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include <cstdarg>
#include <mutex>

enum class ELogLevel : uint8
{
	Verbose = 0,
	Debug,
	Info,
	Warning,
	Error
};

const char* GetLogLevelLabel(ELogLevel Level);

// ============================================================
// ILogOutputDevice — 로그 출력 대상 인터페이스
// ============================================================
class ILogOutputDevice
{
public:
	virtual ~ILogOutputDevice() = default;
	virtual void Log(ELogLevel Level, const char* Category, const char* Message, const char* FormattedMessage) = 0;
};

struct FBufferedLogEntry
{
	ELogLevel Level = ELogLevel::Info;
	FString Category;
	FString Message;
	FString FormattedMessage;
};

// ============================================================
// FLogManager — 중앙 로그 관리 싱글턴
//
// 문자열을 1회 포맷한 뒤 등록된 모든 OutputDevice에 디스패치한다.
// 기본 디바이스: VS 출력창(OutputDebugStringA), 파일(Logs/Engine.log)
// 에디터 콘솔은 Editor 레이어에서 AddOutputDevice로 등록한다.
// ============================================================
class FLogManager : public TSingleton<FLogManager>
{
	friend class TSingleton<FLogManager>;

public:
	void Initialize();
	void Shutdown();

	void AddOutputDevice(ILogOutputDevice* Device);
	void RemoveOutputDevice(ILogOutputDevice* Device);

	void SetMinimumLogLevel(ELogLevel InLevel);
	ELogLevel GetMinimumLogLevel() const;
	bool ShouldLog(ELogLevel InLevel) const;

	void LogMessage(ELogLevel Level, const char* Category, const char* Fmt, ...);
	void LogMessageV(ELogLevel Level, const char* Category, const char* Fmt, va_list Args);

private:
	FLogManager() = default;

	std::mutex Mutex;
	TArray<ILogOutputDevice*> OutputDevices;
	TArray<FBufferedLogEntry> BufferedEntries;
	ELogLevel MinimumLogLevel = ELogLevel::Verbose;
	bool bInitialized = false;

	// 내장 디바이스 (Initialize에서 생성, Shutdown에서 해제)
	ILogOutputDevice* DebugOutputDevice = nullptr;
	ILogOutputDevice* FileOutputDevice = nullptr;
};

// ============================================================
// UE_LOG 매크로 — Engine 레이어에서 정의
// ============================================================
#define UE_LOG(Format, ...) \
	FLogManager::Get().LogMessage(ELogLevel::Info, "Log", Format, ##__VA_ARGS__)

#define UE_LOG_CATEGORY(Category, Level, Format, ...) \
	FLogManager::Get().LogMessage(ELogLevel::Level, #Category, Format, ##__VA_ARGS__)
