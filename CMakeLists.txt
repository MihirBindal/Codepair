cmake_minimum_required(VERSION 3.12)
project(CodePairBackend VERSION 1.0 LANGUAGES C CXX)

# Force C++17 or C++20 for modern concurrency features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL and ZLIB
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Add uSockets and uWebSockets include paths
include_directories(
    third_party/uWebSockets/src
    third_party/uWebSockets/uSockets/src
)

# Compile uSockets sources directly into our project
file(GLOB USOCKETS_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uWebSockets/uSockets/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uWebSockets/uSockets/src/crypto/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uWebSockets/uSockets/src/crypto/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uWebSockets/uSockets/src/eventing/*.c"
)
message(STATUS "Found uSockets sources: ${USOCKETS_SOURCES}")

# Main Executable
add_executable(codepair_server 
    src/main.cpp 
    src/ThreadPool.cpp
    src/SessionManager.cpp
    src/RedisClient.cpp
    src/CodeExecutor.cpp
    ${USOCKETS_SOURCES}
)

# Set include directories
target_include_directories(codepair_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Set compile definitions
target_compile_definitions(codepair_server PRIVATE LIBUS_USE_OPENSSL)

# Link Required Dependencies (Pthreads, OpenSSL, Zlib, Hiredis)
target_link_libraries(codepair_server 
    PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    ZLIB::ZLIB
    pthread
    hiredis
)
