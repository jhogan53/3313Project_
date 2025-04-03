cmake_minimum_required(VERSION 3.16)
project(auction_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get Homebrew prefix.
execute_process(
  COMMAND brew --prefix
  OUTPUT_VARIABLE BREW_PREFIX
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories(${BREW_PREFIX}/include)
link_directories(${BREW_PREFIX}/lib)

# Include jwt-cpp headers (from server/external/jwt-cpp)
include_directories(${CMAKE_SOURCE_DIR}/external/jwt-cpp/include)

# Find Boost (we need Boost.System for Asio)
find_package(Boost 1.87.0 REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})

# Find libpqxx using pkg-config.
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
include_directories(${PQXX_INCLUDE_DIRS})
link_directories(${PQXX_LIBRARY_DIRS})

# Find nlohmann-json.
find_package(nlohmann_json 3.11.3 REQUIRED)

# Find OpenSSL.
find_package(OpenSSL REQUIRED)

add_executable(auction_server server.cpp)
target_link_libraries(auction_server PRIVATE
    Boost::system
    ${PQXX_LIBRARIES}
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)
