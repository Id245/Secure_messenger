cmake_minimum_required(VERSION 3.10)
project(SecureMessenger)

# Set policy for DOWNLOAD_EXTRACT_TIMESTAMP
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)

# Find Boost
set(BOOST_ROOT "/opt/homebrew")
set(BOOST_INCLUDEDIR "/opt/homebrew/include")
set(BOOST_LIBRARYDIR "/opt/homebrew/lib")
find_package(Boost REQUIRED COMPONENTS system)

# Find OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Download JSON header
file(DOWNLOAD
  "https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/json.hpp"
  SHOW_PROGRESS
)

# Make sure common directory exists
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/common)

# Add common headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

# Server executable
add_executable(server server/main.cpp)
target_link_libraries(server
  Boost::system
  ${OPENSSL_LIBRARIES}
  pthread
)

# Client executable
add_executable(client client/main.cpp)
target_link_libraries(client
  Boost::system
  ${OPENSSL_LIBRARIES}
  pthread
)
# ───────── Tests ─────────
include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

enable_testing()

add_executable(unit_tests
    tests/test_all.cpp
)

target_link_libraries(unit_tests PRIVATE doctest::doctest)

add_test(NAME SecureMessengerTests COMMAND unit_tests)
