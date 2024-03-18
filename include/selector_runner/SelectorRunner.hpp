/**
 * @file SelectorRunner.hpp
 * @brief TODO
 */

#ifndef SELECTORRUNNER_HPP
#define SELECTORRUNNER_HPP

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

#include <qdmi.h>

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>

using namespace llvm;
using llvm::orc::ThreadSafeModule;

std::vector<std::string> invokeSelector(const std::string &nameSelector,
                                        ThreadSafeModule &TSM, 
                                        QDMI_Device &device);

#endif // SELECTORRUNNER_HPP
