# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Off by default so local builds aren't broken by pending -Wconversion findings;
# CI opts in explicitly.
option(QRMCI_WARNINGS_AS_ERRORS
       "Treat warnings as errors for QRM-owned targets" OFF)

function(qrmci_enable_warnings target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic
                                             -Wconversion -Wsign-conversion)
    if(QRMCI_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(QRMCI_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  endif()
endfunction()
