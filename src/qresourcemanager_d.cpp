/**
 * @file qresourcemanager_d.cpp
 * @brief TODO
 */
#include <QuantumResourceManager.hpp>

using json = nlohmann::json;
using llvm::orc::ThreadSafeModule;

#define CHECK_ERR(a, b)                                                        \
    {                                                                          \
        if (a != QDMI_SUCCESS)                                                 \
        {                                                                      \
            std::cout << std::endl << "[Error]: " << a << " at " << b;         \
        }                                                                      \
    }

/**
 * @var conn
 * @brief TODO
 */
amqp_connection_state_t conn;

/**
 * @var session
 * @brief TODO
 */
QDMI_Session session = NULL;

/**
 * @todo Comment this function
 */
QuantumTask JSONToQuantumTask(const char *QuantumTask_str)
{
    QuantumTask task;

    json QuantumTask_json = json::parse(QuantumTask_str);

    if (!QuantumTask_json.contains("task_id"))
    {
        std::cout << "   [qresourcemanager_d]..Warning: task_id not defined"
                  << std::endl;
        return QuantumTask();
    }
    task.task_id = QuantumTask_json["task_id"];
    if (!QuantumTask_json.contains("parent_id"))
        task.parent_id = -1;
    else
        task.parent_id = QuantumTask_json["parent_id"];
    task.n_qbits = QuantumTask_json["n_qbits"];
    task.n_shots = QuantumTask_json["n_shots"];
    task.circuit_file = QuantumTask_json["circuit_file"];
    task.circuit_file_type = QuantumTask_json["circuit_file_type"];
    task.result_destination = QuantumTask_json["result_destination"];
    task.preferred_qpu = QuantumTask_json["preferred_qpu"];
    task.scheduled_qpu = QuantumTask_json["scheduled_qpu"];
    task.priority = QuantumTask_json["priority"];
    task.optimisation_level = QuantumTask_json["optimisation_level"];
    task.no_modify = QuantumTask_json["no_modify"];
    task.transpiler_flag = QuantumTask_json["transpiler_flag"];
    task.result_type = QuantumTask_json["result_type"];
    task.submit_time = QuantumTask_json["submit_time"];
    if (!QuantumTask_json.contains("circuit_qiskit"))
    {
        std::cout
            << "   [qresourcemanager_d]..Warning: circuit_qiskit not defined"
            << std::endl;
        return QuantumTask();
    }
    task.circuit_qiskit = QuantumTask_json["circuit_qiskit"];
    task.additional_information = QuantumTask_json["additional_information"];
    task.change_selector = QuantumTask_json["change_selector"];
    task.change_scheduler = QuantumTask_json["change_scheduler"];
    task.thread_safe_module = ThreadSafeModule();

    return task;
}

/**
 * @brief TODO
 * @param conn TODO
 * @param QDQueue TODO
 * @param receivedQirModule TODO
 * @param receivedScheduler TODO
 * @param receivedSelector TODO
 */
void handleQuantumDaemon(amqp_connection_state_t &conn, char const *QDQueue,
                         const QuantumTask &parentQuantumTask)
{

    // TODO THE TARGET-AGNOSTIC OPTIMZATION
    //      BEFORE CIRCUIT CUTTING
    // invokeTargetAgnosticPasses(parentQuantumTask.circuit_qiskit, passes);

    // Invoke the generator
    std::string generator = parentQuantumTask.change_generator == ""
                                ? "libgenerator_cutter.so"
                                : parentQuantumTask.change_generator;

    std::vector<QuantumTask> childQuantumTasks =
        invokeGenerator(parentQuantumTask, generator);

    if (childQuantumTasks.size() == 0)
    {
        std::cout
            << "   [qresourcemanager_d]..Warning: There was an error splitting "
               "the quantum circuit"
            << std::endl;
        return;
    }

    // Compile and execute each generated sub-circuit
    int err;
    std::vector<std::string> modules;
    std::vector<std::string> targets;
    std::map<std::string, int> results;
    auto start = std::chrono::steady_clock::now();
    for (auto &childQuantumTask : childQuantumTasks)
    {
        QDMI_Job job;
        QDMI_Library lib;
        QDMI_Fragment frag;

        std::cout << std::endl;

        // Invoke the scheduler
        std::string scheduler = childQuantumTask.change_scheduler == ""
                                    ? "libscheduler_round_robin.so"
                                    : childQuantumTask.change_scheduler;

        QDMI_Device device = invokeScheduler(scheduler, childQuantumTask);

        // childQuantumTask.setTargetDevice(device);

        if (device == NULL)
        {
            std::cout << "   [qresourcemanager_d]..Warning: "
                      << "There was an error obtaining the "
                      << "target architecture. The device could not "
                      << "be created." << std::endl;
            return;
        }

        FOMAC_print_coupling_mappings(device);

        const char *lastSlash = std::strrchr(device->library.libname, '/');
        if (lastSlash != nullptr)
            targets.push_back(std::string(lastSlash + 1));
        else
        {
            std::cout << "   [qresourcemanager_d]..Warning: "
                      << "Could not add name of device to "
                      << "the QuantumResult." << std::endl;
            return;
        }

        // Invoke the selector
        std::string selector = childQuantumTask.change_selector == ""
                                   ? "libselector_all.so"
                                   : childQuantumTask.change_selector;
        std::vector<std::string> passes = invokeSelector(selector);

        if (passes.empty())
        {
            std::cout << "   [qresourcemanager_d]..Warning: "
                      << "No passes were selected" << std::endl;
            return;
        }

        // Invoke the passes
        invokeTargetSpecificPasses(childQuantumTask.thread_safe_module, passes,
                                   device);
        // Create a fragment
        frag = (QDMI_Fragment)malloc(sizeof(struct QDMI_Fragment_d));
        if (frag == NULL)
        {
            std::cout << "   [qresourcemanager_d]..Warning: "
                      << "The fragment could not be created" << std::endl;

            return;
        }

        SmallVector<char, 0> buffer;
        childQuantumTask.thread_safe_module.withModuleDo(
            [&](Module &module)
            {
                std::string str;
                raw_string_ostream OS(str);
                OS << module; 
                OS.flush();
                const char *qir = str.data();
                modules.push_back((char *)qir);

                raw_svector_ostream ostream(buffer);
                WriteBitcodeToFile(module, ostream);

                if (buffer.empty())
                {
                    std::cout << "   [qresourcemanager_d]..Warning: "
                              << "Could not create bitcode" << std::endl;

                    return;
                }

                void* qirmod = static_cast<void*>(buffer.data());
                frag->sizebuffer = buffer.size();
                err = QDMI_control_pack_qir(device, qirmod, &frag);
            });

        // Submit the adapted QIR to the target platform
        err = QDMI_control_submit(device, &frag, childQuantumTask.n_shots,
                                  device->library.info, &job);
        CHECK_ERR(err, "QDMI_control_submit");

        // Get the results back
        int numbits = 0;
        // TODO Handle err
        QDMI_Status status;
        err = QDMI_control_readout_size(device, &status, &numbits);
        int *raw_numbers = (int *)malloc(((long)1 << numbits) * sizeof(int));
        if (raw_numbers == NULL)
        {
            std::cout << "   [qresourcemanager_d]..Warning: "
                      << "The results could not be fetched" << std::endl;

            return;
        }

        // TODO Handle err
        err = QDMI_control_readout_raw_num(
            device, 
            &status, 
            childQuantumTask.task_id, 
            raw_numbers
        );

        for (long i = 0; i < ((long)1 << numbits); i++)
            results[std::to_string(i)] = raw_numbers[i];

        free(raw_numbers);
        //free(frag->qirmod);
        free(frag);
        // free(device);
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;

    // Create JSON string to send back to the Quantum Daemon
    json QuantumResult_json = {
        {"task_id", -1},
        {"results", results},
        {"destination", ""},
        {"execution_status", true},
        {"executed_qpu", targets},
        {"executed_circuit", modules},
        {"additional_information", ""},
        {"execution_time", elapsed_seconds.count()},
    };

    std::string QuantumResult_str = QuantumResult_json.dump();

    // Send the results back to the Quantum Daemon
    send_message(&conn,                     // conn
                 QuantumResult_str.c_str(), // message
                 QDQueue);                  // queue

    std::cout
        << "   [qresourcemanager_d]..Adapted QIR sent to the Quantum Daemon"
        << std::endl;
}

/**
 * @brief Function for the graceful termination of this daemon closing
 * its own socket before exiting
 * @param signum Number of the interrupt signal
 */
void signalHandler(int signum)
{
    if (signum == SIGTERM)
    {
        int err;

        std::cerr << "   [qresourcemanager_d]..Stoping the QRM daemon"
                  << std::endl;

        // Close the connections
        std::cerr << "   [qresourcemanager_d]..Closing connections to RabbitMQ"
                  << std::endl;
        close_connections(&conn);

        // Finalize the QDMI session
        std::cerr << "   [qresourcemanager_d]..Finalizing QDMI session"
                  << std::endl;
        err = QDMI_session_finalize(session);
        CHECK_ERR(err, "QDMI_session_finalize");

        exit(0);
    }
}

/**
 * @brief The main entry point of the program.
 *
 * The Quantum Resource Manager daemon.
 *
 * @return int
 */
int main(int argc, char *argv[])
{
    //setbuf(stdout, NULL);

    if (argc != 1 && argc != 2 && argc != 3)
    {
        std::cerr << "   [qresourcemanager_d]..aemon_d [screen|log PATH]"
                  << std::endl;
        return 1;
    }

    std::string stream;

    if (argc == 1)
        stream = "screen";
    else if (argc == 2)
    {
        stream = argv[1];
        if (stream != "screen")
        {
            std::cerr << "   [qresourcemanager_d]..aemon_d [screen|log PATH]"
                      << std::endl;
            return 1;
        }
    }
    else if (argc == 3)
    {
        stream = argv[1];
        if (stream != "log")
        {
            std::cerr << "   [qresourcemanager_d]..aemon_d [screen|log PATH]"
                      << std::endl;
            return 1;
        }
    }

    // Fork the process to create a daemon
    //pid_t pid = fork();

    //if (pid < 0)
    //{
    //    std::cerr << "   [qresourcemanager_d]..Failed to fork" << std::endl;
    //    return 1;
    //}

    std::string filePath;

    if (stream == "log")
        filePath = std::string(argv[2]) + "/logs/qresourcemanager_d.log";

    //if (pid > 0)
    //{
    //    std::cout
    //        << "   [qresourcemanager_d]..To stop this daemon type: kill -15 "
    //        << pid << std::endl;
    //    if (stream == "log")
    //        std::cout << "   [qresourcemanager_d]..The log can be found in "
    //                  << filePath << std::endl;

    //    return 0;
    //}

    //// Create a new session and become the session leader
    //setsid();

    //// Change the working directory to root to avoid locking the current
    //// directory
    //chdir("/");

    //// Set up a signal handler for graceful termination
    //signal(SIGTERM, signalHandler);

    // Set the output stream
    if (stream == "log")
    {
        int logFileDescriptor = -1;

        logFileDescriptor = open(filePath.c_str(), O_CREAT | O_RDWR | O_APPEND,
                                 S_IRUSR | S_IWUSR);

        if (logFileDescriptor == -1)
        {
            std::cerr << "   [qresourcemanager_d]..Warning: Could not open the "
                         "log file"
                      << std::endl;
        }
        else
        {
            dup2(logFileDescriptor, STDOUT_FILENO);
            dup2(logFileDescriptor, STDERR_FILENO);
        }
    }

    // Establish a connection to the RabbitMQ server
    const char *QDQueue = "queue_daemon";
    const char *QRMQueue = "queue_manager";
    amqp_socket_t *socket = NULL;

    rabbitmq_new_connection(&conn, &socket);

    // Declare the Quantum Daemon queue
    amqp_queue_declare(conn, 1, amqp_cstring_bytes(QDQueue), 0, 1, 0, 0,
                       amqp_empty_table);

    // Declare the Quantum Resource Manager queue
    amqp_queue_declare(conn, 1, amqp_cstring_bytes(QRMQueue), 0, 1, 0, 0,
                       amqp_empty_table);

    amqp_rpc_reply_t consume_reply = amqp_get_rpc_reply(conn);

    if (consume_reply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        std::cout
            << "   [qresourcemanager_d]..Error starting to consume messages"
            << std::endl;
        return 1;
    }

    std::cout << "   [qresourcemanager_d]..Listening on queue " << QRMQueue
              << std::endl;

    // Start the QDMI session
    int err;
    QInfo info;

    err = QInfo_create(&info);
    CHECK_ERR(err, "QInfo_create");

    err = QDMI_session_init(info, &session);
    // CHECK_ERR(err, "QDMI_session_init");

    while (true)
    {
        // Receive a QuantumTask
        auto *task = receive_message(&conn,     // conn
                                     QRMQueue); // queue

        if (task)
        {
            QuantumTask parentQuantumTask = JSONToQuantumTask(task);

            std::cout << "   [qresourcemanager_d]..Received a QuantumTask"
                      << std::endl;

            //// Create a new thread that executes 'handleQuantumDaemon' to run
            //// the received scheduler, and the received selector targeting
            //// the received QIR
            //std::thread QuantumDaemonThread(handleQuantumDaemon,
            //    std::ref(conn),
            //    QDQueue, 
            //    parentQuantumTask
            //);

            //// Detach from this thread once done
            //QuantumDaemonThread.detach();

            handleQuantumDaemon(conn, QDQueue, parentQuantumTask);
        }
        else
        {
            std::cout
                << "   [qresourcemanager_d]..Error: Failed to receive the task"
                << std::endl;
        }
    }

    return 1;
}
