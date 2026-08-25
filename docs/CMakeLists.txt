# Copyright (c) 2026 MQSS Maintainers
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(Doxygen REQUIRED)
include(FetchContent)

FetchContent_Declare(
  doxygen-awesome-css
  GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
  GIT_TAG ${QRMCI_DOXYGEN_AWESOME_GIT_TAG})
FetchContent_MakeAvailable(doxygen-awesome-css)
FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

set(QRMCI_DOXYGEN_OUTPUT_DIR "${PROJECT_BINARY_DIR}/docs")
set(QRMCI_DOXYGEN_CONFIG_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in")
set(QRMCI_DOXYGEN_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
set(PROJECT_LOGO "mqss_logo.svg")

set(QRMCI_DOXYGEN_INPUT_DIRS
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/index.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/getting-started.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/development-guide.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/api-reference.md")
string(REPLACE ";" " " QRMCI_DOXYGEN_INPUT_DIRS "${QRMCI_DOXYGEN_INPUT_DIRS}")

file(
  GLOB_RECURSE
  QRMCI_DOXYGEN_INPUT_FILES
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/*.hpp"
  "${PROJECT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.md"
  "${CMAKE_CURRENT_SOURCE_DIR}/_static/*.svg"
  "${CMAKE_CURRENT_SOURCE_DIR}/header.html"
  "${CMAKE_CURRENT_SOURCE_DIR}/layout.xml"
  "${CMAKE_CURRENT_SOURCE_DIR}/style.css")

configure_file("${QRMCI_DOXYGEN_CONFIG_FILE_IN}" "${QRMCI_DOXYGEN_CONFIG_FILE}"
               @ONLY)

file(MAKE_DIRECTORY "${QRMCI_DOXYGEN_OUTPUT_DIR}/html")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/_static/mqss_logo_dark.svg"
               "${QRMCI_DOXYGEN_OUTPUT_DIR}/html/mqss_logo_dark.svg" COPYONLY)

add_custom_command(
  OUTPUT "${QRMCI_DOXYGEN_OUTPUT_DIR}/html/index.html"
  COMMAND "${CMAKE_COMMAND}" -E make_directory
          "${QRMCI_DOXYGEN_OUTPUT_DIR}/html"
  COMMAND "${DOXYGEN_EXECUTABLE}" "${QRMCI_DOXYGEN_CONFIG_FILE}"
  DEPENDS ${QRMCI_DOXYGEN_INPUT_FILES} "${QRMCI_DOXYGEN_CONFIG_FILE}"
          "${QRMCI_DOXYGEN_CONFIG_FILE_IN}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Generating QRM&CI documentation with Doxygen"
  VERBATIM)

add_custom_target(docs DEPENDS "${QRMCI_DOXYGEN_OUTPUT_DIR}/html/index.html")
