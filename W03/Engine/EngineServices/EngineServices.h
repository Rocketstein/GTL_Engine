#pragma once

class FRenderResourceManager;

class FEngineServices
{
public:
    static void               RegisterResourceManager(FRenderResourceManager* In) { ResourceManager = In; }
    static FRenderResourceManager* GetResourceManager() { return ResourceManager; }

    // 나중에 필요하면 추가
    // static FRenderBus*       GetRenderBus() { return RenderBus; }
    // static FPhysicsManager*  GetPhysics()   { return Physics; }

private:
    static inline FRenderResourceManager* ResourceManager = nullptr;
};