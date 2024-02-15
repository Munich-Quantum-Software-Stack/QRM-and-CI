/**
 * @file scheduler_round_robin.cpp
 * @brief Implementation of a dummy scheduler.
 */

#include <iostream>
#include <string>
#include <vector>

#include "PassModule.hpp"

#include <fomac.hpp>
#include <qdmi.h>

/**
 * @brief The main entry point of the program.
 *
 * The Scheduler.
 *
 * @return const char *
 */
extern "C" QDMI_Device scheduler(void)
{
    // Query the available devices
    std::vector<QDMI_Device> devices = FOMAC_available_devices();

    std::cout << "   [Scheduler]...........Choosing target QDMI_Device"
              << std::endl;
    
    std::cout << "   [Scheduler]..........." << devices.size()
              << " available device(s)"
              << std::endl;

    QDMI_Device dev = devices.back();

    std::cout << "   [Scheduler]...........QDMI_Device library: "
              << dev->library.libname
              << std::endl;

    std::cout << "   [Scheduler]...........Choosing target QDMI_Device"
              << std::endl;

    return devices.back();
}
