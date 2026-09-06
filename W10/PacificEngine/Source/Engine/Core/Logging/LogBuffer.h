#pragma once
#include "LogEntry.h"
#include "LogOutputDevice.h"

class FLogBuffer : public ILogOutputDevice
{
public:
	void Log(ELogLevel Level, const char* Category, const char* Message, const char* FormattedMessage) override;

	const TArray<FLogEntry>& GetEntries() const { return LogEntries; }
	void Clear();

private:
	TArray<FLogEntry> LogEntries;
};