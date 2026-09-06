#include "LogBuffer.h"

void FLogBuffer::Log(ELogLevel Level, const char* Category, const char* Message, const char* FormattedMessage)
{
	FLogEntry Entry;
	Entry.Level = Level;
	Entry.Category = Category != nullptr ? Category : "";
	Entry.Message = Message != nullptr ? Message : "";
	Entry.FormattedMessage = FormattedMessage != nullptr ? FormattedMessage : Entry.Message;
	LogEntries.push_back(std::move(Entry));
}

void FLogBuffer::Clear()
{
	LogEntries.clear();
}