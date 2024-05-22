set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set RPATH for the shared libraries in this directory
set(CMAKE_INSTALL_RPATH "${INSTALL_PATH}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime PATHS /home/ubuntu/onnxruntime/build/Linux/Release)

find_path(ONNX_RUNTIME_SESSION_INCLUDE_DIRS onnxruntime_cxx_api.h HINTS /home/ubuntu/onnxruntime/include/onnxruntime/core/session)

#find_library(ONNX_RUNTIME_LIB onnxruntime HINTS /usr/local/lib)

#if(NOT ONNX_RUNTIME_LIB)
#  message(FATAL_ERROR "ONNX library not found")
#endif()

#message(${ONNX_RUNTIME_SESSION_INCLUDE_DIRS})
if(NOT ONNX_RUNTIME_SESSION_INCLUDE_DIRS)
  message(FATAL_ERROR "ONNX library not found")
endif()

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

    eval_patrick.cpp
    predictor.cpp

)

target_include_directories(nsga2 PRIVATE ${CMAKE_INSTALL_PREFIX}/include/pass_runner /home/ubuntu/qrm.git/build/_deps/qdmi-src/include /home/ubuntu/qrm.git/build/_deps/qinfo-src/include "/home/ubuntu/onnxruntime/include")
target_link_directories(nsga2 PRIVATE qdmi ${ONNXRUNTIME_LIBRARY})

install(TARGETS nsga2
    DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_PREFIX}/include" # Install headers too
)

add_library(hillclimb SHARED
    hill_climbing.cpp
    rand.cpp
    metrics.cpp
)

target_include_directories(hillclimb PRIVATE ${CMAKE_INSTALL_PREFIX}/include/pass_runner)
target_link_libraries(hillclimb PRIVATE qdmi)

install(TARGETS hillclimb
    DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_PREFIX}/include"
)

# Include all new selectors here
set(SELECTORS_SOURCE_FILES
    selector_all.cpp
    selector_manual.cpp
    selector_ga.cpp
    selector_hillclimb.cpp
)

# Compile each selector as a shared object
foreach(src_file ${SELECTORS_SOURCE_FILES})
    get_filename_component(src_name ${src_file} NAME_WE) 
    add_library(${src_name} MODULE ${src_file})
    target_link_libraries(${src_name} PRIVATE
        QirPassRunner
        qdmi
        qinfo
        fomac
        nsga2
        hillclimb
        ${ONNXRUNTIME_LIBRARY}
    )

    # Set include directories for the library
    target_include_directories(${src_name} PRIVATE ${CMAKE_INSTALL_PREFIX}/include)

    # Install the shared library
    install(TARGETS ${src_name}
        DESTINATION "${INSTALL_PATH}/lib"
    )
endforeach()
