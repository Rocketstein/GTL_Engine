#pragma once
#include "LogOutputDevice.h"

struct FLogEntry
{
	ELogLevel Level = ELogLevel::Info;
	FString Category;
	FString Message;
	FString FormattedMessage;
};