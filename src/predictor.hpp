#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#include <array>
#include <vector>
#include <cstdio>

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>

using llvm::orc::ThreadSafeModule;

float predict(ThreadSafeModule &TSM);

#endif // PREDICTOR_H