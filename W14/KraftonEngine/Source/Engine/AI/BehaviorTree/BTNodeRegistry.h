#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/FName.h"

class FBTNode_Base;
class FBTDecorator_Base;
class FBTService_Base;
class UBehaviorTreeComponent;
struct FBTGraphNode;
struct FBTAuxNode;

// =============================================================================
//  BT 노드 카탈로그(레지스트리) — 동작/데코레이터/서비스의 단일 정의 지점
// =============================================================================
//  컴파일러는 이 카탈로그의 Make 팩토리로 런타임 노드를 만들고, 에디터(팔레트/메뉴/
//  인스펙터)는 같은 카탈로그를 읽어 목록을 자동 생성한다.
//  → 새 동작 = 런타임 클래스 1개 + 여기 1줄. (3곳 분산 → 1곳)
//  Make 팩토리는 그래프 노드/보조노드를 받아 파라미터를 주입한다(파라미터화 leaf 지원).
// =============================================================================

// leaf 가 쓰는 파라미터 힌트 — 인스펙터가 어떤 필드를 노출할지 결정.
enum EBTLeafParam
{
    BTParam_None   = 0,
    BTParam_Key    = 1, // KeyValue (블랙보드 키 / verb 이름 등)
    BTParam_Float  = 2, // FloatValue
    BTParam_Bool   = 4, // BoolValue
    BTParam_String = 8  // StringValue
};

struct FBTLeafType
{
    FName   Id;
    bool    bIsCondition = false;
    FString Category;
    FString Display;
    FBTNode_Base* (*Make)(UBehaviorTreeComponent&, const FBTGraphNode&) = nullptr;
    int     ParamFlags = 0; // EBTLeafParam 비트마스크
};

struct FBTDecoratorType
{
    FName   Id;
    FString Display;
    FBTDecorator_Base* (*Make)(UBehaviorTreeComponent&, const FBTAuxNode&) = nullptr;
};

struct FBTServiceType
{
    FName   Id;
    FString Display;
    FBTService_Base* (*Make)(UBehaviorTreeComponent&, const FBTAuxNode&) = nullptr;
};

const TArray<FBTLeafType>&      GetBTLeafCatalog();
const TArray<FBTDecoratorType>& GetBTDecoratorCatalog();
const TArray<FBTServiceType>&   GetBTServiceCatalog();

const FBTLeafType*      FindBTLeafType(const FName& Id);
const FBTDecoratorType* FindBTDecoratorType(const FName& Id);
const FBTServiceType*   FindBTServiceType(const FName& Id);
