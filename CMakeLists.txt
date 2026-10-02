cmake_minimum_required(VERSION 3.16)
project(raytracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(RT_NATIVE "Compile with -march=native (non-portable, fastest)" ON)

find_package(Threads REQUIRED)

add_executable(raytracer src/main.cpp)
target_link_libraries(raytracer PRIVATE Threads::Threads)

if(MSVC)
  target_compile_options(raytracer PRIVATE /O2 /W4)
else()
  target_compile_options(raytracer PRIVATE -O3 -Wall -Wextra)
  if(RT_NATIVE)
    target_compile_options(raytracer PRIVATE -march=native)
  endif()
endif()
