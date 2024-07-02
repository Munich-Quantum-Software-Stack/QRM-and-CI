/**
 * @file SchedulerRunner.cpp
 * @brief TODO
 */

#include "SchedulerRunner.hpp"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/**
 * @brief TODO
 * @param pathScheduler TODO
 * @return QuantumTask
 */
int invokeScheduler(const std::string &nameScheduler,
                    std::vector<QuantumTask> *childQuantumTasks, 
                    Device2SubmitterType device2Submitter)
{
    //std::string pathScheduler;
    //char buffer[PATH_MAX];

    //ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

    char *scheduler_path = getenv("SCHEDULER_PATH");
    std::string pathScheduler = std::string(scheduler_path);
    
    pathScheduler.append(nameScheduler);

    std::cout << "   [Scheduler Runner]....Invoking scheduler: "
              << nameScheduler << std::endl;

    // Load the scheduler as a shared library
    void *lib_handle = dlopen(pathScheduler.c_str(), RTLD_LAZY);

    if (!lib_handle)
    {
        std::cerr
            << "   [Scheduler Runner]..Error loading scheduler as a shared "
               "library: "
            << dlerror() << std::endl;

        return 1;
    }

    // Dynamic loading and linking of the shared library
    typedef int (*SchedulerFunction)(Device2SubmitterType, std::vector<QuantumTask> *);
    SchedulerFunction scheduler =
        reinterpret_cast<SchedulerFunction>(dlsym(lib_handle, "scheduler"));

    if (!scheduler)
    {
        std::cerr << "   [Scheduler Runner]..Error finding function in shared "
                     "library: "
                  << dlerror() << std::endl;

        dlclose(lib_handle);
    }


    // Call the scheduler function
    return scheduler(device2Submitter, childQuantumTasks);
}
