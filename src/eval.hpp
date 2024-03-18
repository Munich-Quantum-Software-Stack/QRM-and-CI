#ifndef EVAL_HPP
#define EVAL_HPP

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>
#include <map>

using llvm::orc::ThreadSafeModule;

int evaluate_depth(ThreadSafeModule &TSM);
int evaluate_gates(ThreadSafeModule &TSM);
double evaluate_entanglement_ratio(ThreadSafeModule &TSM);
double evaluate_parallelism(ThreadSafeModule &TSM);
double evaluate_critical_depth(ThreadSafeModule &TSM);
std::map<std::string, int> evaluate_gate_counts(ThreadSafeModule &TSM);

#endif // EVAL_HPP
