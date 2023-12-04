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
#include <llvm/IR/Module.h>
#include <netinet/in.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace llvm;

std::vector<std::string> invokeSelector(std::unique_ptr<Module> &module,
                                        const std::string &nameSelector);

#endif // SELECTORRUNNER_HPP
