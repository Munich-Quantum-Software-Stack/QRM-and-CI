/* This code and any associated documentation is provided "as is"

Copyright 2024 Munich Quantum Software Stack Project

Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
"License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

TODO

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-------------------------------------------------------------------------
  author Martin Letras
  date   April 2025
  version 1.0
  brief
        Definition of the header of the scheduler. Signature functions to
        invoke the scheduler.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#pragma once

#include "llvm.hpp"

#include <QuantumResourceManager.hpp>
#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <dlfcn.h>
#include <fcntl.h>
#include <fomac.hpp>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <qdmi.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

struct QuantumTask;

int invokeScheduler(const std::string &nameScheduler,
                    std::vector<QuantumTask> *childQuantumTasks);
