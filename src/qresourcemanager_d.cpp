/**
 * @file qresourcemanager_d.cpp
 * @brief TODO
 */
#include <QuantumResourceManager.hpp>

using json = nlohmann::json;
using llvm::orc::ThreadSafeModule;
using llvm::orc::ThreadSafeContext;

#define CHECK_ERR(a, b)                                                        \
    {                                                                          \
        if (a != QDMI_SUCCESS)                                                 \
        {                                                                      \
            std::cout << std::endl << "   [qresourcemanager_d]..Warning: " << b << " returned with status " << a << std::endl;         \
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
    task.priority = QuantumTask_json["priority"];
    task.optimisation_level = QuantumTask_json["optimisation_level"];
    task.no_modify = QuantumTask_json["no_modify"];
    task.transpiler_flag = QuantumTask_json["transpiler_flag"];
    task.result_type = QuantumTask_json["result_type"];
    task.submit_time = QuantumTask_json["submit_time"];
    if (!QuantumTask_json.contains("qir"))
    {
        std::cout
            << "   [qresourcemanager_d]..Warning: Generic QIR missing"
            << std::endl;
        return QuantumTask();
    }
    task.qir = QuantumTask_json["qir"];
    task.additional_information = QuantumTask_json["additional_information"];
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
                         QuantumTask &parentQuantumTask)
{
    int err;
    auto start = std::chrono::steady_clock::now();

    // Insert LLVM::ThreadSafeModule to parentQuantumTask
    ThreadSafeContext TSCtx(std::make_unique<LLVMContext>());
    SMDiagnostic error;
    std::string circuit = parentQuantumTask.qir;
    auto M = parseIR(MemoryBufferRef(circuit, "QIR (LRZ)"), error,
                      *TSCtx.getContext());
    ThreadSafeModule TSM = ThreadSafeModule(std::move(M), std::move(TSCtx));
    parentQuantumTask.thread_safe_module = std::move(TSM);

    // Invoke the target-agnostic selector
    std::vector<std::string> agnosticPasses = invokeTargetAgnosticSelector("libselector_agnostic.so");

    if (agnosticPasses.empty())
    {
        std::cout << "   [qresourcemanager_d]..Warning: "
                  << "No passes were selected" << std::endl;
        return;
    }

    // Invoke target-agnostic passes
    invokePasses(parentQuantumTask.thread_safe_module, agnosticPasses);

    // Invoke the generator
    std::vector<QuantumTask> childQuantumTasks =
        invokeGenerator(parentQuantumTask, "libgenerator_cutter.so");

    if (childQuantumTasks.size() == 0)
    {
        std::cout
            << "   [qresourcemanager_d]..Warning: There was an error splitting "
               "the quantum circuit"
            << std::endl;
        return;
    }

    // Invoke the scheduler
    err = invokeScheduler("libscheduler_round_robin.so", &childQuantumTasks);
    CHECK_ERR(err, "invokeScheduler");

    // Compile and execute each generated sub-circuit
    std::vector<std::string> modules;
    std::vector<std::string> targets;
    std::map<std::string, int> results;
    for (auto &childQuantumTask : childQuantumTasks)
    {
        QDMI_Job job = (QDMI_Job)malloc(sizeof(struct QDMI_Job_impl_d));
        QDMI_Library lib;
        QDMI_Fragment frag;

        QDMI_Device device = childQuantumTask.scheduled_qpu;

        if (device == NULL)
        {
            std::cout << "   [qresourcemanager_d]..Warning: "
                      << "There was an error obtaining the "
                      << "target architecture. The device could not "
                      << "be created." << std::endl;
            return;
        }

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

        // Invoke the target-specific selector
        std::vector<std::string> specificPasses = invokeTargetSpecificSelector("libselector_specific.so", device);

        if (specificPasses.empty())
        {
            std::cout << "   [qresourcemanager_d]..Warning: "
                      << "No passes were selected" << std::endl;
            return;
        }

        // Invoke target-specific passes
        invokePasses(childQuantumTask.thread_safe_module, specificPasses, device);

        // Create a fragment
        frag = (QDMI_Fragment)malloc(sizeof(struct QDMI_Fragment_d));
        if (frag == NULL)
        {
            std::cout << "   [qresourcemanager_d]..Warning: "
                      << "The fragment could not be created" << std::endl;

            return;
        }

        SmallVector<char, 0> buffer;
        std::string str;
        raw_string_ostream OS(str);
        int nqubits = 0;
        childQuantumTask.thread_safe_module.withModuleDo(
            [&](Module &module)
            {
                OS << module; 
                OS.flush();
                modules.push_back(str.data());
                //std::cout << "   [qresourcemanager_d]..Packing QIR: " 
                //          << std::endl << std::endl
                //          << str.data() << std::endl;
                raw_svector_ostream ostream(buffer);
                WriteBitcodeToFile(module, ostream);

                if (buffer.empty())
                {
                    std::cout << "   [qresourcemanager_d]..Warning: "
                              << "Could not create bitcode" << std::endl;

                    return;
                }

                void* qirmod = static_cast<void *>(buffer.data());
                frag->sizebuffer = buffer.size();
                err = QDMI_control_pack_qir(device, qirmod, &frag);
                CHECK_ERR(err, "QDMI_control_pack_qir");
            });

        // Submit the adapted QIR to the target platform
        job->task_id = childQuantumTask.task_id;
        err = QDMI_control_submit(device, &frag, childQuantumTask.n_shots,
                                  device->library.info, &job);
        CHECK_ERR(err, "QDMI_control_submit");

        // Wait for the results to be ready
        QDMI_Status status;
        err = QDMI_control_wait(device, &job, &status);
        CHECK_ERR(err, "QDMI_control_wait");

        // Get the results back
        if (nqubits == 0)
        {
            err = QDMI_control_readout_size(device, &status, &nqubits);
            CHECK_ERR(err, "QDMI_control_readout_size");

            if (nqubits == 0)
            {
                std::cout << "   [qresourcemanager_d]..Warning: "
                          << "The results could not be fetched" << std::endl;

                return;
            }
        }

        int* raw_numbers = new int[1 << nqubits];
        memset(raw_numbers, 0, sizeof(int) * (1 << nqubits));

        err = QDMI_control_readout_raw_num(
            device, 
            &status, 
            job->task_id, 
            raw_numbers
        );
        CHECK_ERR(err, "QDMI_control_readout_raw_num");

        for (long i = 0; i < ((long)1 << nqubits); i++)
        {
            //std::cout << "\t" << raw_numbers[i] << std::endl;
            if (raw_numbers[i] > 0)
                results[std::to_string(i)] += raw_numbers[i];
        }

        free(raw_numbers);
        free(frag);
        free(job);
        free(device);
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed_milliseconds = end - start;
    std::cout << "   [qresourcemanager_d]..Elapsed milliseconds: " << elapsed_milliseconds.count() << std::endl;

    // Create JSON string to send back to the Quantum Daemon
    json QuantumResult_json = {
        {"task_id", -1},
        {"results", results},
        {"destination", ""},
        {"execution_status", true},
        {"executed_qpu", targets},
        {"executed_circuit", modules},
        {"additional_information", ""},
        {"execution_time", elapsed_milliseconds.count()},
    };

    std::string QuantumResult_str = QuantumResult_json.dump();

    // Send the results back to the Quantum Daemon
    send_message(&conn,                     // conn
                 QuantumResult_str.c_str(), // message
                 QDQueue);                  // queue

    std::cout
        << "   [qresourcemanager_d]..Executed QIR circuits sent back to the Quantum Daemon"
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

    // Start the QDMI session
    int err;
    QInfo info;

    err = QInfo_create(&info);
    CHECK_ERR(err, "QInfo_create");

    err = QDMI_session_init(info, &session);
    CHECK_ERR(err, "QDMI_session_init");

    while (true)
    {
        std::cout << "   [qresourcemanager_d]..Waiting for a new job" << std::endl;

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
