cmake_minimum_required(VERSION 3.20)
project(HChatBackend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ------------------------------------------------------------------
# Dependency roots
#   MAIN  : main vcpkg tree -> protobuf 6.33.4 / gRPC / hiredis / jsoncpp / spdlog (static)
#   CONN  : manually-built MySQL Connector/C++ JDBC (libmysqlcppconn.so, protobuf-free,
#           libmysqlclient merged in) built from cached source vs apt libmysqlclient 8.0.46
#   BOOST : system Boost 1.83
#   apt   : libmysqlclient-dev (mysql/mysql.h), libssl/libcrypto (runtime deps of the .so)
# ------------------------------------------------------------------
set(MAIN_VCPKG "/home/xxxl/vcpkg_installed/x64-linux")
set(CONN_DIR   "/home/xxxl/mysql_conn")

list(APPEND CMAKE_PREFIX_PATH "${MAIN_VCPKG}")

find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost 1.83 REQUIRED COMPONENTS system filesystem)
find_package(Threads REQUIRED)

message(STATUS "protobuf: ${protobuf_VERSION}   gRPC: ${gRPC_VERSION}   Boost: ${Boost_VERSION_STRING}")

include_directories("${MAIN_VCPKG}/include" "${MAIN_VCPKG}/include/hiredis")

set(HC_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/common")
set(CONN_INC  "${CONN_DIR}/include")   # <jdbc/*> + <mysql/jdbc.h>;  <mysql/mysql.h> from system
set(CONN_LIB  "${CONN_DIR}/lib")

function(add_hchat_service exe srcdir)
  file(GLOB SVC_SRCS CONFIGURE_DEPENDS "${srcdir}/*.cpp" "${srcdir}/*.cc")
  add_executable(${exe} ${SVC_SRCS})
  target_include_directories(${exe} PRIVATE
    "${srcdir}" "${HC_COMMON}" "${CONN_INC}" ${Boost_INCLUDE_DIRS})
  target_link_libraries(${exe} PRIVATE
    protobuf::libprotobuf
    gRPC::grpc++
    hiredis::hiredis
    JsonCpp::JsonCpp
    spdlog::spdlog
    "${CONN_LIB}/libmysqlcppconn.so"
    "/usr/lib/x86_64-linux-gnu/libzstd.so"
    "/usr/lib/x86_64-linux-gnu/liblz4.so"
    Boost::system Boost::filesystem
    Threads::Threads dl)
  set_target_properties(${exe} PROPERTIES
    BUILD_RPATH   "${CONN_LIB}"
    INSTALL_RPATH "${CONN_LIB}")
endfunction()

add_hchat_service(GateServer   "${CMAKE_CURRENT_SOURCE_DIR}/GateServer")
add_hchat_service(StatusServer "${CMAKE_CURRENT_SOURCE_DIR}/StatusServer")
add_hchat_service(ChatServer   "${CMAKE_CURRENT_SOURCE_DIR}/ChatServer")
# ChatServer2 reuses ChatServer sources (Windows uses SharedSources.props); only config.ini differs
add_hchat_service(ChatServer2  "${CMAKE_CURRENT_SOURCE_DIR}/ChatServer")
