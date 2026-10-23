cmake_minimum_required(VERSION 3.20)
project(DMM6500_Console LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# NI-VISA paths for the current MSVC / Windows setup.
set(VISA_INCLUDE_DIR "C:/Program Files/IVI Foundation/VISA/Win64/Include"
    CACHE PATH "NI-VISA include directory")
set(VISA_LIBRARY "C:/Program Files/IVI Foundation/VISA/Win64/Lib_x64/msc/visa64.lib"
    CACHE FILEPATH "NI-VISA MSVC library")

if(NOT EXISTS "${VISA_INCLUDE_DIR}/visa.h")
    message(FATAL_ERROR "visa.h not found. Set VISA_INCLUDE_DIR to the NI-VISA Include directory.")
endif()

if(NOT EXISTS "${VISA_LIBRARY}")
    message(FATAL_ERROR "visa64.lib not found. Set VISA_LIBRARY to the NI-VISA MSVC library.")
endif()

add_executable(DMM6500_Console
    src/main.cpp
    src/DMMFactory.cpp
    src/Keithley6500Driver.cpp
    src/LiveJsonPublisher.cpp
)

target_include_directories(DMM6500_Console PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${VISA_INCLUDE_DIR}"
)

target_link_libraries(DMM6500_Console PRIVATE "${VISA_LIBRARY}")

if(MSVC)
    target_compile_options(DMM6500_Console PRIVATE /W4 /permissive-)
endif()
