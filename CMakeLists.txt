cmake_minimum_required(VERSION 3.3)
project(ccnxPerf)

include( CTest )
#enable_testing()

set(CCNX_HOME $ENV{CCNX_HOME})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(CCNX_LIBRARIES longbow longbow-ansiterm parc ccnx_common ccnx_api_portal ccnx_transport_rta ccnx_api_control ccnx_api_notify)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

set(CCNX_PERF_CLIENT_SOURCE_FILES
        src/ccnxPerf_Main.c)

set(CCNX_PERF_SERVER_SOURCE_FILES
        src/ccnxPerfServer_Main.c)

include_directories(${CCNX_HOME}/include)
include_directories(src)

link_directories(${CCNX_HOME}/lib)

add_executable(ccnxPerf ${CCNX_PERF_CLIENT_SOURCE_FILES})
target_link_libraries(ccnxPerf ${CCNX_LIBRARIES})
install(TARGETS ccnxPerf RUNTIME DESTINATION bin)

add_executable(ccnxPerfServer ${CCNX_PERF_SERVER_SOURCE_FILES})
target_link_libraries(ccnxPerfServer ${CCNX_LIBRARIES})
install(TARGETS ccnxPerfServer RUNTIME DESTINATION bin)
