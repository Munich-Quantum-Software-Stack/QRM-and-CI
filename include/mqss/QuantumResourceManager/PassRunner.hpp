/**
 * @file GeneratorRunner.hpp
 * @brief TODO
 */

#pragma once

#include "mlir/IR/BuiltinOps.h"

using namespace mlir;

namespace QRM {

class PassRunner {
public:
  // initialize the pass runner
  void init();
  void invokePasses(ModuleOp circuit, const std::vector<std::string> &passes);
  void invokePasses(std::vector<ModuleOp> circuits,
                    const std::vector<std::string> &passes);
  void invokePasses(ModuleOp circuit, const std::vector<std::string> &passes,
                    std::string device);
  void invokePasses(std::vector<ModuleOp> circuits,
                    const std::vector<std::string> &passes, std::string device);
  void applyOptimizationLevel(ModuleOp circuit, int oLevel);
  void applyOptimizationLevel(std::vector<ModuleOp> circuits, int oLevel);
  void transpile(ModuleOp circuit);
  void transpile(std::vector<ModuleOp> circuits);

private:
};

} // namespace QRM
