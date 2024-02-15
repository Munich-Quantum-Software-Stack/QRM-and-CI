/**
 * @file qrm.hpp
 * @brief TODO
 */

#ifndef QRM_HPP
#define QRM_HPP

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
#include <fomac_q7.hpp>
#include <qdmi.h>
#include <qdmi_internal.h>
#include <qinfo.h>

/**
 * @todo Comment this
 */
struct QuantumTask
{
    int task_id;
    int n_qbits;
    int n_shots;
    std::string circuit_file;
    std::string circuit_file_type;
    std::string result_destination;
    std::string preferred_qpu;
    std::string scheduled_qpu;
    int priority;
    int optimisation_level;
    bool no_modify;
    bool transpiler_flag;
    int result_type;
    std::string submit_time;
    std::string circuit_qiskit;
    std::string additional_information;
    std::string change_generator;
    std::string change_selector;
    std::string change_scheduler;
};

QuantumTask JSONToQuantumTask(const char *QuantumTask_str);
void handleQuantumDaemon(amqp_connection_state_t &conn, char const *QDQueue,
                         const QuantumTask &quantumTask);
void signalHandler(int signum);

#endif // QRM_HPP
