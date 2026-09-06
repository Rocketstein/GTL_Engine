#pragma once

#include "Core/CoreTypes.h"

#include <filesystem>

// ============================================================
// FScriptPaths — Lua Script 전용 경로 정책/파일 IO 유틸
// - "Scripts/" 기준 경로 정책을 한 곳에 모아 Lua 코드와 Component 코드의 책임을 분리한다.
// ============================================================
class FScriptPaths
{
public:
	// 에디터/직렬화/런타임 내부에서 공통으로 쓰는 저장용 경로를 만든다.
	// 결과는 가능하면 "Scripts/Foo.lua" 형태의 UTF-8 상대 경로로 통일한다.
	static FString NormalizeScriptPath(const FString& ScriptPath);

	// 실제 파일 접근이 필요할 때 프로젝트 루트 기준 절대 경로로 변환한다.
	static std::filesystem::path ResolveScriptPath(const FString& ScriptPath);

	// 스크립트 로딩부가 파일 시스템 세부 사항을 몰라도 되도록 읽기까지 감싼다.
	static bool ReadScriptFile(const FString& ScriptPath, FString& OutScriptText, FString& OutError);

private:
	// 첫 번째 세그먼트가 Scripts 루트인지 대소문자 무시하고 판정한다.
	static bool IsScriptsRootPath(const std::filesystem::path& Path);

	// scripts/, SCRIPTS/ 등 어떤 입력이 와도 내부 표현은 Scripts/로 고정한다.
	static std::filesystem::path MakeCanonicalScriptsPath(const std::filesystem::path& Path);

	// 내부 정책의 핵심 단계
	// - 빈 경로는 그대로 빈 값 유지
	// - 절대 경로인데 프로젝트 루트 하위면 상대 경로로 되돌림
	// - Scripts/ prefix가 없으면 자동 부여
	// - scripts/SCRIPTS 등 대소문자는 canonical하게 Scripts로 맞춤
	static std::filesystem::path NormalizeScriptRelativePath(const FString& ScriptPath);
};
