# 1. Set the minimum required CMake version
cmake_minimum_required(VERSION 3.10)

# 2. Name your project
project(AdiLang)

# 3. Set the C++ standard (e.g., C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 4. Tell CMake which source files to compile into your executable
add_executable(adilang
    src/main.cpp
    src/compiler.cpp
    src/lexer.cpp
    src/parser.cpp

)