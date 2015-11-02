####################################################
#
# config.mk
#
# This file contains general definitions and variables for the Makefile.am's
# Note that automake will include this file in the Makefile.in's, it will NOT
# be pulled at make or configure time. It will be done at automake time (or
# autoreconf).  So, changes to this file will not affect the build process
# unless you run automake.
#
##############

LIBRT=@LIBRT@

ROOT_INCLUDE = $(abs_top_srcdir)
ROOT_INCLUDE_FLAG = -I$(abs_top_srcdir)

LIBCCNX_LIB_FLAG  = @LIBCCNX_LIB@ -lccnx_api_portal -lrta -lccnx_api_control -lccnx_api_notify -lccnx_common
LIBPARC_LIB_FLAG  = @LIBPARC_LIB@ -lparc -lcrypto -lm
LONGBOW_LIB_FLAG  = @LONGBOW_LIB@ -llongbow -llongbow-ansiterm

AM_CPPFLAGS= -pipe --std=c99 @DEBUG_FLAG@ @DEBUG_CPPFLAGS@ 
AM_LDFLAGS= 

CCNX_PERF_INC_CPPFLAGS = 
CCNX_PERF_INC_CPPFLAGS+= $(ROOT_INCLUDE_FLAG) 
CCNX_PERF_INC_CPPFLAGS+= @LIBCCNX_INCLUDE@
CCNX_PERF_INC_CPPFLAGS+= @LONGBOW_INCLUDE@
CCNX_PERF_INC_CPPFLAGS+= @LIBPARC_INCLUDE@
CCNX_PERF_INC_CPPFLAGS+= -I..

CCNX_PERF_INC_LFLAGS = 
CCNX_PERF_INC_LFLAGS+= $(LIBCCNX_LIB_FLAG)
CCNX_PERF_INC_LFLAGS+= $(LIBPARC_LIB_FLAG)
CCNX_PERF_INC_LFLAGS+= $(LONGBOW_LIB_FLAG)
CCNX_PERF_INC_LFLAGS+= -lcrypto
CCNX_PERF_INC_LFLAGS+= -lm
CCNX_PERF_INC_LFLAGS+= -lpthread

# These occur only in unit tests
CCNX_PERF_TEST_C_FLAGS=--coverage
CCNX_PERF_TEST_L_FLAGS=--coverage -rdynamic -static

# These occur only in executables tests
CCNX_PERF_EXE_C_FLAGS=
CCNX_PERF_EXE_L_FLAGS=-rdynamic -static

DOXYGEN_BIN=@DOXYGEN_BIN@
LONGBOW_DOXYGEN_BIN_REPORT=@LONGBOW_DOXYGEN_BIN_REPORT@
CCNX_PERF_VERSION=`cat ${ROOT_INCLUDE}/VERSION`
GENERATE_ABOUT=@GENERATE_ABOUT_BIN@
