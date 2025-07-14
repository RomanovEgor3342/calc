cmake_minimum_required(VERSION 3.10)
project(TcpCalculatorServer)

set(CMAKE_CXX_STANDARD 17)

add_executable(server
    main.cpp
    calc.cpp
)

add_executable(client
    client.cpp
    calc.cpp
)