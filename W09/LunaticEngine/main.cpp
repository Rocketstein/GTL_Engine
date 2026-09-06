#include <Windows.h>
#include <string>
#include "Engine/Runtime/Launch.h"
#include "Engine/Runtime/EngineLoop.h"

namespace
{
	bool HasCommandLineFlag(const char* Flag)
	{
		LPSTR CmdLine = GetCommandLineA();
		if (!CmdLine) return false;
		const std::string CmdLineStr = CmdLine;
		return CmdLineStr.find(Flag) != std::string::npos;
	}

	std::string GetCommandLineValue(const char* Key)
	{
		LPSTR CmdLine = GetCommandLineA();
		if (!CmdLine) return "";

		const std::string CmdLineStr = CmdLine;
		const std::string Prefix = std::string(Key) + "=";
		const size_t Start = CmdLineStr.find(Prefix);
		if (Start == std::string::npos)
		{
			return "";
		}

		size_t ValueStart = Start + Prefix.length();
		if (ValueStart < CmdLineStr.length() && CmdLineStr[ValueStart] == '"')
		{
			const size_t ValueEnd = CmdLineStr.find('"', ValueStart + 1);
			if (ValueEnd == std::string::npos)
			{
				return CmdLineStr.substr(ValueStart + 1);
			}
			return CmdLineStr.substr(ValueStart + 1, ValueEnd - ValueStart - 1);
		}

		const size_t ValueEnd = CmdLineStr.find(' ', ValueStart);
		return CmdLineStr.substr(ValueStart, ValueEnd == std::string::npos ? std::string::npos : ValueEnd - ValueStart);
	}

	int RunCookCommandlet(HINSTANCE hInstance, int nShowCmd)
	{
		FEngineLoop EngineLoop;
		if (!EngineLoop.Init(hInstance, nShowCmd))
		{
			return -1;
		}

		std::string CookOutput = GetCommandLineValue("-cookoutput");
		if (CookOutput.empty())
		{
			CookOutput = GetCommandLineValue("--cookoutput");
		}

		const int Result = EngineLoop.RunCookOnly(CookOutput);
		EngineLoop.Shutdown();
		return Result;
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nShowCmd)
{
	if (HasCommandLineFlag("--cook") || HasCommandLineFlag("-cook"))
	{
		return RunCookCommandlet(hInstance, nShowCmd);
	}

	return Launch(hInstance, nShowCmd);
}
