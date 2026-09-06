// 직렬화 영역의 세부 동작을 구현합니다.
#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <unordered_map>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Component/LightComponentBase.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/CameraComponent.h"
#include "GameFramework/StaticMeshActor.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Object/FName.h"
#include "Profiling/PlatformTime.h"

// ---- JSON vector helpers ---------------------------------------------------

static void WriteVec3(json::JSON& Obj, const char* Key, const FVector& V)
{
    json::JSON arr = json::Array();
    arr.append(static_cast<double>(V.X));
    arr.append(static_cast<double>(V.Y));
    arr.append(static_cast<double>(V.Z));
    Obj[Key] = arr;
}

static FVector ReadVec3(json::JSON& Arr)
{
    FVector out(0, 0, 0);
    int i = 0;
    for (auto& e : Arr.ArrayRange())
    {
        if (i == 0)
            out.X = static_cast<float>(e.ToFloat());
        else if (i == 1)
            out.Y = static_cast<float>(e.ToFloat());
        else if (i == 2)
            out.Z = static_cast<float>(e.ToFloat());
        ++i;
    }
    return out;
}

// May have to remove this block
static json::JSON WriteMatrix(const FMatrix& Matrix)
{
    json::JSON arr = json::Array();
    for (float Value : Matrix.Data)
    {
        arr.append(static_cast<double>(Value));
    }
    return arr;
}

static bool ReadMatrix(json::JSON& Arr, FMatrix& OutMatrix)
{
    if (Arr.JSONType() != json::JSON::Class::Array || Arr.size() != 16)
    {
        return false;
    }

    int32 i = 0;
    for (auto& e : Arr.ArrayRange())
    {
        OutMatrix.Data[i++] = static_cast<float>(e.ToFloat());
    }
    return true;
}
// May have to remove this block

// ---------------------------------------------------------------------------

namespace SceneKeys
{
static constexpr const char* Version = "Version";
static constexpr const char* Name = "Name";
static constexpr const char* ClassName = "ClassName";
static constexpr const char* WorldType = "WorldType";
static constexpr const char* ContextName = "ContextName";
static constexpr const char* ContextHandle = "ContextHandle";
static constexpr const char* Actors = "Actors";
static constexpr const char* Visible = "bVisible";
static constexpr const char* ActorFolder = "ActorFolder";
static constexpr const char* ActorFolders = "ActorFolders";
static constexpr const char* RootComponent = "RootComponent";
static constexpr const char* NonSceneComponents = "NonSceneComponents";
static constexpr const char* Properties = "Properties";
static constexpr const char* Children = "Children";
static constexpr const char* ShadowResolution = "Shadow Resolution";
static constexpr const char* BoneLocalMatrices = "Bone Local Matrices"; // May have to remove this block
} // namespace SceneKeys

static const char* WorldTypeToString(EWorldType Type)
{
    switch (Type)
    {
    case EWorldType::Game:
        return "Game";
    case EWorldType::PIE:
        return "PIE";
    default:
        return "Editor";
    }
}

static EWorldType StringToWorldType(const string& Str)
{
    if (Str == "Game")
        return EWorldType::Game;
    if (Str == "PIE")
        return EWorldType::PIE;
    return EWorldType::Editor;
}

// ============================================================
// Save
// ============================================================

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, FWorldContext& WorldContext, UCameraComponent* PerspectiveCam)
{
    using namespace json;

    if (!WorldContext.World)
        return;

    string FinalName = InSceneName.empty()
                           ? "Save_" + GetCurrentTimeStamp()
                           : InSceneName;

    std::wstring SceneDir = GetSceneDirectory();
    std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
    std::filesystem::create_directories(SceneDir);

    JSON Root = SerializeWorld(WorldContext.World, WorldContext, PerspectiveCam);
    Root[SceneKeys::Version] = 3;
    Root[SceneKeys::Name] = FinalName;

    std::ofstream File(FileDestination);
    if (File.is_open())
    {
        File << Root.dump();
        File.flush();
        File.close();
    }
}

json::JSON FSceneSaveManager::SerializeWorld(UWorld* World, const FWorldContext& Ctx, UCameraComponent* PerspectiveCam)
{
    using namespace json;
    JSON w = json::Object();
    w[SceneKeys::ClassName] = World->GetClass()->GetName();
    w[SceneKeys::WorldType] = WorldTypeToString(Ctx.WorldType);
    w[SceneKeys::ContextName] = Ctx.ContextName;
    w[SceneKeys::ContextHandle] = Ctx.ContextHandle.ToString();

    JSON ActorFolders = json::Array();
    TSet<FString> SeenFolders;
    auto AppendFolder = [&](const FString& FolderPath)
    {
        if (FolderPath.empty() || SeenFolders.find(FolderPath) != SeenFolders.end())
        {
            return;
        }

        SeenFolders.insert(FolderPath);
        ActorFolders.append(FolderPath);
    };

    for (const FString& FolderPath : World->GetEditorActorFolders())
    {
        AppendFolder(FolderPath);
    }
    for (AActor* Actor : World->GetActors())
    {
        if (Actor)
        {
            AppendFolder(Actor->GetEditorFolderPath());
        }
    }
    if (ActorFolders.size() > 0)
    {
        w[SceneKeys::ActorFolders] = ActorFolders;
    }

    // ---- Actors: serialize ----
    JSON Actors = json::Array();
    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
            continue;
        JSON a = SerializeActor(Actor);
        Actors.append(a);
    }
    w[SceneKeys::Actors] = Actors;

    // ---- Perspective camera ----
    JSON cam = SerializeCamera(PerspectiveCam);
    if (cam.size() > 0)
    {
        w["PerspectiveCamera"] = cam;
    }

    return w;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor)
{
    using namespace json;
    JSON a = json::Object();
    a[SceneKeys::ClassName] = Actor->GetClass()->GetName();
    a[SceneKeys::Name] = Actor->GetFName().ToString();
    a["Tag"] = Actor->GetActorTag().ToString();
    a[SceneKeys::Visible] = Actor->IsVisible();
    if (!Actor->GetEditorFolderPath().empty())
    {
        a[SceneKeys::ActorFolder] = Actor->GetEditorFolderPath();
    }

    JSON ActorProperties = SerializeActorProperties(Actor);
    if (ActorProperties.size() > 0)
    {
        a[SceneKeys::Properties] = ActorProperties;
    }

    // RootComponent 트리 직렬화
    if (Actor->GetRootComponent())
    {
        a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Actor->GetRootComponent());
    }

    // Non-scene components
    JSON NonScene = json::Array();
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (!Comp)
            continue;
        if (Comp->IsA<USceneComponent>())
            continue;

        JSON c = json::Object();
        c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
        c[SceneKeys::Name] = Comp->GetFName().ToString();
        c[SceneKeys::Properties] = SerializeProperties(Comp);
        NonScene.append(c);
    }
    a[SceneKeys::NonSceneComponents] = NonScene;

    return a;
}

json::JSON FSceneSaveManager::SerializeActorProperties(AActor* Actor)
{
    using namespace json;
    JSON props = json::Object();

    if (!Actor)
    {
        return props;
    }

    TArray<FPropertyDescriptor> Descriptors;
    Actor->GetEditableProperties(Descriptors);

    for (const FPropertyDescriptor& Prop : Descriptors)
    {
        props[Prop.Name] = SerializePropertyValue(Prop);
    }

    return props;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp)
{
    using namespace json;
    JSON c = json::Object();
    c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
    c[SceneKeys::Name] = Comp->GetFName().ToString();
    c[SceneKeys::Properties] = SerializeProperties(Comp);

    JSON Children = json::Array();
    for (USceneComponent* Child : Comp->GetChildren())
    {
        if (!Child)
            continue;
        Children.append(SerializeSceneComponentTree(Child));
    }
    c[SceneKeys::Children] = Children;

    return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UActorComponent* Comp)
{
    using namespace json;
    JSON props = json::Object();

    TArray<FPropertyDescriptor> Descriptors;
    Comp->GetEditableProperties(Descriptors);

    for (const auto& Prop : Descriptors)
    {
        // if (Prop.Name == "Static Mesh") continue; // Primitives 블록에 이미 저장됨
        props[Prop.Name] = SerializePropertyValue(Prop);
    }

    if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Comp))
    {
        props[SceneKeys::ShadowResolution] = JSON(static_cast<int32>(LightComponent->GetShadowResolution()));
    }

	// May have to remove this block
    if (USkeletalMeshComponent* SkeletalComponent = Cast<USkeletalMeshComponent>(Comp))
    {
        JSON BoneMatrices = json::Array();
        const int32 BoneCount = SkeletalComponent->GetNumBones();
        for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            BoneMatrices.append(WriteMatrix(SkeletalComponent->GetBoneLocalMatrix(BoneIndex)));
        }
        props[SceneKeys::BoneLocalMatrices] = BoneMatrices;
    }
    // May have to remove this block

    return props;
}

json::JSON FSceneSaveManager::SerializePropertyValue(const FPropertyDescriptor& Prop)
{
    using namespace json;

    switch (Prop.Type)
    {
    case EPropertyType::Bool:
        return JSON(*static_cast<bool*>(Prop.ValuePtr));

    case EPropertyType::Int:
    case EPropertyType::Enum:
        return JSON(*static_cast<int32*>(Prop.ValuePtr));

    case EPropertyType::Float:
        return JSON(static_cast<double>(*static_cast<float*>(Prop.ValuePtr)));

    case EPropertyType::Vec2:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        JSON arr = json::Array();
        arr.append(static_cast<double>(v[0]));
        arr.append(static_cast<double>(v[1]));
        return arr;
    }
    case EPropertyType::Vec3:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        JSON arr = json::Array();
        arr.append(static_cast<double>(v[0]));
        arr.append(static_cast<double>(v[1]));
        arr.append(static_cast<double>(v[2]));
        return arr;
    }
    case EPropertyType::Rotator:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        JSON arr = json::Array();
        arr.append(static_cast<double>(v[0]));
        arr.append(static_cast<double>(v[1]));
        arr.append(static_cast<double>(v[2]));
        return arr;
    }
    case EPropertyType::Vec4:
    case EPropertyType::Color4:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        JSON arr = json::Array();
        arr.append(static_cast<double>(v[0]));
        arr.append(static_cast<double>(v[1]));
        arr.append(static_cast<double>(v[2]));
        arr.append(static_cast<double>(v[3]));
        return arr;
    }
    case EPropertyType::String:
    case EPropertyType::StaticMeshRef:
    case EPropertyType::SkeletalMeshRef:
    case EPropertyType::ComponentRef:
        return JSON(*static_cast<FString*>(Prop.ValuePtr));

    case EPropertyType::MaterialSlot:
    {
        const FMaterialSlot* Slot = static_cast<const FMaterialSlot*>(Prop.ValuePtr);
        JSON obj = json::Object();
        obj["Path"] = JSON(Slot->Path);
        return obj;
    }

    case EPropertyType::ByteBool:
        return JSON(static_cast<bool>(*static_cast<uint8_t*>(Prop.ValuePtr) != 0));

    case EPropertyType::Name:
        return JSON(static_cast<FName*>(Prop.ValuePtr)->ToString());

    default:
        return JSON();
    }
}

// ---- Camera helpers ----

json::JSON FSceneSaveManager::SerializeCamera(UCameraComponent* Cam)
{
    using namespace json;
    JSON cam = json::Object();
    if (!Cam)
        return cam;

    const FMatrix& M = Cam->GetWorldMatrix();
    WriteVec3(cam, "Location", M.GetLocation());
    WriteVec3(cam, "Rotation", M.GetEuler());

    const FCameraState& S = Cam->GetCameraState();
    cam["FOV"] = static_cast<double>(S.FOV);
    cam["NearClip"] = static_cast<double>(S.NearZ);
    cam["FarClip"] = static_cast<double>(S.FarZ);

    return cam;
}

void FSceneSaveManager::DeserializeCamera(json::JSON& CameraJSON, FPerspectiveCameraData& OutCam)
{
    using namespace json;
    if (CameraJSON.JSONType() == JSON::Class::Null)
        return;

    if (CameraJSON.hasKey("Location"))
        OutCam.Location = ReadVec3(CameraJSON["Location"]);
    if (CameraJSON.hasKey("Rotation"))
        OutCam.Rotation = ReadVec3(CameraJSON["Rotation"]);
    if (CameraJSON.hasKey("FOV"))
    {
        auto& Val = CameraJSON["FOV"];
        float fov = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
        // 엔진 내부는 라디안 — π(~3.14)를 넘으면 degree로 간주하고 변환
        if (fov > 3.14159265f)
            fov *= (3.14159265f / 180.0f);
        OutCam.FOV = fov;
    }
    if (CameraJSON.hasKey("NearClip"))
    {
        auto& Val = CameraJSON["NearClip"];
        OutCam.NearClip = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
    }
    if (CameraJSON.hasKey("FarClip"))
    {
        auto& Val = CameraJSON["FarClip"];
        OutCam.FarClip = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
    }
    OutCam.bValid = true;
}

// ============================================================
// Load
// ============================================================

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam)
{
    using json::JSON;
    std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
    if (!File.is_open())
    {
        std::cerr << "Failed to open file at target destination" << std::endl;
        return;
    }

    string FileContent((std::istreambuf_iterator<char>(File)),
                       std::istreambuf_iterator<char>());

    JSON root = JSON::Load(FileContent);

    int32 SceneVersion = root.hasKey(SceneKeys::Version) ? root[SceneKeys::Version].ToInt() : 1;

    string ClassName = root[SceneKeys::ClassName].ToString();
    ClassName = ClassName.empty() ? "UWorld" : ClassName; // Default to "World" if ClassName is missing
    UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
    if (!WorldObj || !WorldObj->IsA<UWorld>())
        return;

    UWorld* World = static_cast<UWorld*>(WorldObj);

    EWorldType WorldType = root.hasKey(SceneKeys::WorldType)
                               ? StringToWorldType(root[SceneKeys::WorldType].ToString())
                               : EWorldType::Editor;
    FString ContextName = root.hasKey(SceneKeys::ContextName)
                              ? root[SceneKeys::ContextName].ToString()
                              : "Loaded Scene";
    FString ContextHandle = root.hasKey(SceneKeys::ContextHandle)
                                ? root[SceneKeys::ContextHandle].ToString()
                                : ContextName;

    World->InitWorld();

    if (root.hasKey(SceneKeys::ActorFolders))
    {
        for (auto& FolderJSON : root[SceneKeys::ActorFolders].ArrayRange())
        {
            const FString FolderPath = FolderJSON.ToString();
            if (!FolderPath.empty())
            {
                World->AddEditorActorFolder(FolderPath);
            }
        }
    }

    // "PerspectiveCamera" 우선, 구버전 "Camera" 키도 지원
    const char* CamKey = root.hasKey("PerspectiveCamera") ? "PerspectiveCamera"
                         : root.hasKey("Camera")          ? "Camera"
                                                          : nullptr;
    if (CamKey)
    {
        JSON& Cam = root[CamKey];
        DeserializeCamera(Cam, OutCam);
    }

    // Deserialize Actors
    if (root.hasKey(SceneKeys::Actors))
    {
        for (auto& ActorJSON : root[SceneKeys::Actors].ArrayRange())
        {
            string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();
            AActor* Actor = nullptr;

            UObject* ActorObj = FObjectFactory::Get().Create(ActorClass, World);
            if (!ActorObj || !ActorObj->IsA<AActor>())
                continue;
            Actor = static_cast<AActor*>(ActorObj);
            World->AddActor(Actor);

            if (ActorJSON.hasKey(SceneKeys::Visible))
            {
                Actor->SetVisible(ActorJSON[SceneKeys::Visible].ToBool());
            }

            if (ActorJSON.hasKey(SceneKeys::Name))
            {
                Actor->SetFName(FName(ActorJSON[SceneKeys::Name].ToString()));
            }

            if (ActorJSON.hasKey("Tag"))
            {
                Actor->SetActorTag(FName(ActorJSON["Tag"].ToString()));
            }

            if (ActorJSON.hasKey(SceneKeys::ActorFolder))
            {
                const FString FolderPath = ActorJSON[SceneKeys::ActorFolder].ToString();
                Actor->SetEditorFolderPath(FolderPath);
                World->AddEditorActorFolder(FolderPath);
            }

            if (ActorJSON.hasKey(SceneKeys::Properties))
            {
                JSON& PropsJSON = ActorJSON[SceneKeys::Properties];
                DeserializeActorProperties(Actor, PropsJSON);
            }

            // RootComponent 트리 복원
            if (ActorJSON.hasKey(SceneKeys::RootComponent))
            {
                JSON& RootJSON = ActorJSON[SceneKeys::RootComponent];
                USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor);
                if (Root)
                    Actor->SetRootComponent(Root);
            }

            // Non-scene components 복원
            if (ActorJSON.hasKey(SceneKeys::NonSceneComponents))
            {
                for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange())
                {
                    string CompClass = CompJSON[SceneKeys::ClassName].ToString();
                    UObject* CompObj = FObjectFactory::Get().Create(CompClass, Actor);
                    if (!CompObj || !CompObj->IsA<UActorComponent>())
                        continue;

                    UActorComponent* Comp = static_cast<UActorComponent*>(CompObj);
                    Actor->RegisterComponent(Comp);

                    if (CompJSON.hasKey(SceneKeys::Name))
                    {
                        Comp->SetFName(FName(CompJSON[SceneKeys::Name].ToString()));
                    }

                    if (CompJSON.hasKey(SceneKeys::Properties))
                    {
                        JSON& PropsJSON = CompJSON[SceneKeys::Properties];
                        DeserializeProperties(Comp, PropsJSON);
                    }
                }
            }

            World->RemoveActorToOctree(Actor);
            World->InsertActorToOctree(Actor);
        }
    }

    OutWorldContext.WorldType = WorldType;
    OutWorldContext.World = World;
    OutWorldContext.ContextName = ContextName;
    OutWorldContext.ContextHandle = FName(ContextHandle);
}

USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner)
{
    string ClassName = Node[SceneKeys::ClassName].ToString();
    if (ClassName == "UUIComponent" && Node.hasKey(SceneKeys::Properties))
    {
        json::JSON& PropsJSON = Node[SceneKeys::Properties];
        if (PropsJSON.hasKey("Texture Path") ||
            PropsJSON.hasKey("SubUV Rect") ||
            PropsJSON.hasKey("Sprite Columns"))
        {
            ClassName = "UTextureUIComponent";
        }
    }

    UObject* Obj = FObjectFactory::Get().Create(ClassName, Owner);
    if (!Obj || !Obj->IsA<USceneComponent>())
        return nullptr;

    USceneComponent* Comp = static_cast<USceneComponent*>(Obj);
    Owner->RegisterComponent(Comp);

    if (Node.hasKey(SceneKeys::Name))
    {
        Comp->SetFName(FName(Node[SceneKeys::Name].ToString()));
    }

    // Restore properties
    if (Node.hasKey(SceneKeys::Properties))
    {
        json::JSON& PropsJSON = Node[SceneKeys::Properties];
        DeserializeProperties(Comp, PropsJSON);
    }
    Comp->MarkTransformDirty();

    // Restore children recursively
    if (Node.hasKey(SceneKeys::Children))
    {
        for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange())
        {
            USceneComponent* Child = DeserializeSceneComponentTree(ChildJSON, Owner);
            if (Child)
            {
                Child->AttachToComponent(Comp);
            }
        }
    }

    return Comp;
}

void FSceneSaveManager::DeserializeActorProperties(AActor* Actor, json::JSON& PropsJSON)
{
    if (!Actor)
    {
        return;
    }

    TArray<FPropertyDescriptor> Descriptors;
    Actor->GetEditableProperties(Descriptors);

    for (FPropertyDescriptor& Prop : Descriptors)
    {
        if (!PropsJSON.hasKey(Prop.Name.c_str()))
        {
            continue;
        }

        json::JSON& Value = PropsJSON[Prop.Name.c_str()];
        DeserializePropertyValue(Prop, Value);
        Actor->PostEditProperty(Prop.Name.c_str());
    }
}

void FSceneSaveManager::DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON)
{
    TArray<FPropertyDescriptor> Descriptors;
    Comp->GetEditableProperties(Descriptors);

    for (auto& Prop : Descriptors)
    {
        if (!PropsJSON.hasKey(Prop.Name.c_str()))
            continue;
        json::JSON& Value = PropsJSON[Prop.Name.c_str()];
        DeserializePropertyValue(Prop, Value);
        Comp->PostEditProperty(Prop.Name.c_str());
    }

    if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Comp))
    {
        if (PropsJSON.hasKey(SceneKeys::ShadowResolution))
        {
            LightComponent->SetShadowResolution(static_cast<uint32>(PropsJSON[SceneKeys::ShadowResolution].ToInt()));
            Comp->PostEditProperty(SceneKeys::ShadowResolution);
        }
    }

    // 2nd pass: PostEditProperty가 새 프로퍼티를 추가할 수 있음
    // (예: SetStaticMesh → MaterialSlots 생성 → "Element N" 디스크립터 추가)
    TArray<FPropertyDescriptor> Descriptors2;
    Comp->GetEditableProperties(Descriptors2);

    for (size_t i = Descriptors.size(); i < Descriptors2.size(); ++i)
    {
        auto& Prop = Descriptors2[i];
        if (!PropsJSON.hasKey(Prop.Name.c_str()))
            continue;
        json::JSON& Value = PropsJSON[Prop.Name.c_str()];
        DeserializePropertyValue(Prop, Value);
        Comp->PostEditProperty(Prop.Name.c_str());
    }

	// May have to remove this block
    if (USkeletalMeshComponent* SkeletalComponent = Cast<USkeletalMeshComponent>(Comp))
    {
        if (PropsJSON.hasKey(SceneKeys::BoneLocalMatrices))
        {
            json::JSON& BoneMatricesJSON = PropsJSON[SceneKeys::BoneLocalMatrices];
            if (BoneMatricesJSON.JSONType() == json::JSON::Class::Array)
            {
                TArray<FMatrix> BoneLocalMatrices;
                BoneLocalMatrices.reserve(BoneMatricesJSON.size());

                bool bAllMatricesValid = true;
                for (auto& MatrixJSON : BoneMatricesJSON.ArrayRange())
                {
                    FMatrix Matrix = FMatrix::Identity;
                    if (!ReadMatrix(MatrixJSON, Matrix))
                    {
                        bAllMatricesValid = false;
                        break;
                    }
                    BoneLocalMatrices.push_back(Matrix);
                }

                if (bAllMatricesValid)
                {
                    SkeletalComponent->SetBoneLocalMatrices(BoneLocalMatrices);
                }
            }
        }
    }
    // May have to remove this block
}

void FSceneSaveManager::DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value)
{
    switch (Prop.Type)
    {
    case EPropertyType::Bool:
        *static_cast<bool*>(Prop.ValuePtr) = Value.ToBool();
        break;

    case EPropertyType::ByteBool:
        *static_cast<uint8_t*>(Prop.ValuePtr) = Value.ToBool() ? 1 : 0;
        break;

    case EPropertyType::Int:
    case EPropertyType::Enum:
        *static_cast<int32*>(Prop.ValuePtr) = Value.ToInt();
        break;

    case EPropertyType::Float:
        *static_cast<float*>(Prop.ValuePtr) = static_cast<float>(Value.ToFloat());
        break;

    case EPropertyType::Vec2:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        int i = 0;
        for (auto& elem : Value.ArrayRange())
        {
            if (i < 2)
                v[i] = static_cast<float>(elem.ToFloat());
            i++;
        }
        break;
    }
    case EPropertyType::Vec3:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        int i = 0;
        for (auto& elem : Value.ArrayRange())
        {
            if (i < 3)
                v[i] = static_cast<float>(elem.ToFloat());
            i++;
        }
        break;
    }
    case EPropertyType::Rotator:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        int i = 0;
        for (auto& elem : Value.ArrayRange())
        {
            if (i < 3)
                v[i] = static_cast<float>(elem.ToFloat());
            i++;
        }
        break;
    }
    case EPropertyType::Vec4:
    case EPropertyType::Color4:
    {
        float* v = static_cast<float*>(Prop.ValuePtr);
        int i = 0;
        for (auto& elem : Value.ArrayRange())
        {
            if (i < 4)
                v[i] = static_cast<float>(elem.ToFloat());
            i++;
        }
        break;
    }
    case EPropertyType::String:
    case EPropertyType::StaticMeshRef:
    case EPropertyType::SkeletalMeshRef:
    case EPropertyType::ComponentRef:
        *static_cast<FString*>(Prop.ValuePtr) = Value.ToString();
        break;

    case EPropertyType::MaterialSlot:
    {
        FMaterialSlot* Slot = static_cast<FMaterialSlot*>(Prop.ValuePtr);
        if (Value.hasKey("Path"))
            Slot->Path = Value["Path"].ToString();
        break;
    }

    case EPropertyType::Name:
        *static_cast<FName*>(Prop.ValuePtr) = FName(Value.ToString());
        break;

    default:
        break;
    }
}

// ============================================================
// Utility
// ============================================================

string FSceneSaveManager::GetCurrentTimeStamp()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);

    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}

TArray<FString> FSceneSaveManager::GetSceneFileList()
{
    TArray<FString> Result;
    std::wstring SceneDir = GetSceneDirectory();
    if (!std::filesystem::exists(SceneDir))
    {
        return Result;
    }

    for (auto& Entry : std::filesystem::directory_iterator(SceneDir))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == SceneExtension)
        {
            Result.push_back(FPaths::ToUtf8(Entry.path().stem().wstring()));
        }
    }
    return Result;
}
