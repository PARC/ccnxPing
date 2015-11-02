include ../config.mk

# Set the CPP flags, inherited from ../config.mk (see first line)
AM_CPPFLAGS+= $(CCNX_PERF_INC_CPPFLAGS)

bin_PROGRAMS =  ccnx_perf ccnx_perf_server

ccnx_perf_SOURCES = \
	ccnxPerf_Main.c \
	ccnxPerf_About.c

ccnx_perf_LDADD = \
    ${CCNX_PERF_INC_LFLAGS}

ccnx_perf_server_SOURCES = \
	ccnxPerfServer_Main.c \
	ccnxPerfServer_About.c

ccnx_perf_server_LDADD = \
    ${CCNX_PERF_INC_LFLAGS}

ccnx_perf_all_files = \
	${ccnx_perf_SOURCES} \
	${ccnx_perf_server_SOURCES}

# This line generates a list of coverage output files that need to be cleaned up later
ccnx_perf_coverage_files = \
    $(ccnx_perf_all_files:.c=.gcno) \
    $(ccnx_perf_all_files:.c=.gcda)

# This line adds the list of coverage files to the list of files that 'make clean' should delete
CLEANFILES = ${ccnx_perf_coverage_files} \
		 	 ccnxPerf_About.c ccnxPerf_About.h \
			 ccnxPerfServer_About.c ccnxPerfServer_About.h

ccnxPerf_About.c ccnxPerf_About.h: MINI-NOTICE SHORT-NOTICE LONG-NOTICE
	${GENERATE_ABOUT} ccnxPerf "CCNx Performance Tool" ${CCNX_PERF_VERSION} MINI-NOTICE SHORT-NOTICE LONG-NOTICE

ccnxPerfServer_About.c ccnxPerfServer_About.h: MINI-NOTICE SHORT-NOTICE LONG-NOTICE
	${GENERATE_ABOUT} ccnxPerfServer "CCNx Performance Tool Server" ${CCNX_PERF_VERSION} MINI-NOTICE SHORT-NOTICE LONG-NOTICE
