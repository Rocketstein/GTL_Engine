#include "Name.h"
#include <algorithm>
#include <string>

FName::FName(char* pStr)
{
    ComparisonIndex = FNamePool::InsertComparisonMap(pStr);
    DisplayIndex = FNamePool::InsertDisplayMap(pStr);
}

FName::FName(FString str)
{
    ComparisonIndex = FNamePool::InsertComparisonMap(str);
    DisplayIndex = FNamePool::InsertDisplayMap(str);
}

int32 FName::Compare(const FName& Other) const
{
    FString str1 = this->GetFString();
    std::transform(str1.begin(), str1.end(), str1.begin(),
        [](unsigned char c) { return std::tolower(c); });   //소문자 전환

    FString str2 = Other.GetFString();
    std::transform(str2.begin(), str2.end(), str2.begin(),
        [](unsigned char c) { return std::tolower(c); });   //소문자 전환

    return str1.compare(str2);
}

FString FName::GetFString() const
{
    return FNamePool::GetDisplayFString(this->DisplayIndex);
}

int FNamePool::nextComparisonIndex = 0;
int FNamePool::nextDisplayIndex = 0;
FNamePool::~FNamePool()
{
}

int32 FNamePool::InsertDisplayMap(const FString str)
{

    FNamePool& instance = FNamePool::Instance();

    auto iter = instance.FNameDisplayEntryRev.find(str);
    if (iter == instance.FNameDisplayEntryRev.end()) {  //존재하지 않으면
        nextDisplayIndex++;
        instance.FNameDisplayEntry.insert({ nextDisplayIndex, str });
        instance.FNameDisplayEntryRev.insert({ str, nextDisplayIndex });
        return nextDisplayIndex;
    }
    else //존재하면
        return iter->second;
}

int32 FNamePool::InsertComparisonMap(const FString str)
{

    FNamePool& instance = FNamePool::Instance();
    FString key = str; 
    std::transform(key.begin(), key.end(), key.begin(),
        [](unsigned char c) { return std::tolower(c); });   //소문자 전환

    auto iter = instance.FNameComparisonEntry.find(key);
    if (iter == instance.FNameComparisonEntry.end()) {
        instance.FNameComparisonEntry.insert({ key, ++nextComparisonIndex });
        return nextComparisonIndex;
    }
    else return iter->second;
    
}

FString FNamePool::GetDisplayFString(const int32 index)
{
    
    auto iter = Instance().FNameDisplayEntry.find(index);
    if (iter == Instance().FNameDisplayEntry.end()) return "";
    return iter->second;
    
}



