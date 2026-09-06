#pragma once
#include <functional>
#include <list>

struct task {
    float delay;
    std::function<void()> task;
};

class AsyncTaskManager
{
public:
    static AsyncTaskManager* getInstance()
    {
        static AsyncTaskManager* instance = new AsyncTaskManager();
        return instance;
    }
    void Enqueue(float delay, std::function<void()> task);
    void Tick(float deltaTime);
    void Clear();
private:
    std::list<task> taskList;
};

