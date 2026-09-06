#include "SceneSaveManager.h"
#include "SimpleJSON/json.hpp"

void FSceneSaveManager::SaveSceneAsJSON(UScene* Scene) {
    if (!Scene) return;

    std::string FileDestination = GetFileDialoguePath(FILE_SAVE);
    if (FileDestination.empty()) return;

    json::JSON Root;
    Root["Scene"]["Version"] = 1;
    Root["Scene"]["Name"] = std::string("Scene_") + std::to_string(Scene->UUID);

    json::JSON Objects = json::Array();

    // Scene object first (LoadSceneFromJSON uses it to identify the scene)
    Objects.append(SerializeObject(Scene));

    for (AActor* Actor : Scene->GetActors()) {
        if (!Actor || Actor->bPendingKill) continue;

        Objects.append(SerializeObject(Actor));

        for (USceneComponent* Comp : Actor->GetComponents()) {
            if (!Comp || Comp->bPendingKill) continue;
            Objects.append(SerializeObject(Comp));
        }
    }

    Root["Scene"]["Objects"] = Objects;

    std::ofstream File(FileDestination);
    if (File.is_open()) {
        File << Root.dump();
        File.flush();
        File.close();
    }
}

json::JSON FSceneSaveManager::SerializeObject(UObject* Object) {
    json::JSON j = json::Object();
    if (!Object) return j;
    Object->SerializeHeader(j);   // ClassName, UUID, InternalIndex
    Object->Serialize(j);         // virtual — type-specific fields
    return j;
}

UScene* FSceneSaveManager::LoadSceneFromJSON(UWorld* World) {
    FString FilePath = GetFileDialoguePath(FILE_LOAD);
    if (FilePath.empty()) return nullptr;

    std::ifstream File(FilePath);
    if (!File.is_open()) {
        std::cerr << "Failed to open file: " << FilePath << std::endl;
        return nullptr;
    }

    json::JSON Root = json::JSON::Load(string(
        std::istreambuf_iterator<char>(File),
        std::istreambuf_iterator<char>()));

    TMap<uint32, UObject*> UUIDMap;
    UScene* Scene = nullptr;
    uint32 MaxUUID = 0;

    // Pass 1: create all objects and restore their data
    for (auto& JSONObject : Root["Scene"]["Objects"].ArrayRange()) {
        string ClassName = JSONObject["ClassName"].ToString();
        UObject* Obj = FObjectFactory::Get().Create(ClassName);
        if (!Obj) continue;

        uint32 UUID = static_cast<uint32>(JSONObject["UUID"].ToInt());
        Obj->UUID          = UUID;
        Obj->InternalIndex = static_cast<uint32>(JSONObject["InternalIndex"].ToInt());
        Obj->Name = FName(JSONObject["FName"].ToString());
        MaxUUID = (UUID > MaxUUID) ? UUID : MaxUUID;

        UUIDMap[UUID] = Obj;
        Obj->Deserialize(JSONObject);

        if (Obj->IsA<UScene>())
            Scene = Obj->Cast<UScene>();
    }

    if (!Scene) return nullptr;  // corrupted save

    // Assign a fresh UUID to avoid conflicts with existing objects
    Scene->UUID = ++MaxUUID;
    EngineStatics::ResetUUIDGeneration(MaxUUID + 1);

    // Pass 2: resolve UUID references (parents, root components, scene membership)
    LinkReferences(UUIDMap, Root["Scene"]["Objects"], Scene);

    return Scene;
}

UScene* FSceneSaveManager::LinkReferences(const TMap<uint32, UObject*>& uuidMap, json::JSON Savedata, UScene* Scene) {
    using namespace json;

    // Pass 1: restore all parent-child relationships
    for (auto& JSONObject : Savedata.ArrayRange()) {
        uint32 cachedUUID = JSONObject["UUID"].ToInt();
        auto it = uuidMap.find(cachedUUID);
        if (it == uuidMap.end()) continue;
        UObject* Obj = it->second;

        if (Obj->IsA<USceneComponent>()) {
            USceneComponent* SceneComp = Obj->Cast<USceneComponent>();
            uint32 ParentUUID = JSONObject["ParentUUID"].ToInt();
            if (ParentUUID) {
                auto ParentIt = uuidMap.find(ParentUUID);
                if (ParentIt != uuidMap.end()) {
                    SceneComp->SetParent(
                        static_cast<USceneComponent*>(ParentIt->second));
                }
            }
        }
    }

    // Pass 2: set root components and world relationships
    for (auto& JSONObject : Savedata.ArrayRange()) {
        uint32 cachedUUID = JSONObject["UUID"].ToInt();
        auto it = uuidMap.find(cachedUUID);
        if (it == uuidMap.end()) continue;
        UObject* Obj = it->second;

        if (Obj->IsA<AActor>()) {
            AActor* Actor = Obj->Cast<AActor>();

            Actor->SetOwnerUUID(Scene->UUID);

            uint32 RootCompUUID = JSONObject["RootComponentUUID"].ToInt();
            if (RootCompUUID) {
                auto RootIt = uuidMap.find(RootCompUUID);
                if (RootIt != uuidMap.end()) {
                    auto* RootComp = static_cast<USceneComponent*>(RootIt->second);
                    Actor->SetRootComponent(RootComp);
                    RootComp->SetOwningActor(Actor);
                }
            }

            Scene->AddActor(Actor);
        }
    }

    return Scene;
}


FString FSceneSaveManager::GetFileDialoguePath(EDialogueMode Mode) {
    string FileDestination;
    switch (Mode) {
    case (FILE_SAVE) : {
        string SceneName = "Save_" + GetCurrentTimeStamp();

        // Save dialogue
        char szFile[MAX_PATH] = {};
        // Pre-fill the filename box with the scene name
        strncpy_s(szFile, SceneName.c_str(), MAX_PATH - 1);

        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFilter = "Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = "Scene";
        ofn.lpstrTitle = "Save Scene";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetSaveFileNameA(&ofn)) {
            return "";
        }

        FileDestination = szFile;
        break;
    }

    case (FILE_LOAD):
        char szFile[MAX_PATH] = {};

        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFilter = "Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = "Scene";
        ofn.lpstrTitle = "Load Scene";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameA(&ofn)) {
            break;
        }

        FileDestination = szFile;

        const size_t DotPos = FileDestination.rfind('.');
        if (DotPos == std::string::npos) {
            std::cerr << "Invalid file: no extension found" << std::endl;
            return "";
        }
        const std::string Extension = FileDestination.substr(DotPos);
        if (Extension != ".Scene") {
            std::cerr << "Invalid file type: expected .Scene, got " << Extension << std::endl;
            return "";
        }
        break;
    }

    return FileDestination;
}

string FSceneSaveManager::GetCurrentTimeStamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);

    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}
