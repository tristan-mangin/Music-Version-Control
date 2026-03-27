cmake_minimum_required(VERSION 3.16)
project(bvcs VERSION 0.1.0 LANGUAGES CXX)

# Require C++17 for std::filesystem
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# find dependencies 

find_package(OpenSSL REQUIRED)

# source files shared between the main executable and tests 

# List every .cpp in src/ except main.cpp so tests can link against the same
# compiled objects without pulling in a second main() entry point
set(BVCS_SOURCES
    storageHandling/hasher.cpp
    storageHandling/object_store.cpp
    storageHandling/commit.cpp
    storageHandling/index.cpp
    storageHandling/repository.cpp
    storageHandling/utils.cpp
)

# main executable 

add_executable(bvcs
    storageHandling/main.cpp
    ${BVCS_SOURCES}
)

target_include_directories(bvcs PRIVATE storageHandling)

target_link_libraries(bvcs PRIVATE
    OpenSSL::Crypto
)

# tests

enable_testing()

# Helper function so each test file only needs one line to register.
# Usage: add_bvcs_test(test_hasher) will compile tests/test_hasher.cpp
function(add_bvcs_test test_name)
    add_executable(${test_name}
        tests/${test_name}.cpp
        ${BVCS_SOURCES}
    )
    target_include_directories(${test_name} PRIVATE src)
    target_link_libraries(${test_name} PRIVATE OpenSSL::Crypto)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_bvcs_test(test_hasher)
add_bvcs_test(test_utils)
add_bvcs_test(test_object_store)
# Add future test files here, e.g.:
# add_bvcs_test(test_commit)
# add_bvcs_test(test_repository)