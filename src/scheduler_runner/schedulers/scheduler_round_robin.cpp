/**
 * @file scheduler_round_robin.cpp
 * @brief Implementation of a dummy scheduler.
 */

#include <iostream>
#include <string>
#include <vector>

#include <PassModule.hpp>
#include <QuantumResourceManager.hpp>

#include <fomac.hpp>
#include <qdmi.h>

struct QuantumTask;

/**
 * @brief The main entry point of the program.
 *
 * The Scheduler.
 *
 * @return const char *
 */
extern "C" int scheduler(std::vector<QuantumTask> *childQuantumTasks)
{
    // Query the available devices
    std::vector<QDMI_Device> devices = FOMAC_available_devices(false /*verbose*/);

    if (devices.size() == 0)
    {
        std::cout << "   [Scheduler]...........Error: no devices found" << std::endl;
        return 1;
    }

    std::cout << "   [Scheduler]..........." << devices.size()
              << " available device(s)" << std::endl;

    for (auto &childQuantumTask : *childQuantumTasks)
    {
        QDMI_Device target_device = NULL;

        for (auto device : devices)
        {
            const char *device_libname = strrchr(device->library.libname, '/');

            if (device_libname == childQuantumTask.preferred_qpu)
            {
                target_device = device;
                break;
            }
        }

        if (!target_device)
            target_device = devices.back();

        const char *target_libname = strrchr(target_device->library.libname, '/');

        std::cout << "   [Scheduler]...........Setting "
                  << target_libname << " as target device "
                  << "for job with ID " << childQuantumTask.task_id
                  << std::endl;

        childQuantumTask.scheduled_qpu = target_device;
    }

    return 0; 
}
