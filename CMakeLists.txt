cmake_minimum_required(VERSION 3.18)

# Force NVHPC pgc++ compiler path (user requested pgc++ always)
set(CMAKE_CXX_COMPILER "/opt/nvidia/hpc_sdk/Linux_x86_64/23.11/compilers/bin/pgc++" CACHE STRING "PGC++ compiler" FORCE)

project(p_6th LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_OPENACC "Enable OpenACC flags for NVHPC/PGI compilers" ON)

# Boost.Program_options is required by all targets.
find_package(Boost REQUIRED COMPONENTS program_options)

set(IS_NVHPC_COMPILER OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "NVHPC" OR CMAKE_CXX_COMPILER_ID STREQUAL "PGI")
  set(IS_NVHPC_COMPILER ON)
endif()

if(ENABLE_OPENACC AND NOT IS_NVHPC_COMPILER)
  message(WARNING "ENABLE_OPENACC=ON but compiler is '${CMAKE_CXX_COMPILER_ID}'. Building without OpenACC flags.")
  set(ENABLE_OPENACC OFF)
endif()

function(link_common target)
  target_link_libraries(${target} PRIVATE Boost::program_options)
  target_compile_options(${target} PRIVATE -O3)
endfunction()

function(add_openacc_flags target mode)
  if(NOT ENABLE_OPENACC)
    return()
  endif()

  if(mode STREQUAL "host")
    set(_acc_flag -acc=host)
  elseif(mode STREQUAL "multicore")
    set(_acc_flag -acc=multicore)
  elseif(mode STREQUAL "gpu")
    set(_acc_flag -acc=gpu)
  else()
    message(FATAL_ERROR "Unknown OpenACC mode: ${mode}")
  endif()

  # IMPORTANT: NVHPC links OpenACC runtime only if -acc=... is present at link stage.
  target_compile_options(${target} PRIVATE ${_acc_flag} -Minfo=accel)
  target_link_options(${target} PRIVATE ${_acc_flag})
endfunction()

add_executable(cpu_onecore eq.cpp)
link_common(cpu_onecore)
add_openacc_flags(cpu_onecore host)

add_executable(cpu_multicore eq.cpp)
link_common(cpu_multicore)
add_openacc_flags(cpu_multicore multicore)

add_executable(gpu_base eq.cpp)
link_common(gpu_base)
add_openacc_flags(gpu_base gpu)

add_executable(gpu1 gpu1.cpp)
link_common(gpu1)
add_openacc_flags(gpu1 gpu)

add_executable(gpu2 gpu2.cpp)
link_common(gpu2)
add_openacc_flags(gpu2 gpu)

add_executable(gpu3 gpu3.cpp)
link_common(gpu3)
add_openacc_flags(gpu3 gpu)

# Compiler is forced to pgc++ above.
