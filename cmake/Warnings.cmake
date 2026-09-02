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

# find_package'd IMPORTED targets (GTest, fmt, Boost, ...) already have their
# include directories treated as SYSTEM by CMake by default. Targets pulled in
# via FetchContent (mqss, mqss-ci, mqss_scheduler, mqss_submitter) are regular,
# non-IMPORTED targets, so that default doesn't apply to them: without this, the
# -Wconversion/-Wpedantic/etc. flags qrmci_enable_warnings adds to QRM-owned
# targets also fire on third-party header content those targets include. Call
# this on the real (non-ALIAS) dependency target once, after it's created, to
# mark it SYSTEM for every consumer.
function(qrmci_treat_as_system target)
  if(NOT TARGET ${target})
    return()
  endif()
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.25")
    set_target_properties(${target} PROPERTIES SYSTEM TRUE)
  endif()
endfunction()
