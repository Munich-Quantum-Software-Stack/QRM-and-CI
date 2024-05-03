#include "../include/connection_handling.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <stdio.h>
#include <string>
#include <thread>
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

QuantumResult JSONToQuantumResult(const char *QuantumResult_str)
{
    QuantumResult result;

    json QuantumResult_json = json::parse(QuantumResult_str);

    result.task_id = QuantumResult_json["task_id"];
    result.results = QuantumResult_json["results"];
    result.destination = QuantumResult_json["destination"];
    result.execution_status = QuantumResult_json["execution_status"];
    result.executed_qpu =
        QuantumResult_json["executed_qpu"].get<std::vector<std::string>>();
    result.executed_circuit =
        QuantumResult_json["executed_circuit"].get<std::vector<std::string>>();
    result.additional_information =
        QuantumResult_json["additional_information"];
    result.execution_time = QuantumResult_json["execution_time"];

    return result;
}

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);

    // Establish a connection to the RabbitMQ server
    const char *QDQueue = "queue_daemon";
    const char *QRMQueue = "queue_manager";

    amqp_connection_state_t conn;
    amqp_socket_t *socket = NULL;
    rabbitmq_new_connection(&conn, &socket);

    // Open the QIR file
    // const char *filename = "../../benchmarks/test.ll";
    std::string benchmark_dir = "../../../MQT_under_20";
    std::string default_results_dir = "../../../logs";
    std::string final_results_dir =
        "../../../experiments/int/interleaf_rndmut/int_u20_obj5_gen0_pop32_len200";

    for (const auto &entry : std::filesystem::directory_iterator(benchmark_dir))
        if (entry.is_regular_file() && entry.path().extension() == ".ll")
        {
            std::ifstream file(entry.path(), std::ios::binary);
            if (!file.is_open())
            {
                std::cerr
                    << "[Quantum Daemon]......Failed to open file with QIR: "
                    << entry.path() << std::endl;
                return 1;
            }

            // Read the file with the generic QIR
            const std::streamsize chunkSize = 1024;
            char bufferQir[chunkSize];
            std::string genericQir;

            while (!file.eof())
            {
                file.read(bufferQir, chunkSize);
                genericQir.append(bufferQir, file.gcount());
            }
            file.close();

            // Create JSON string to send to the Quantum Resource Manager
            std::time_t currentTime = std::time(nullptr);
            const int bufferSize = 80;
            char buffer[bufferSize];
            std::strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S",
                          std::localtime(&currentTime));
            std::string submit_time(buffer);
            json QuantumTask_json = {
                {"task_id", -1},
                {"n_qbits", 0},
                {"n_shots", 10000},
                {"circuit_file", ""},
                {"circuit_file_type", "QIR"},
                {"result_destination", ""},
                {"preferred_qpu", "Q20"},
                {"scheduled_qpu", ""},
                {"priority", 0},
                {"optimisation_level", 0},
                {"no_modify", false},
                {"transpiler_flag", true},
                {"result_type", 0},
                {"submit_time", submit_time},
                {"circuit_qiskit", genericQir},
                {"additional_information", ""},
                {"change_selector", "libselector_ga.so"},
                {"change_scheduler", "libscheduler_round_robin.so"},
            };

            std::string QuantumTask_str = QuantumTask_json.dump();

            // delete[] genericQir;

            // Send the QuantumTask to the Quantum Resource Manager
            std::cout << "[Quantum Daemon]......Sending QuantumTask to the QRM"
                      << std::endl;

            send_message(&conn, QuantumTask_str.c_str(), QRMQueue);

            // Receive the response from the daemon
            const char *results = receive_message(&conn, QDQueue);

            if (results)
            {
                QuantumResult quantumResult = JSONToQuantumResult(results);

                std::cout << "[Quantum Daemon]......Received QuantumResult"
                          << std::endl;
                std::cout << "                      L ...task_id: "
                          << quantumResult.task_id << std::endl;
                std::cout << "                      L ...destination: "
                          << quantumResult.destination << std::endl;
                std::cout << "                      L ...execution_status: "
                          << quantumResult.execution_status << std::endl;
                std::cout << "                      L ...executed_qpu(s): {";
                for (const auto &qpu : quantumResult.executed_qpu)
                    std::cout << " " << qpu;
                std::cout << " }" << std::endl;
                std::cout
                    << "                      L ...additional_information: "
                    << quantumResult.additional_information << std::endl;
                std::cout << "                      L ...execution_time: "
                          << quantumResult.execution_time << " s." << std::endl;
                std::cout << "                      L ...executed_circuit(s): ";
                for (const auto &qir : quantumResult.executed_circuit)
                    std::cout << std::endl << qir;
                std::cout << std::endl << "Results: " << std::endl;
                for (const auto &result : quantumResult.results)
                    std::cout << "\t" << result.first << ": " << result.second
                              << std::endl;
                std::cout << std::endl;
            }
            else
            {
                std::cout << "[Quantum Daemon]......Error: Failed to receive "
                             "the results"
                          << std::endl;
            }
            // TODO: uncomment
            std::string entry_file(entry.path().stem());
            std::string copy_to = final_results_dir + "/" + entry_file;
            std::filesystem::path to_check = copy_to;

            if (std::filesystem::is_directory(to_check))
            {
                std::filesystem::remove_all(to_check);
            }

            std::filesystem::copy(default_results_dir, copy_to);
        }
    // Close the connections
    close_connections(&conn);

    return 0;
}
