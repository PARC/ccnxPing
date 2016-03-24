cmake_minimum_required(VERSION 3.3)
project(ccnxPerf)

include( CTest )

set(CCNX_HOME $ENV{CCNX_HOME})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(CCNX_LIBRARIES longbow longbow-ansiterm parc ccnx_common ccnx_api_portal ccnx_transport_rta ccnx_api_control ccnx_api_notify)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

set(CCNX_PERF_CLIENT_SOURCE_FILES
        ccnxPerf_Client.c
        ccnxPerf_Common.c
        ccnxPerf_Stats.c)

set(CCNX_PERF_SERVER_SOURCE_FILES
        ccnxPerf_Server.c
        ccnxPerf_Common.c)

include_directories(${CCNX_HOME}/include)

link_directories(${CCNX_HOME}/lib)

add_executable(ccnxPerf_Client ${CCNX_PERF_CLIENT_SOURCE_FILES})
target_link_libraries(ccnxPerf_Client ${CCNX_LIBRARIES})
install(TARGETS ccnxPerf_Client RUNTIME DESTINATION bin)

add_executable(ccnxPerf_Server ${CCNX_PERF_SERVER_SOURCE_FILES})
target_link_libraries(ccnxPerf_Server ${CCNX_LIBRARIES})
install(TARGETS ccnxPerf_Server RUNTIME DESTINATION bin)
