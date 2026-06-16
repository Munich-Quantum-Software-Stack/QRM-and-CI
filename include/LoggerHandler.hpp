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
        Definition of the name of the loggers and logger files. Each module
        of the QRM has its own.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#pragma once

#define LOGGER_QRM "mqss::QRM"
#define LOGGER_AGNOSTIC_PASS_RUNNER "mqss::AgnosticPassRunner"
#define LOGGER_SCHEDULER "mqss::Scheduler"
#define LOGGER_TRANSPILER "mqss::Transpiler"
#define LOGGER_SUBMITTER "mqss::Submitter"
#define LOGGER_MOCK_DEVICE "mqss::MockDevice"

#define FILE_LOGGER_QRM "QRM.log"
#define FILE_LOGGER_AGNOSTIC_PASS_RUNNER "AgnosticPassRunner.log"
#define FILE_LOGGER_SCHEDULER "Scheduler.log"
#define FILE_LOGGER_TRANSPILER "Transpiler.log"
#define FILE_LOGGER_SUBMITTER "Submitter.log"
#define FILE_LOGGER_MOCK_DEVICE "Mock-Device.log"
