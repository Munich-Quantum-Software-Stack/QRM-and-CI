#include "../include/QuantumResourceManager.hpp"

#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct QuantumResult
{
    int task_id;
    std::map<std::string, int> results;
    std::string destination;
    bool execution_status;
    std::vector<std::string> executed_qpu;
    std::vector<std::string> executed_circuit;
    std::string additional_information;
    double execution_time;
};

int main()
{
    QuantumResult result;

    amqp_connection_state_t conn;

    std::time_t currentTime = std::time(nullptr);
    const int bufferSize = 80;
    char buffer[bufferSize];
    std::strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S",
                  std::localtime(&currentTime));
    std::string submit_time(buffer);

    // Open the QIR file
    const char *filename = "benchmarks/bell_state.ll";
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "[Quantum Daemon]......Failed to open file with QIR: "
                  << filename << std::endl;
        return 1;
    }

    const std::streamsize chunkSize = 1024;
    char bufferQir[chunkSize];
    std::string genericQir;

    while (!file.eof())
    {
        file.read(bufferQir, chunkSize);
        genericQir.append(bufferQir, file.gcount());
    }
    file.close();

    json QuantumTask_json = {
        {"task_id", -1},
        {"n_qbits", 0},
        {"n_shots", 10000},
        {"circuit_file", ""},
        {"circuit_file_type", "QIR"},
        {"result_destination", ""},
        {"preferred_qpus", "Q20, Q5"},
        {"duration", 1},
        {"scheduled_qpu", ""},
        {"priority", 0},
        {"optimisation_level", 0},
        {"no_modify", false},
        {"transpiler_flag", true},
        {"result_type", 0},
        {"submit_time", submit_time},
        {"circuit_qiskit", genericQir},
        {"additional_information", ""},
        {"change_selector", "libselector_manual.so"},
        {"change_scheduler", "libscheduler_heuristic.so"},
    };

    auto qt = JSONToQuantumTask(QuantumTask_json.dump().c_str());
    handleQuantumDaemon(conn, "SomeweirdQueue", qt);

    return 0;
}
