#pragma once

#include "Core/EngineTypes.h"

namespace AsciiUtils
{
	inline char ToLower(char Character)
	{
		return (Character >= 'A' && Character <= 'Z')
			? static_cast<char>(Character + ('a' - 'A'))
			: Character;
	}

	inline char ToUpper(char Character)
	{
		return (Character >= 'a' && Character <= 'z')
			? static_cast<char>(Character - ('a' - 'A'))
			: Character;
	}

	inline bool IsDigit(char Character)
	{
		return Character >= '0' && Character <= '9';
	}

	inline bool IsLower(char Character)
	{
		return Character >= 'a' && Character <= 'z';
	}

	inline bool IsUpper(char Character)
	{
		return Character >= 'A' && Character <= 'Z';
	}

	inline bool IsAlpha(char Character)
	{
		return IsLower(Character) || IsUpper(Character);
	}

	inline bool IsAlnum(char Character)
	{
		return IsAlpha(Character) || IsDigit(Character);
	}

	inline bool IsSpace(char Character)
	{
		return Character == ' '
			|| Character == '\t'
			|| Character == '\n'
			|| Character == '\r'
			|| Character == '\f'
			|| Character == '\v';
	}

	inline void ToLowerInPlace(FString& Value)
	{
		for (char& Character : Value)
		{
			Character = ToLower(Character);
		}
	}

	inline void ToUpperInPlace(FString& Value)
	{
		for (char& Character : Value)
		{
			Character = ToUpper(Character);
		}
	}
}
