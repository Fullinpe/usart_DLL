cmake_minimum_required(VERSION 3.14)
project(usart_DLL)

set(CMAKE_CXX_STANDARD 14)

add_library(usart_DLL SHARED  library.cpp SerialPort.cpp)