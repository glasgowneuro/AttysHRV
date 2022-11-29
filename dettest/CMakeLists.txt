cmake_minimum_required(VERSION 3.8.0)
project (ECGDetTest LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wconversion -Wextra -pedantic)

add_executable(test test.cpp ../app/src/main/cpp/ecg_rr_det.cpp)
target_link_libraries(test iir)
