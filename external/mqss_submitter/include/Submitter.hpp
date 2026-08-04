/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "qdmi.hpp"

namespace mqss {

class Submitter {
public:
  Submitter() = delete; // Delete default constructor to enforce initialization
                        // with parameters

  // Constructor to initialize the Submitter with the backend driver, device
  // name, and authentication token
  Submitter(const std::string &driver_name, const std::string &device_name,
            const std::string &token);

  // Function to submit a quantum task to the backend
  std::vector<std::optional<std::map<std::string, size_t>>>
  submitTask(const std::vector<std::string> &progs, size_t shots,
             QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QIRBASESTRING);

private:
  qdmi::Session session_;
  qdmi::Device device_;
  bool device_available_;
};

} // namespace mqss
