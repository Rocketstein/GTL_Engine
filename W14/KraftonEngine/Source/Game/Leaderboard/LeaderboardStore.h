#pragma once

// ============================================================
// LeaderboardStore — file-backed top-N record store for the victory leaderboard.
//
// Lives under Source/Game (game-flow code, per project convention). Header-only
// so it needs no vcxproj entry; it is included only by GameLuaBindings.cpp, which
// exposes Submit / Load to Lua.
//
// Records persist as one plain line per entry under FPaths::SaveDir():
//     NAME TIMESEC REVIVES
// e.g. "ABC 83.42 2". Name is 3 uppercase initials (no spaces), so the
// space-delimited format round-trips safely. Kept text (not JSON) on purpose:
// the dataset is tiny and a line format is trivial to read, write and eyeball.
//
// Ranking (SortEntries): faster time first, fewer revives as the tiebreaker.
// To rank by "fewest exploited revives" instead, swap the two comparisons.
// ============================================================

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>

#include "Platform/Paths.h"

namespace GameLeaderboard
{
	struct FEntry
	{
		std::string Name;     // 3 uppercase initials
		float       TimeSec = 0.0f; // run time to victory, seconds
		int         Revives = 0;    // number of revives used
	};

	// Keep at most this many records on disk / in the view.
	static constexpr int MaxEntries = 6;

	inline std::wstring FilePath()
	{
		return FPaths::Combine(FPaths::SaveDir(), L"Leaderboard.txt");
	}

	inline void SortEntries(std::vector<FEntry>& Entries)
	{
		std::stable_sort(Entries.begin(), Entries.end(),
			[](const FEntry& A, const FEntry& B)
			{
				if (A.TimeSec != B.TimeSec) return A.TimeSec < B.TimeSec; // faster first
				return A.Revives < B.Revives;                            // fewer revives first
			});
	}

	inline std::vector<FEntry> Load()
	{
		std::vector<FEntry> Entries;
		std::ifstream File(FilePath().c_str());
		if (!File.is_open())
		{
			return Entries;
		}

		std::string Line;
		while (std::getline(File, Line))
		{
			if (Line.empty()) continue;
			std::istringstream In(Line);
			FEntry E;
			if (In >> E.Name >> E.TimeSec >> E.Revives)
			{
				Entries.push_back(E);
			}
		}
		SortEntries(Entries);
		return Entries;
	}

	inline void Save(std::vector<FEntry>& Entries)
	{
		SortEntries(Entries);
		if (static_cast<int>(Entries.size()) > MaxEntries)
		{
			Entries.resize(MaxEntries);
		}

		FPaths::CreateDir(FPaths::SaveDir());
		std::ofstream File(FilePath().c_str(), std::ios::trunc);
		if (!File.is_open())
		{
			return;
		}

		for (const FEntry& E : Entries)
		{
			File << E.Name << ' ' << E.TimeSec << ' ' << E.Revives << '\n';
		}
	}

	// Append a record and persist the (re-sorted, truncated) top list.
	inline void Submit(const std::string& Name, float TimeSec, int Revives)
	{
		std::vector<FEntry> Entries = Load();

		FEntry E;
		E.Name    = Name.empty() ? std::string("AAA") : Name;
		E.TimeSec = TimeSec;
		E.Revives = Revives;
		Entries.push_back(E);

		Save(Entries);
	}
}
