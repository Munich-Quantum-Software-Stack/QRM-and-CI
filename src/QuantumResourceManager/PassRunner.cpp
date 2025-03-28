#include "mqss/QuantumResourceManager/PassRunner.hpp"

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

using namespace mlir;
using namespace mqss::opt;

namespace QRM {

void PassRunner::invokePasses(ModuleOp circuit,
                              const std::vector<std::string> &passes) {
  std::cout << "Invoking Target Agnostic Passes" << std::endl;
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

  //  if (mlir::failed(pm.parsePassPipeline(passPipelineRef))) {
  //    llvm::errs() << "Failed to parse pass pipeline: " << passPipeline <<
  //    "\n"; return;
  //  }
}

void PassRunner::invokePasses(ModuleOp circuit,
                              const std::vector<std::string> &passes,
                              std::string device) {
  std::cout << "Invoking Target Specific Passes" << std::endl;
  mlir::PassManager pm(circuit.getContext());
}

void PassRunner::applyOptimizationLevel(ModuleOp circuit, int oLevel) {
  std::cout << "Invoking Optimization Level" << oLevel << std::endl;
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

} // namespace QRM
