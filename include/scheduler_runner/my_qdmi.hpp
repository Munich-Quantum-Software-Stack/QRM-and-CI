#ifndef MY_QDMI_HPP
#define MY_QDMI_HPP

#include "QuantumResourceManager.hpp"

#include <deque>

struct QuantumTask;

struct MyQueue
{
    std::string platform;            // Name of the platform
    std::deque<QuantumTask *> tasks; // List of tasks in the queue
    float totalDuration;             // Total duration of all tasks in the queue

    MyQueue(std::string platform) : platform(platform), totalDuration(0.0) {}

    float insertTask(int position, QuantumTask *task, float duration)
    {
        if (position >= 0 && position <= tasks.size())
        {
            tasks.insert(tasks.begin() + position, task);
            totalDuration += duration; // Update total duration
            return 1.;                 // TODO
        }
        else
        {
            // Handle error: position out of range
            return 0.;
        }
    }
};

#endif // MY_QDMI_HPP
