#ifndef EVAL_HPP
#define EVAL_HPP

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>
#include <map>

using llvm::orc::ThreadSafeModule;

std::vector<double>
evaluate_supermarq_plus(ThreadSafeModule &TSM,
                        std::map<std::string, int> &gate_counts);

#endif // EVAL_HPP
