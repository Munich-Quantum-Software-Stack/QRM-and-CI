#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <array>
#include <cstdio>
#include <map>
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#include <vector>

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>

using llvm::orc::ThreadSafeModule;

float predict(ThreadSafeModule &TSM);

#endif // PREDICTOR_H
