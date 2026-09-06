#pragma once
#include <string>

enum class EGameState
{
    Ready,
    Select,
    Game,
    Result
};

enum class EMatchResult
{
    None,
    Player1Win,
    Player2Win,
    Draw
};

struct CharacterData
{
    int Id;
    std::string Name;
    std::wstring SpritePath;
};

struct PlayerSelection
{
    int CharacterIndex = 0;
};