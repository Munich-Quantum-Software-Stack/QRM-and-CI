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
        Base class defining the logger of the QRM.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#pragma once

#include <memory>
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

namespace mqss {

class Logger {
public:
  // Initialize the logger (call this once in the main process)
  static void init(const std::string &logFilePath,
                   const std::string &loggerName = "default_logger");

  // Get the logger instance (can be called by any process or thread)
  static std::shared_ptr<spdlog::logger> getLogger();

  // Clean up the logger (call this before exiting)
  static void cleanup();

private:
  static std::shared_ptr<spdlog::logger> logger; // Shared logger instance
  static std::once_flag init_flag;               // Ensure single initialization
  static std::string loggerName;                 // Unique logger name
};

} // namespace mqss
