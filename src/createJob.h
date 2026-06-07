/****************************************************************-*- C++ -*-****
 * Copyright (c) 2022 - 2026 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/
#include "lrz_qdmi/device.h"
#include <regex>
#include <stdio.h>
#include <string>
#include "qdmi/constants.h"

#define LRZ_HOST_URL "LRZ_HOST_URL"
#define MQP_SECRET_TOKEN "MQP_SECRET_TOKEN"


#define EXIT_ON_FAIL(func, msg)                                                \
  {                                                                            \
    err = func;                                                                \
    if (err != QDMI_SUCCESS) {                                                 \
      std::cout << msg << std::endl;                                           \
      exit(err);                                                               \
    }                                                                          \
  }

enum HardwareTypes {
  SIMULATOR,
  SUPERCONDUCTING,
  IONTRAP,
  NEUTRALATOMS,
  UNKNOWN
};

struct HardwareSession {
  std::string name="";
  size_t n_qubit=-1;
  HardwareTypes type = UNKNOWN;
  std::shared_ptr<LRZ_QDMI_Device_Session> session;
};

std::vector<HardwareSession> HARDWARE_SESSIONS = {
    {"QExa20", 20, SUPERCONDUCTING, nullptr}
    // {"Q20", 20, SUPERCONDUCTING, nullptr},
    
    // {"QLM", 38, SIMULATOR, nullptr},
    // {"AQT20", 20, IONTRAP, nullptr},
    // {"MUNIQC-Atoms20", 20, NEUTRALATOMS, nullptr},
    // {"Q5", 5, SUPERCONDUCTING, nullptr},
    // {"WMI3", 3, SUPERCONDUCTING, nullptr},
    // {"MAQCS", 12, SIMULATOR, nullptr},
};

std::unordered_map<std::string, HardwareSession *> NAME_SESSIONS = {
    {"QExa20", &HARDWARE_SESSIONS[1]}
    // {"Q20", &HARDWARE_SESSIONS[0]},
    
    // {"QLM", &HARDWARE_SESSIONS[2]},
    // {"AQT20", &HARDWARE_SESSIONS[3]},
    // {"MUNIQC-Atoms20", &HARDWARE_SESSIONS[4]},
    // {"Q5", &HARDWARE_SESSIONS[5]},
    // {"WMI3", &HARDWARE_SESSIONS[6]},
    // {"MAQCS", &HARDWARE_SESSIONS[7]}
};

// #define CREATE_JOB(job, n_shot, format, program)                               \
//   {                                                                            \
//     assert(LRZ_QDMI_device_session_create_device_job(session, &job),        \
//               QDMI_SUCCESS);                                                   \
//     ASSERT_EQ(                                                                 \
//         LRZ_QDMI_device_job_set_parameter(                                     \
//             job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(n_shot), &n_shot), \
//         QDMI_SUCCESS);                                                         \
//     ASSERT_EQ(LRZ_QDMI_device_job_set_parameter(                               \
//                   job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,                \
//                   sizeof(format), &format),                                    \
//               QDMI_SUCCESS);                                                   \
//     ASSERT_EQ(LRZ_QDMI_device_job_set_parameter(                               \
//                   job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, strlen(program) + 1, \
//                   program),                                                    \
//               QDMI_SUCCESS);                                                   \
//   }