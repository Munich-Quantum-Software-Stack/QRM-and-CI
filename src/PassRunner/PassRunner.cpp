#include "mqss/PassRunner/PassRunner.hpp"

#include "Optimizer/Pipelines.hpp"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Pass/PassRegistry.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"

#include <functional>
#include <iostream>
#include <unordered_map>
// include generated pass declaration
#include "Passes/Decompositions.h.inc"
#include "Passes/Transforms.h.inc"
// cudaq includes
#include "cudaq/Frontend/nvqpp/AttributeNames.h"
#include "cudaq/Optimizer/Transforms/Passes.h"
// includes in runtime
#include "common/RuntimeMLIR.h"
using namespace mlir;
using namespace mqss::opt;

namespace QRM {

void PassRunner::invokePasses(ModuleOp circuit,
                              const std::vector<std::string> &passes) {
#ifdef DEBUG
  std::cout << "Invoking Target Agnostic Passes" << std::endl;
#endif
  // Join the vector into a single string with commas
  std::string passPipeline = llvm::join(passes, ",");
  mlir::PassManager pm(circuit.getContext());
  // Parse the pass pipeline
  llvm::StringRef passPipelineRef(passPipeline);
  std::string errMsg;
  llvm::raw_string_ostream errOs(errMsg);
  // Add additional passes if necessary
  if (failed(parsePassPipeline(passPipelineRef, pm, errOs))) {
    llvm::errs() << "Failed to parse pass pipeline: " << passPipeline << " "
                 << errOs.str() << "\n";
    return;
  }
  if (mlir::failed(pm.run(circuit)))
    std::runtime_error("The pass failed...");
}

void PassRunner::invokePasses(std::vector<ModuleOp> circuits,
                              const std::vector<std::string> &passes) {
  for (auto module : circuits) {
    invokePasses(module, passes); // later explore a way to parallelize this!
  }
}

void PassRunner::invokePasses(ModuleOp circuit,
                              const std::vector<std::string> &passes,
                              std::string device) {
#ifdef DEBUG
  std::cout << "Invoking Target Specific Passes" << std::endl;
#endif
  mlir::PassManager pm(circuit.getContext());
}

void PassRunner::invokePasses(std::vector<ModuleOp> circuits,
                              const std::vector<std::string> &passes,
                              std::string device) {
  for (auto module : circuits) {
    invokePasses(module, passes,
                 device); // later explore a way to parallelize this!
  }
}

void PassRunner::applyOptimizationLevel(ModuleOp circuit, int oLevel) {
#ifdef DEBUG
  std::cout << "Invoking Optimization Level" << oLevel << std::endl;
#endif
  mlir::PassManager pm(circuit.getContext());
  // Function map
  std::unordered_map<int, std::function<void(mlir::PassManager &)>>
      functionMap = {
          {0, [](mlir::PassManager &) {}}, {1, O1}, {2, O2}, {3, O3}};
  // Lookup and invoke function
  if (auto it = functionMap.find(oLevel); it != functionMap.end())
    it->second(pm); // Call function with mlir::PassManager
  else
    std::cout << "Optimization level" << oLevel << " not found!\n";
  // applying the passes
  if (mlir::failed(pm.run(circuit)))
    std::runtime_error("The pass failed...");
}

void PassRunner::applyOptimizationLevel(std::vector<ModuleOp> circuits,
                                        int oLevel) {
  for (auto module : circuits) {
    applyOptimizationLevel(module,
                           oLevel); // later explore a way to parallelize this!
  }
}

void PassRunner::transpile(ModuleOp circuit) {
  using namespace cudaq::opt;
  std::string basis[] = {
      "phased_rx",
      "z(1)",
  };
#ifdef DEBUG
  std::cout << "Transpiling circuit to IQM" << std::endl;
#endif
  mlir::PassManager pm(circuit.getContext());
  BasisConversionPassOptions options;
  options.basis = basis;
  pm.addPass(createBasisConversionPass(options));
  // pass to canonical form and remove non-used operations
  pm.addPass(mlir::createCanonicalizerPass());
  pm.addPass(mlir::createCSEPass());
  if (mlir::failed(pm.run(circuit)))
    std::runtime_error("The pass failed...");
}

void PassRunner::transpile(std::vector<ModuleOp> circuits) {
  for (auto module : circuits) {
    transpile(module); // later explore a way to parallelize this!
  }
}
} // namespace QRM
