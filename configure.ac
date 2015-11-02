AC_INIT([metis], m4_esyscmd([tr -d '\n' < BASE_VERSION]), [bugs@ccnx.org])

# Tell autoconf to include the host/build/target triplets
AC_CANONICAL_TARGET
#AC_CANONICAL_SYSTEM
#AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects])
: ${CFLAGS=""}


AC_PROG_CC([clang gcc cc])
AC_GNU_SOURCE
AM_PROG_CC_C_O
AM_PROG_AR
AM_PATH_PYTHON
AM_SILENT_RULES([yes])
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_CONFIG_HEADERS([config.h])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h errno.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MMAP
AC_CHECK_FUNCS([getcwd gettimeofday memset select socket strdup strncasecmp strndup])

AC_MSG_CHECKING(for struct stat has mtimespec)
AC_CACHE_VAL(ac_cv_stat_mtimespec,
[AC_TRY_COMPILE([
#include <sys/types.h>
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#include <sys/stat.h>],[struct stat teststat; teststat.st_mtimespec;],
ac_cv_stat_mtimespec=st_mtimespec)
])

if test -z "$ac_cv_stat_mtimespec" ; then #{
AC_TRY_COMPILE([
#include <sys/types.h>
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#include <sys/stat.h>],[struct stat teststat; teststat.st_mtim;],
ac_cv_stat_mtimespec=st_mtim)
fi #}

if test -z "$ac_cv_stat_mtimespec"; then
ac_cv_stat_mtimespec=no
fi

AC_MSG_RESULT($ac_cv_stat_mtimespec)
if test ! "$ac_cv_stat_mtimespec" = no; then
    AC_DEFINE_UNQUOTED(ST_STAT_MTIMESPEC,$ac_cv_stat_mtimespec,"What do we have for mtimespec in stat struct")
fi

AC_MSG_CHECKING(for dcache line size)
uname_s=`uname -s`
# Let's figure out what platform we're in...
AS_IF([test x$uname_s = xDarwin], 
	  		[ac_cache_line_size=`sysctl -n hw.cachelinesize`],
      [test x$uname_s = xLinux], 
	        [ac_cache_line_size=`getconf LEVEL1_DCACHE_LINESIZE`],
      [ac_cache_line_size="unknown"])

if test "$ac_cache_line_size" = unknown; then
  ac_cache_line_size_result="unknown... setting to 64"
  ac_cache_line_size=64
else
  ac_cache_line_size_result=$ac_cache_line_size
fi

AC_DEFINE_UNQUOTED(LEVEL1_DCACHE_LINESIZE,$ac_cache_line_size,CPU Cache line size)
AC_MSG_RESULT($ac_cache_line_size_result)

AC_PATH_PROG(DOXYGEN_BIN, doxygen, error, [$DEPENDENCY_DIR/bin:$PATH])
AC_PATH_PROG(LONGBOW_DOXYGEN_BIN_REPORT, longbow-doxygen-report, error, [$LONGBOW_HOME/bin:$PATH])


AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

#AC_CONFIG_SUBDIRS([transport/transport_rta/libevent])

AH_TOP([
#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <config_top.h>])

AH_BOTTOM([
#endif])

AC_DEFINE([FAIL_FAST],[1],[Fast Fail])

m4_include(m4/ax_lib_longbow.m4)
AX_LIB_LONGBOW
m4_include(m4/ax_lib_parc.m4)
AX_LIB_PARC
m4_include(m4/ax_lib_ccnx.m4)
AX_LIB_CCNX
m4_include(m4/ax_debug_option.m4)
AX_DEBUG_OPTION("-O3")
m4_include(m4/ax_set_version.m4)
AX_SET_VERSION
m4_include(m4/ax_generate_about.m4)
AX_GENERATE_ABOUT


AC_CHECK_LIB([rt], [clock_gettime], 
           [AC_SUBST(LIBRT,-lrt)],
           [])



case "${target_os}" in
    darwin*) osname="darwin" ;;
    linux*)  osname="linux" ;;
    *)       osname="unknown" ;;
esac

AM_CONDITIONAL([DARWIN], [test "$osname" = "darwin" ] )
AM_CONDITIONAL([LINUX], [test "$osname" = "linux" ] )

AC_OUTPUT
