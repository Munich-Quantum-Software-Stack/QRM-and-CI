#include "predictor.hpp"
#include "eval.hpp"

float predict(ThreadSafeModule &TSM)
{
    // Initialize session options
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);

    // Initialize the environment
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "ModelPrediction");

    // Initialize the session
    Ort::Session session(env,
                         "/home/ubuntu/mqt/mqt-predictor/evaluations/"
                         "supervised_ml_models/model.onnx",
                         session_options);

    // Create a memory information object
    Ort::MemoryInfo memory_info =
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    // Prepare input tensor
    std::map<std::string, int> gate_counts = evaluate_gate_counts(TSM);
    int num_qubits = 5; // evaluate_num_qubits(TSM);
    int depth = 100;    // evaluate_depth(TSM);
    // Supermarq features
    float program_communication = 0.5; // evaluate_program_communication(TSM);
    float critical_depth = 0.5;        // evaluate_critical_depth(TSM);
    float entanglement_ratio = 0.5;    // evaluate_entanglement_ratio(TSM);
    float parallelism = 0.5;           // evaluate_parallelism(TSM);
    float liveness = 0.5;              // evaluate_liveness(TSM);
    float directed_program_communication =
        0.5; // evaluate_directed_program_communication(TSM);
    float singleQ_gates_per_layer =
        0.5; // evaluate_singleQ_gates_per_layer(TSM);
    float multiQ_gates_per_layer = 0.5; // evaluate_multiQ_gates_per_layer(TSM);
    float my_critical_depth = 0.5;      // evaluate_my_critical_depth(TSM);

    std::array<float, 40> input_data;
    for (int i = 0; i < 40; i++)
    {
        input_data[i] = 1.0f;
    }
    std::vector<int64_t> input_shape = {1, 40};
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_data.data(), input_data.size(), input_shape.data(),
        input_shape.size());

    // Set the input
    std::vector<const char *> input_node_names = {"float_input"};
    std::vector<Ort::Value> input_tensors;
    input_tensors.push_back(std::move(input_tensor));

    // Prepare output tensor
    std::vector<const char *> output_node_names = {"variable"};
    std::array<float, 1> output_data;
    std::vector<int64_t> output_shape = {1, 1};
    Ort::Value output_tensor = Ort::Value::CreateTensor<float>(
        memory_info, output_data.data(), output_data.size(),
        output_shape.data(), output_shape.size());

    // Add output_tensor to output_tensors
    // Add output_tensor to output_tensors
    std::vector<Ort::Value> output_tensors;
    output_tensors.push_back(std::move(output_tensor));

    // Run the model
    session.Run(
        Ort::RunOptions{nullptr}, input_node_names.data(), input_tensors.data(),
        input_tensors.size(), output_node_names.data(), output_tensors.data(),
        output_tensors.size()); // Use the output tensors for application
    float *floatarr = output_tensors[0].GetTensorMutableData<float>();

    return floatarr[0];
}
