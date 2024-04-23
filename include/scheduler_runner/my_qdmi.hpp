#ifndef MY_QDMI_HPP
#define MY_QDMI_HPP

#include "QuantumResourceManager.hpp"

#include <deque>

struct QuantumTask;

struct MyQueue
{
    std::string platform;            // Name of the platform
    std::deque<QuantumTask *> tasks; // List of tasks in the queue

    MyQueue(std::string platform) : platform(platform) {}

    void insertTask(int position, QuantumTask *task, float duration)
    {
        if (0 <= position && position <= tasks.size())
        {
            tasks.insert(tasks.begin() + position, task);
        }
        else
        {
            std::cerr << "Error: Position " << position
                      << " is out of range.\n";
        }
    }
};

#endif // MY_QDMI_HPP
