cmake_minimum_required(VERSION 3.3)
project(ccnx_perf)

set(CCNX_HOME $ENV{CCNX_HOME})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CCNX_LIBRARIES longbow parc ccnx_common ccnx_api_portal rta ccnx_api_control ccnx_api_notify)

set(CCNX_PERF_CLIENT_SOURCE_FILES
        src/ccnxPerf_Main.c)

set(CCNX_PERF_SERVER_SOURCE_FILES
        src/ccnxPerfServer_Main.c)

include_directories(${CCNX_HOME}/include)
include_directories(src)

link_directories(${CCNX_HOME}/lib)

add_executable(ccnx_perf ${CCNX_PERF_CLIENT_SOURCE_FILES})
add_executable(ccnx_perf_server ${CCNX_PERF_SERVER_SOURCE_FILES})

target_link_libraries(ccnx_perf ${CCNX_LIBRARIES})
target_link_libraries(ccnx_perf_server ${CCNX_LIBRARIES})
