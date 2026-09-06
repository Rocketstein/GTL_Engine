#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

template <typename... Args>
class TDelegate;

/*
 * Delegate 타입 별칭을 선언하기 위한 매크로.
 * 이 매크로는 Delegate 인스턴스를 만드는 것이 아니라,
 * 특정 인자 목록을 가진 TDelegate 타입에 이름을 붙인다.
 */
#define DECLARE_DELEGATE_TYPE(TypeName, ...) using TypeName = TDelegate<__VA_ARGS__>

/*
 * Delegate에 등록된 Handler를 식별하기 위한 번호표 역할을 한다.
 *
 * Handler 자체(std::function)는 비교하기 어렵기 때문에,
 * Add() 시점에 고유 ID를 발급하고 이 Handle을 통해 Remove()할 수 있게 한다.
 *
 * InvalidID는 유효하지 않은 Handle을 의미한다.
 */
struct FDelegateHandle
{
    static constexpr uint64_t InvalidID = 0;

    uint64_t ID = InvalidID;

    bool IsValid() const { return ID != InvalidID; }

    void Reset() { ID = InvalidID; }

    friend bool operator==(const FDelegateHandle &A, const FDelegateHandle &B) { return A.ID == B.ID; }
    friend bool operator!=(const FDelegateHandle &A, const FDelegateHandle &B) { return !(A == B); }
};

/*
 * 여러 Handler를 등록해두고, 이벤트 발생 시 한 번에 호출하기 위한 Delegate 클래스
 *
 * Args...는 Broadcast 시 전달할 인자 타입 목록이다.
 *
 * Add() 또는 AddDynamic()으로 Handler를 등록하고,
 * Broadcast()로 등록된 Handler들을 호출한다.
 *
 * Add()는 등록된 Handler를 나중에 제거할 수 있도록 FDelegateHandle을 반환한다.
 * Remove()는 이 Handle을 이용해 특정 Handler를 제거한다.
 *
 * Broadcast 중 Handler가 Remove되어도 안전하도록,
 * Broadcast 시점의 Handle 목록을 Snapshot으로 복사한 뒤 순회한다.
 */
template <typename... Args>
class TDelegate
{
  public:
    using HandlerType = std::function<void(Args...)>;

    // 일반 함수, 람다, std::function 형태의 Handler를 Delegate에 등록한다.
    // 등록된 Handler를 나중에 제거할 수 있도록 FDelegateHandle을 반환한다.
    FDelegateHandle Add(const HandlerType &Handler)
    {
        FDelegateHandle NewHandle;
        NewHandle.ID = NextHandleID++;

        FDelegateEntry NewEntry;
        NewEntry.Handle = NewHandle;
        NewEntry.Handler = Handler;

        Handlers.push_back(NewEntry);

        return NewHandle;
    }

    // 객체 인스턴스와 멤버 함수를 Delegate에 등록한다.
    // 내부적으로 멤버 함수 호출을 람다로 감싼 뒤 Add()에 전달한다.
    template <typename T>
    FDelegateHandle AddDynamic(T *Instance, void (T::*Func)(Args...))
    {
        return Add(
            [Instance, Func](Args... InArgs)
            {
                if (Instance)
                {
                    (Instance->*Func)(InArgs...);
                }
            });
    }

    // Add() 또는 AddDynamic()에서 반환받은 Handle을 이용해 특정 Handler를 제거한다.
    // Handler 자체는 비교하기 어렵기 때문에 Handle.ID를 기준으로 제거한다.
    void Remove(FDelegateHandle Handle)
    {
        typename std::vector<FDelegateEntry>::iterator NewEnd;

        NewEnd = std::remove_if(Handlers.begin(), Handlers.end(), [Handle](const FDelegateEntry &Entry) { return Entry.Handle == Handle; });

        Handlers.erase(NewEnd, Handlers.end());
    }

    void Clear() { Handlers.clear(); }

    /*
     * Delegate에 등록된 모든 Handler를 호출한다.
     *
     * Broadcast 중 삭제된 Handler가 있어도 순회가 깨지지 않도록 Handle 목록을 Snapshot으로 복사한 뒤 순회한다.
     * 호출 직전에 FindHandler()로 현재 등록되어있는 Handler인지 확인한다.
     */
    void Broadcast(Args... InArgs)
    {
        std::vector<FDelegateHandle> Snapshot;
        for (size_t Index = 0; Index < Handlers.size(); ++Index)
        {
            Snapshot.push_back(Handlers[Index].Handle);
        }

        for (size_t Index = 0; Index < Snapshot.size(); ++Index)
        {
            HandlerType HandlerToCall;

            if (FindHandler(Snapshot[Index], HandlerToCall))
            {
                HandlerToCall(InArgs...);
            }
        }
    }

    // 현재 Delegate에 등록된 Handler가 하나라도 있는지 확인한다.
    bool IsBound() const { return !Handlers.empty(); }

  private:
    struct FDelegateEntry
    {
        FDelegateHandle Handle;  // 식별자
        HandlerType     Handler; // 실제 호출할 함수
    };

  private:
    // 특정 Handle에 해당하는 Handler를 찾는다.
    // Broadcast 중 Remove된 Handler는 여기서 찾지 못하므로 호출되지 않는다.
    bool FindHandler(FDelegateHandle Handle, HandlerType &OutHandler) const
    {
        for (size_t Index = 0; Index < Handlers.size(); ++Index)
        {
            if (Handlers[Index].Handle == Handle)
            {
                OutHandler = Handlers[Index].Handler;
                return true;
            }
        }

        return false;
    }

  private:
    std::vector<FDelegateEntry> Handlers;
    uint64_t                    NextHandleID = 1;
};
