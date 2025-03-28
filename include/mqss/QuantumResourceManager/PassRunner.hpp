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
  void invokePasses(ModuleOp circuit, const std::vector<std::string> &passes,
                    std::string device);
  void applyOptimizationLevel(ModuleOp circuit, int oLevel);

private:
};

} // namespace QRM
