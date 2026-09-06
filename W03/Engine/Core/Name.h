#pragma once
#include "CoreTypes.h"

struct FName
{
    FName() = default;
    FName(char* pStr);
    FName(FString str);

    int32 Compare(const FName& Other) const; //FName::Compare 을 사용해서 두 FName 을 비교할 수도 있는데, 이 값이 다른 값보다 작으면 음수, 같으면 0, 크면 양수를 반환
    bool operator==(const FName& other) const { return this->ComparisonIndex == other.ComparisonIndex; }

    FString GetFString() const;

    int32 DisplayIndex = -1; //원래 문자 출력용
    int32 ComparisonIndex = -1;  //비교용
};

class FNamePool {

public:

    ~FNamePool();

    static int32 InsertDisplayMap(const FString str);
    static int32 InsertComparisonMap(const FString str);

    static FString GetDisplayFString(const int32 index);

private:

    static int32 nextDisplayIndex;
    static int32 nextComparisonIndex;

    TMap< FString, int32 > FNameComparisonEntry;  //FString 비교용 엔트리

    TMap< int32, FString > FNameDisplayEntry;  //FString 저장용 엔트리
    TMap< FString, int32 > FNameDisplayEntryRev;  //FString 저장용 엔트리
    

    static FNamePool& Instance(){
        static FNamePool _instance;
        return _instance;
    }

};