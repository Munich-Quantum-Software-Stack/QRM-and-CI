# Include all new schedulers here
set(SCHEDULERS_SOURCE_FILES scheduler_round_robin.cpp)

# Set RPATH for the shared libraries in this directory
set(CMAKE_INSTALL_RPATH "${INSTALL_PATH}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

llvm_map_components_to_libnames(LLVM_LIBS support
                                          core
                                          mcjit
                                          analysis
                                          scalaropts
                                          codegen
                                          transformutils
                                          ipo
                                          objcarcopts
                                          vectorize
                                          instcombine
                                          bitwriter)

# Compile each scheduler as a shared object
foreach(src_file ${SCHEDULERS_SOURCE_FILES})
  get_filename_component(src_name ${src_file} NAME_WE)
  add_library(${src_name} MODULE ${src_file})
  target_link_libraries(${src_name} PRIVATE QirPassRunner GeneratorRunner SchedulerRunner SelectorRunner
                                            qdmi qinfo fomac
                                            nlohmann_json::nlohmann_json rabbitmq::rabbitmq
                                            ${LLVM_LIBS})

  set_target_properties(
    ${src_name}
    PROPERTIES
      INSTALL_RPATH
      "${CMAKE_INSTALL_PREFIX}/lib"
      INSTALL_RPATH_USE_LINK_PATH TRUE)

  # Set include directories for the library
  target_include_directories(${src_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

  # Install the shared library
  install(TARGETS ${src_name}
          DESTINATION "${INSTALL_PATH}/lib")

endforeach()
