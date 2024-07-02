/**
 * @file QuantumResourceManager.hpp
 * @brief TODO
 */

#ifndef QUANTUMRESOURCEMANAGER_HPP
#define QUANTUMRESOURCEMANAGER_HPP

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <connection_handling.hpp>

#include <GeneratorRunner.hpp>
#include <PassRunner.hpp>
#include <SchedulerRunner.hpp>
#include <SelectorRunner.hpp>

#include <fomac.hpp>
#include <qdmi.h>
#include <qinfo.h>
#include <QuantumTask.hpp>
#include <scheduler.hpp>

using llvm::orc::ThreadSafeModule;


QuantumTask JSONToQuantumTask(const char *QuantumTask_str);
void handleQuantumDaemon(amqp_connection_state_t &conn, char const *QDQueue,
                         const QuantumTask &quantumTask);
void signalHandler(int signum);

#endif // QUANTUMRESOURCEMANAGER_HPP
