# Set RPATH for the shared libraries in this directory
set(CMAKE_INSTALL_RPATH "${INSTALL_PATH}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_library(nsga2 SHARED
    nsga2.cpp
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
    tourselect.cpp
)

target_include_directories(nsga2 PRIVATE 
    ${CMAKE_INSTALL_PREFIX}/src/selector_runner/selectors/
    ${CMAKE_INSTALL_PREFIX}/include/pass_runner
    ${CMAKE_BINARY_DIR}/_deps/qdmi-src/include #TODO
    ${CMAKE_BINARY_DIR}/_deps/qinfo-src/include #TODO
)
#target_link_directories(nsga2 PRIVATE qdmi)
target_link_libraries(nsga2 PRIVATE qdmi LLVM PassRunner)

install(TARGETS nsga2
    DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_PREFIX}/include" # Install headers too
)

#add_library(hillclimb SHARED
#    hill_climbing.cpp
#    rand.cpp
#    metrics.cpp
#)
#
#target_include_directories(hillclimb PRIVATE ${CMAKE_INSTALL_PREFIX}/include/pass_runner)
#target_link_libraries(hillclimb PRIVATE qdmi)
#
#install(TARGETS hillclimb
#    DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
#    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_PREFIX}/include"
#)

# Include all new selectors here
set(SELECTORS_SOURCE_FILES
#    selector_all.cpp
#    selector_manual.cpp
    selector_ga.cpp
    selector_specific.cpp 
    selector_agnostic.cpp
#    selector_hillclimb.cpp
)

# Compile each selector as a shared object
foreach(src_file ${SELECTORS_SOURCE_FILES})
    get_filename_component(src_name ${src_file} NAME_WE) 
    add_library(${src_name} SHARED ${src_file})
    target_link_libraries(${src_name} PRIVATE
        QirPassRunner
        qdmi
        qinfo
        fomac
        nsga2
#        hillclimb
        LLVM
    )

    # Set include directories for the library
    target_include_directories(${src_name} PRIVATE ${CMAKE_INSTALL_PREFIX}/include)

    # Install the shared library
    install(TARGETS ${src_name}
        DESTINATION "${INSTALL_PATH}/lib"
    )
endforeach()
