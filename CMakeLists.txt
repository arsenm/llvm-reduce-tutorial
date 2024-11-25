# mkdir build; cd build_
# cmake .. -G Ninja -DCMAKE_PREFIX_PATH=$HOME/src/llvm-project/build_rel_with_debinfo -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake_minimum_required(VERSION 3.20.0)
set(CMAKE_CXX_STANDARD 17)
project(BuggyPass)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang CONFIG)

include(AddLLVM)

add_llvm_pass_plugin(buggy_plugin buggy_plugin.cpp LINK_COMPONENTS Core)

target_include_directories(buggy_plugin PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(buggy_plugin PRIVATE ${LLVM_DEFINITIONS})

process_llvm_pass_plugins(buggy_plugin)

file(GENERATE OUTPUT buggy_opt
     INPUT ${CMAKE_CURRENT_SOURCE_DIR}/buggy_opt.in
     FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                      GROUP_READ GROUP_EXECUTE
                      WORLD_READ WORLD_EXECUTE)

if(Clang_FOUND)
  file(GENERATE OUTPUT buggy_clang
       INPUT ${CMAKE_CURRENT_SOURCE_DIR}/buggy_clang.in
       FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE
                        WORLD_READ WORLD_EXECUTE)
endif()
