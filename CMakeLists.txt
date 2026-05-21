cmake_minimum_required(VERSION 3.18)

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
    target_compile_options(${target} PRIVATE -acc=host)
  elseif(mode STREQUAL "multicore")
    target_compile_options(${target} PRIVATE -acc=multicore)
  elseif(mode STREQUAL "gpu")
    target_compile_options(${target} PRIVATE -acc=gpu)
  else()
    message(FATAL_ERROR "Unknown OpenACC mode: ${mode}")
  endif()

  # Optional: keep compiler feedback handy.
  target_compile_options(${target} PRIVATE -Minfo=accel)
endfunction()

add_executable(cpu_onecore eq.cpp)
link_common(cpu_onecore)
add_openacc_flags(cpu_onecore host)

add_executable(cpu_multicore eq.cpp)
link_common(cpu_multicore)
add_openacc_flags(cpu_multicore multicore)

add_executable(gpu1 gpu1.cpp)
link_common(gpu1)
add_openacc_flags(gpu1 gpu)

add_executable(gpu2 gpu2.cpp)
link_common(gpu2)
add_openacc_flags(gpu2 gpu)

add_executable(gpu3 gpu3.cpp)
link_common(gpu3)
add_openacc_flags(gpu3 gpu)

# Prefer pgc++ when available; user requested using pgc++/pgc for builds.
if(DEFINED ENV{PGCXX})
  set(PGCXX $ENV{PGCXX})
else()
  find_program(PGCXX pgc++)
endif()
if(PGCXX)
  message(STATUS "Using pgc++ for targets: ${PGCXX}")
  set(CMAKE_CXX_COMPILER ${PGCXX} CACHE STRING "PGC++ compiler" FORCE)
endif()
