cmake_minimum_required(VERSION 3.16)
project(bvcs VERSION 0.1.0 LANGUAGES CXX)

option(ENABLE_COVERAGE "Enable compiler coverage instrumentation" OFF)

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

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bvcs PRIVATE -O0 -g --coverage)
        target_link_options(bvcs PRIVATE --coverage)
    else()
        message(WARNING "Coverage requested, but compiler '${CMAKE_CXX_COMPILER_ID}' is not supported by this setup.")
    endif()
endif()

# tests

enable_testing()

# Helper function so each test file only needs one line to register.
# Usage: add_bvcs_test(test_hasher) will compile tests/test_hasher.cpp
function(add_bvcs_test test_name)
    add_executable(${test_name}
        tests/${test_name}.cpp
        ${BVCS_SOURCES}
    )
    target_include_directories(${test_name} PRIVATE storageHandling)
    target_link_libraries(${test_name} PRIVATE OpenSSL::Crypto)

    if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${test_name} PRIVATE -O0 -g --coverage)
        target_link_options(${test_name} PRIVATE --coverage)
    endif()

    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_bvcs_test(test_hasher)
add_bvcs_test(test_utils)
add_bvcs_test(test_object_store)
add_bvcs_test(test_commit)
add_bvcs_test(test_index) 
add_bvcs_test(test_repository)

add_executable(test_main tests/test_main.cpp)
target_include_directories(test_main PRIVATE src)

if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_main PRIVATE -O0 -g --coverage)
    target_link_options(test_main PRIVATE --coverage)
endif()

# Pass the path to the bvcs binary automatically at test time
add_test(
    NAME test_main
    COMMAND test_main $<TARGET_FILE:bvcs>
)

if(ENABLE_COVERAGE)
    find_program(GCOVR_EXECUTABLE gcovr)

    if(GCOVR_EXECUTABLE)
        add_custom_target(coverage
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ${GCOVR_EXECUTABLE}
                    -r ${CMAKE_SOURCE_DIR}
                    --html
                    --html-details
                    --output ${CMAKE_BINARY_DIR}/coverage/index.html
                    --exclude ${CMAKE_SOURCE_DIR}/tests/.*
                    --exclude ${CMAKE_BINARY_DIR}/.*
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run tests and generate HTML coverage report at build/coverage/index.html"
            VERBATIM
        )
    else()
        message(WARNING "ENABLE_COVERAGE is ON but 'gcovr' was not found. The coverage target will print install instructions.")
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E echo "gcovr is not installed."
            COMMAND ${CMAKE_COMMAND} -E echo "Install it with: pip install gcovr"
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "Coverage target requires gcovr"
            VERBATIM
        )
    endif()
endif()