# mkdir build; cd build_
# cmake .. -G Ninja -DCMAKE_PREFIX_PATH=$HOME/src/llvm-project/build_rel_with_debinfo -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLVM_PROJECT_SRC=$HOME/src/llvm-project

cmake_minimum_required(VERSION 3.20.0)
set(CMAKE_CXX_STANDARD 17)
project(BuggyPass)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang CONFIG)

include(AddLLVM)

option(LLVM_PROJECT_SRC
       "Source directory for llvm-project"
       $ENV{LLVM_PROJECT_SRC})

add_llvm_pass_plugin(buggy_plugin buggy_plugin.cpp LINK_COMPONENTS Core)

target_include_directories(buggy_plugin PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(buggy_plugin PRIVATE ${LLVM_DEFINITIONS})

process_llvm_pass_plugins(buggy_plugin)

set(script_permissions OWNER_READ OWNER_WRITE OWNER_EXECUTE
                       GROUP_READ GROUP_EXECUTE
                        WORLD_READ WORLD_EXECUTE)

file(GENERATE OUTPUT buggy_opt
     INPUT ${CMAKE_CURRENT_SOURCE_DIR}/buggy_opt.in
     FILE_PERMISSIONS ${script_permissions})

if(Clang_FOUND)
  file(GENERATE OUTPUT buggy_clang
       INPUT ${CMAKE_CURRENT_SOURCE_DIR}/buggy_clang.in
       FILE_PERMISSIONS ${script_permissions})
endif()

file(GENERATE OUTPUT interestingness.sh
     INPUT interestingness.sh.in
     FILE_PERMISSIONS ${script_permissions})

file(GENERATE OUTPUT interestingness-O2.sh
     INPUT interestingness-O2.sh.in
     FILE_PERMISSIONS ${script_permissions})

file(GENERATE OUTPUT interestingness-cleanup-passes.sh
     INPUT interestingness-cleanup-passes.sh.in
     FILE_PERMISSIONS ${script_permissions})

file(GENERATE OUTPUT interestingness-multi-crash.sh
     INPUT interestingness-multi-crash.sh.in
     FILE_PERMISSIONS ${script_permissions})

file(GENERATE OUTPUT interestingness-multi-crash-filtered-error-msg.sh
     INPUT interestingness-multi-crash-filtered-error-msg.sh.in
     FILE_PERMISSIONS ${script_permissions})

file(GENERATE OUTPUT interestingness-multi-crash-filtered-error-msg-filecheck.sh
     INPUT interestingness-multi-crash-filtered-error-msg-filecheck.sh.in
     FILE_PERMISSIONS ${script_permissions})

find_program(REDUCE_PIPELINE reduce_pipeline.py PATHS "${LLVM_PROJECT_SRC}/llvm/utils")
find_program(TIME_CMD time)

if(NOT REDUCE_PIPELINE)
  message(WARNING "Did not find reduce_pipeline.py, skipping reduce pipeline example script")
else()
  message(STATUS "Found reduce_pipeline.py: ${REDUCE_PIPELINE}")

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/reduce-pipeline-example.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/reduce-pipeline-example.sh.tmp @ONLY)
  file(GENERATE OUTPUT reduce-pipeline-example.sh
       INPUT ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/reduce-pipeline-example.sh.tmp
       FILE_PERMISSIONS ${script_permissions})
endif()

find_program(TIMEOUT_CMD timeout)


if(NOT REDUCE_PIPELINE)
  message(WARNING "Did not find timeout, skipping hang example interestingness script")
else()
  message(STATUS "Found timeout: ${TIMEOUT_CMD}")

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/interestingness-hang.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/interestingness-hang.sh.tmp @ONLY)
  file(GENERATE OUTPUT interestingness-hang.sh
       INPUT ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/interestingness-hang.sh.tmp
       FILE_PERMISSIONS ${script_permissions})
endif()


if(Clang_FOUND)
  set(MISCOMPILE_AND_RUN_TEST_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/opt-bisect-miscompile-example/test.c)

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/opt-bisect-miscompile-example/miscompile_and_run.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/opt-bisect-miscompile-example/miscompile_and_run.sh.in.tmp @ONLY)

  file(GENERATE
    OUTPUT opt-bisect-miscompile-example/miscompile_and_run.sh
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/opt-bisect-miscompile-example/miscompile_and_run.sh.in.tmp
    FILE_PERMISSIONS ${script_permissions})
endif()

add_subdirectory(reduce-llvm-reduce-introducing-unreachable-blocks)
