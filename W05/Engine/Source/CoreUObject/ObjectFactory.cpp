#include "ObjectFactory.h"
#include "Core/Logging/LogMacros.h"

TMap<const void *, std::function<UObject *()>> &FObjectFactory::GetRegistry()
{
    static TMap<const void *, std::function<UObject *()>> Registry;
    return Registry;
}

UObject *FObjectFactory::ConstructObject(const void *Id)
{
    auto &Registry = GetRegistry();

    UE_LOG(ObjectFactory, ELogLevel::Verbose, "ConstructObject input class=%p registry=%p size=%zu",
           Id, &Registry, Registry.size());

    auto It = Registry.find(Id);
    if (It == Registry.end())
    {
        UE_LOG(ObjectFactory, ELogLevel::Error,
               "ConstructObject failed: no creator registered for class=%p", Id);
        return nullptr;
    }

    UObject *Obj = It->second();

    UE_LOG(ObjectFactory, ELogLevel::Debug, "ConstructObject success: class=%p obj=%p", Id, Obj);

    return Obj;
}

void FObjectFactory::RegisterObjectType(const void *Id, std::function<UObject *()> FactoryFunction)
{
    auto &Registry = GetRegistry();
    Registry[Id] = std::move(FactoryFunction);

    UE_LOG(ObjectFactory, ELogLevel::Verbose, "RegisterClass: class=%p registry=%p size=%zu", Id,
           &Registry, Registry.size());
}