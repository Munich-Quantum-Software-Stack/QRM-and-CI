# include(FetchContent)

# FetchContent_Declare(
#   MQSSCI
#   GIT_REPOSITORY https://github.com/Munich-Quantum-Software-Stack/MQSS-Quantum-Compilation-Suite.git
#   GIT_TAG fc8fa7ff51650dfa78a5d29c68b24242ae01a47a # Use the latest tag
# )

# FetchContent_MakeAvailable(MQSSCI)

include(ExternalProject)

set(MQSSCI_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/mqssci-build)

ExternalProject_Add(mqssci_external
  GIT_REPOSITORY    https://github.com/akshay9594/MQSS-Passes-Suite.git
  GIT_TAG           82a50fad6d36814229d67047e5b0da332d3d906b
  PREFIX            ${CMAKE_BINARY_DIR}/_deps/mqssci-build
  SOURCE_DIR        ${CMAKE_BINARY_DIR}/_deps/mqssci-src
  BINARY_DIR        ${CMAKE_BINARY_DIR}/_deps/mqssci-src   # in-source make, adjust if out-of-source supported
  CONFIGURE_COMMAND ""   # no configure step needed for plain make
  BUILD_COMMAND bash -c "make setup-env && . /workspaces/QRM/build/_deps/mqssci-src/_deps/.venv/bin/activate && make build INSTALL_DIR=${MQSSCI_INSTALL_DIR}"
  INSTALL_COMMAND   make target INSTALL_DIR=${MQSSCI_INSTALL_DIR}
  # BUILD_IN_SOURCE   TRUE  # set if module-Y's Makefile expects to run from its source root
)

set(MQSSCI_SRC_DIR ${CMAKE_BINARY_DIR}/_deps/mqssci-src)