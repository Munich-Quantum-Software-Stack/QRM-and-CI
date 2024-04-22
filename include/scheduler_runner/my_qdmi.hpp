#ifndef MY_QDMI_HPP
#define MY_QDMI_HPP

#include "QuantumResourceManager.hpp"

#include <deque>

struct QuantumTask;

struct Queue
{
    std::string platform;            // Name of the platform
    std::deque<QuantumTask *> tasks; // List of tasks in the queue
    float totalDuration;             // Total duration of all tasks in the queue

    Queue(std::string platform) : platform(platform), totalDuration(0.0) {}

    void insertTask(int position, QuantumTask *task, float duration)
    {
        if (position >= 0 && position <= tasks.size())
        {
            tasks.insert(tasks.begin() + position, task);
            totalDuration += duration; // Update total duration
        }
        else
        {
            // Handle error: position out of range
        }
    }

    void removeTaskAtBeginning(float duration)
    {
        if (!tasks.empty())
        {
            totalDuration -= duration; // Update total duration
            tasks.pop_front();
        }
        else
        {
            // Handle error: no tasks to remove
        }
    }

    float getEndTime() const { return totalDuration; }
};

#endif // MY_QDMI_HPP
