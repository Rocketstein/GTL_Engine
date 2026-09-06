#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include "CoreTypes.h"


template<typename... Args>
class TDelegate
{
    using HandlerType = std::function<void(Args...)>;
    struct FHandlerElement
    {
        uint64 Id = 0;
        void* Target;
        HandlerType Func;
    };

public:
    // 런타임 객체에 대한 지정 없이 핸들러 등록
    void Add(const HandlerType& handler)
    {
        FHandlerElement Element;
        Element.Target = nullptr;
        Element.Func = handler;
        AddInternal(Element);
    }

    // Instance 명의로 핸들러를 등록
    template<typename T>
    void AddDynamic(T* Instance, void (T::*Func)(Args...))
    {
        FHandlerElement Element;
        Element.Target = Instance;
        Element.Func = [Instance, Func](Args... args) { (Instance->*Func)(args...); };
        AddInternal(Element);
    }
    
    // Instance 명의로 등록된 핸들러를 모두 제거
    template<typename T>
    void RemoveDynamic(T* Instance)
    {
		if(Instance == nullptr) 
			return;

        Handlers.erase(
            std::remove_if(
                Handlers.begin(), Handlers.end(),
                [Instance](const FHandlerElement& Node) {
                    return Node.Target == Instance;
                }
            ), 
            Handlers.end()
        );
    }
    
    void Clear()
    {
        Handlers.clear();
    }
    
    void Broadcast(Args... args)
    {
        TArray<FHandlerElement> Snapshot = Handlers;
        for (const FHandlerElement& Node : Snapshot)
        {
            if (!IsHandlerStillBound(Node.Id))
            {
                continue;
            }
            Node.Func(args...);
        }
    }

private:
    TArray<FHandlerElement> Handlers;
    uint64 NextHandlerId = 1;

    void AddInternal(FHandlerElement handler)
    {
        handler.Id = NextHandlerId++;
        Handlers.push_back(handler);
    }

    bool IsHandlerStillBound(uint64 Id) const
    {
        return std::any_of(
            Handlers.begin(), Handlers.end(),
            [Id](const FHandlerElement& Node)
            {
                return Node.Id == Id;
            });
    }
};

#define DECLARE_DELEGATE(Name, ...) TDelegate<__VA_ARGS__> Name
