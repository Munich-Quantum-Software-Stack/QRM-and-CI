#include "predictor.hpp"
#include "eval.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

/*
 * @brief Predict some figure of merit based on a pretrained ONNX model for each
 * model
 * @param TSM The quantum circuit to evaluate
 * @param models The trained models to predict some figure of merit
 * @return The predicted figure of merit for each model
 */
std::map<std::string, float> predict(const ThreadSafeModule &TSM,
                                     const std::vector<std::string> models)
{
    // Prepare circuit feature vector for model input
    std::map<std::string, int> gate_counts = {{"__quantum__qis__U3__body", 0},
                                              {"u2", 0},
                                              {"u1", 0},
                                              {"__quantum__qis__cnot__body", 0},
                                              {"id", 0},
                                              {"u0", 0},
                                              {"u", 0},
                                              {"p", 0},
                                              {"__quantum__qis__x__body", 0},
                                              {"__quantum__qis__y__body", 0},
                                              {"__quantum__qis__z__body", 0},
                                              {"__quantum__qis__h__body", 0},
                                              {"__quantum__qis__s__body", 0},
                                              {"sdg", 0},
                                              {"__quantum__qis__t__body", 0},
                                              {"tdg", 0},
                                              {"__quantum__qis__rx__body", 0},
                                              {"__quantum__qis__ry__body", 0},
                                              {"__quantum__qis__rz__body", 0},
                                              {"sx", 0},
                                              {"sxdg", 0},
                                              {"__quantum__qis__cz__body", 0},
                                              {"__quantum__qis__cy__body", 0},
                                              {"swap", 0},
                                              {"ch", 0},
                                              {"ccx", 0},
                                              {"cswap", 0},
                                              {"crx", 0},
                                              {"cry", 0},
                                              {"crz", 0},
                                              {"cu1", 0},
                                              {"cp", 0},
                                              {"cu3", 0},
                                              {"csx", 0},
                                              {"cu", 0},
                                              {"rxx", 0},
                                              {"rzz", 0},
                                              {"rccx", 0},
                                              {"rc3x", 0},
                                              {"c3x", 0},
                                              {"c3sqrtx", 0},
                                              {"c4x", 0},
                                              {"__quantum__qis__mz__body", 0}};

    // Create a memory information object
    Ort::MemoryInfo memory_info =
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    // Generate supermarq+ features
    std::vector<double> supermarq_plus =
        evaluate_supermarq_plus(TSM, gate_counts);

    // Prepare input tensor
    std::array<float, 52> input_data;
    int i = 0;
    // Fill input_data with gate_counts values
    for (const auto &pair : gate_counts)
    {
        input_data[i++] = (float)(pair.second);
    }
    // Fill the rest of input_data with supermarq_plus values
    for (const double &value : supermarq_plus)
    {
        input_data[i++] = (float)(value);
    }
    std::vector<int64_t> input_shape = {1, 52};
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_data.data(), input_data.size(), input_shape.data(),
        input_shape.size());

    // Set the input
    std::vector<const char *> input_node_names = {"float_input"};
    std::vector<Ort::Value> input_tensors;
    input_tensors.push_back(std::move(input_tensor));

    // Initialize session options
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);

    // Initialize the environment
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "Predictor");

    // Create a map to store the results
    std::map<std::string, float> results;

    // Loop over all models
    for (const std::string &model : models)
    {
        // Declare the session pointer
        Ort::Session *session = nullptr;

        std::string model_path; // Import the model for desired figure of merit
        model_path =
            "include/scheduler_runner/predictor_models/" + model + ".onnx";

        std::ifstream file(model_path, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        try
        {
            std::vector<char> buffer(size);
            if (file.read(buffer.data(), size))
            {
                // Initialize the session
                session = new Ort::Session(env, buffer.data(), buffer.size(),
                                           session_options);
            }
            else
            {
                throw std::runtime_error("Failed to read model file: " +
                                         model_path);
            }
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Error during model import: " << ex.what()
                      << std::endl;
        }

        // Prepare output tensor
        std::vector<const char *> output_node_names = {"variable"};
        std::array<float, 1> output_data;
        std::vector<int64_t> output_shape = {1, 1};
        Ort::Value output_tensor = Ort::Value::CreateTensor<float>(
            memory_info, output_data.data(), output_data.size(),
            output_shape.data(), output_shape.size());

        // Add output_tensor to output_tensors
        std::vector<Ort::Value> output_tensors;
        output_tensors.push_back(std::move(output_tensor));

        // Run the model
        session->Run(Ort::RunOptions{nullptr}, input_node_names.data(),
                     input_tensors.data(), input_tensors.size(),
                     output_node_names.data(), output_tensors.data(),
                     output_tensors.size());
        float *floatarr = output_tensors[0].GetTensorMutableData<float>();

        // Add the model string and model score to the map
        results[model] = floatarr[0];

        // Delete the session after use
        delete session;
    }

    // Return the map of model strings and model scores
    return results;
}
