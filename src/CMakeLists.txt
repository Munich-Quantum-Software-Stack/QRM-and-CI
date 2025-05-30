add_subdirectory(common)
add_subdirectory(PassRunner)

configure_file("SimulateKernelLoweredToLLVM.py.in"
               "${CMAKE_BINARY_DIR}/SimulateKernelLoweredToLLVM.py" @ONLY)
