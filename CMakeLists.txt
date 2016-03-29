cmake_minimum_required(VERSION 3.3)
project(ccnxPing)

include( CTest )

set(CCNX_HOME $ENV{CCNX_HOME})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(CCNX_LIBRARIES longbow longbow-ansiterm parc ccnx_common ccnx_api_portal ccnx_transport_rta ccnx_api_control ccnx_api_notify)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

set(CCNX_PING_CLIENT_SOURCE_FILES
        ccnxPing_Client.c
        ccnxPing_Common.c
        ccnxPing_Stats.c)

set(CCNX_PING_SERVER_SOURCE_FILES
        ccnxPing_Server.c
        ccnxPing_Common.c)

include_directories(${CCNX_HOME}/include)

link_directories(${CCNX_HOME}/lib)

add_executable(ccnxPing_Client ${CCNX_PING_CLIENT_SOURCE_FILES})
target_link_libraries(ccnxPing_Client ${CCNX_LIBRARIES})
install(TARGETS ccnxPing_Client RUNTIME DESTINATION bin)

add_executable(ccnxPing_Server ${CCNX_PING_SERVER_SOURCE_FILES})
target_link_libraries(ccnxPing_Server ${CCNX_LIBRARIES})
install(TARGETS ccnxPing_Server RUNTIME DESTINATION bin)
