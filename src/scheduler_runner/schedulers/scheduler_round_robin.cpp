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
extern "C" QDMI_Device scheduler(const QuantumTask &childQuantumTask)
{
    // Query the available devices
    std::vector<QDMI_Device> devices = FOMAC_available_devices();

    std::cout << "   [Scheduler]..........." << devices.size()
              << " available device(s)" << std::endl;

    std::cout << "   [Scheduler]...........Choosing target QDMI_Device "
              << "for QuantumTask with ID " << childQuantumTask.task_id
              << std::endl;

    return devices.back();
}
