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

install(TARGETS nsga2
        DESTINATION "${INSTALL_PATH}/bin/lib/selector_runner/selectors")

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
  install(TARGETS ${src_name}
          DESTINATION "${INSTALL_PATH}/bin/lib/selector_runner/selectors")
endforeach()

target_link_libraries(selector_ga PRIVATE nsga2)
