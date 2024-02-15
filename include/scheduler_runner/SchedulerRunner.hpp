/**
 * @file SchedulerRunner.hpp
 * @brief TODO
 */

#ifndef SCHEDULERRUNNER_HPP
#define SCHEDULERRUNNER_HPP

#include "llvm.hpp"
#include <fomac.hpp>
//#include <qdmi.hpp>
#include <qdmi.h>

#include "../../src/my_qdmi.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <dlfcn.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>
using llvm::orc::ThreadSafeModule;

int invokeScheduler(const std::string &nameScheduler, QuantumTask &task);

#endif // SCHEDULERRUNNER_HPP
