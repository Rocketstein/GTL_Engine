#include "AsyncTaskManager.h"

void AsyncTaskManager::Enqueue(float delay, std::function<void()> task)
{
	taskList.push_back({ delay, task });
}

void AsyncTaskManager::Tick(float deltaTime)
{
    for (auto it = taskList.begin(); it != taskList.end();) {
        it->delay -= deltaTime;
        if (it->delay <= 0) {
            it->task();
            it = taskList.erase(it);
        }
        else {
            ++it;
        }
    }
}

void AsyncTaskManager::Clear()
{
    taskList.clear();
}
