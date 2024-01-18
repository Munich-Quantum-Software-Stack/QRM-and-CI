add_library(nsga2 SHARED nsga2.cpp
                         allocate.cpp
                         crossover.cpp
                         crowddist.cpp
                         dominance.cpp
                         eval.cpp
                         fillnds.cpp
                         initialize.cpp
                         list.cpp
                         merge.cpp
                         mutation.cpp
                         rand.cpp
                         rank.cpp
                         report.cpp
                         sort.cpp
                         tourselect.cpp)

set(CUSTOM_NSGA2_LIBRARY_PATH "${INSTALL_PATH}/bin/lib/selector_runner/selectors")

install(TARGETS nsga2
        DESTINATION "${CUSTOM_NSGA2_LIBRARY_PATH}")

# Include all new selectors here
set(SELECTORS_SOURCE_FILES selector_all.cpp
                           selector_ga.cpp)

# Set RPATH for the shared libraries in this directory
set(CMAKE_INSTALL_RPATH "${INSTALL_PATH}/bin/lib/selector_runner/selectors")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Compile each selector as a shared object
foreach(src_file ${SELECTORS_SOURCE_FILES})
  get_filename_component(src_name ${src_file} NAME_WE)
  add_library(${src_name} MODULE ${src_file})
  target_link_libraries(${src_name} PRIVATE
    ${CUSTOM_QDMI_LIBRARY_PATH}/libQDMI.so
    ${CUSTOM_FOMAC_LIBRARY_PATH}/libFoMaC.so
    ${CUSTOM_BACKENDS_LIBRARY_PATH}/libBackends.so
    ${CUSTOM_NSGA2_LIBRARY_PATH}/libnsga2.so)

  set_target_properties(
    ${src_name}
    PROPERTIES
      INSTALL_RPATH
      "${CMAKE_INSTALL_PREFIX}/bin/lib/backends:${CMAKE_INSTALL_PREFIX}/bin/lib/fomac:${CMAKE_INSTALL_PREFIX}/bin/lib/qdmi"
      INSTALL_RPATH_USE_LINK_PATH TRUE)

  # Set include directories for the library
  target_include_directories(${src_name} PRIVATE ${CUSTOM_QDMI_INCLUDE_PATH}
                                                 ${CUSTOM_FOMAC_INCLUDE_PATH}
                                                 ${CUSTOM_BACKENDS_INCLUDE_PATH})

  #link_directories(${CMAKE_INSTALL_PREFIX}/bin/lib/passes)
  #file(GLOB PASSES_TO_LINK ${CMAKE_INSTALL_PREFIX}/bin/lib/passes/*.so)
  #target_link_libraries(${src_name} PRIVATE ${PASSES_TO_LINK})

  install(TARGETS ${src_name}
          DESTINATION "${INSTALL_PATH}/bin/lib/selector_runner/selectors")
endforeach()

#target_link_libraries(selector_ga PRIVATE nsga2)
