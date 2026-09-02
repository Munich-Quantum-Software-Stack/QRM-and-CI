/*
 * Copyright (c) 2026 MQSS Maintainers
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/// @file IntegrationTestHelpers.h
/// @brief Shared fixtures for the tests/integration client programs, which
///        exercise a live QRM&CI deployment (apps/standalone or
///        apps/distributed) over a real RabbitMQ/QDMI setup.

#pragma once

#include <string>

namespace mqss::qrmci::test {

/// @brief A compilable/executable 2-qubit Quake circuit, identical to the one
///        used by submit_task.cpp. Shared by the integration test client
///        programs that need a circuit the "quake" input format and the
///        example QDMI device both accept.
/// @return The Quake circuit source.
inline const std::string &sampleQuakeCircuit() {
  static const std::string circuit = R"(
  func.func @__nvqpp__mlirgen__testILm2EE() attributes {"cudaq-entrypoint", "cudaq-kernel"} {
  %0 = quake.alloca !quake.veq<2>
  %1 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x %1 : (!quake.ref) -> ()
  %2 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %3 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%2] %3 : (!quake.ref, !quake.ref) -> ()
  %4 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %5 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%4] %5 : (!quake.ref, !quake.ref) -> ()
  %6 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %7 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%6] %7 : (!quake.ref, !quake.ref) -> ()
  %8 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %9 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%8] %9 : (!quake.ref, !quake.ref) -> ()
  %10 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  %11 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x [%10] %11 : (!quake.ref, !quake.ref) -> ()
  %12 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  quake.x %12 : (!quake.ref) -> ()
  %13 = quake.extract_ref %0[1] : (!quake.veq<2>) -> !quake.ref
  %14 = quake.extract_ref %0[0] : (!quake.veq<2>) -> !quake.ref
  quake.x [%13] %14 : (!quake.ref, !quake.ref) -> ()
  %measOut = quake.mz %0 : (!quake.veq<2>) -> !cc.stdvec<!quake.measure>
  return
  }
  )";
  return circuit;
}

} // namespace mqss::qrmci::test
