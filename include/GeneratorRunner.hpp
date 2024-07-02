/**
 * @file GeneratorRunner.hpp
 * @brief TODO
 */

#ifndef GENERATORRUNNER_HPP
#define GENERATORRUNNER_HPP

#include <algorithm>
#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <libgen.h>
#include <netinet/in.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <PassRunner.hpp>

using namespace llvm;
using llvm::orc::ThreadSafeModule;

struct QuantumTask;

std::vector<QuantumTask> invokeGenerator(QuantumTask& parentQuantumTask,
                                         const std::string &nameGenerator);

#endif // GENERATORRUNNER_HPP