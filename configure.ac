AC_PREREQ([2.69])
AC_INIT([x0], [0.11.0-dev], [christian@parpart.family], [], [https://github.com/christianparpart/x0])
dnl AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([configure.ac])
dnl AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects serial-tests])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

AC_LANG_CPLUSPLUS
AC_PROG_RANLIB
AC_PROG_CC
#AC_PROG_CC_FOR_BUILD
AC_PROG_CXX
#AC_PROG_CXX_FOR_BUILD
#AC_PROC_LIBTOOL
AM_PROG_AR
AM_PROG_AS
#LT_INIT
PKG_PROG_PKG_CONFIG([0.25])

uname_system=`uname -s`
AC_MSG_NOTICE("Operating system: $uname_system")
AM_CONDITIONAL([PLATFORM_DARWIN], [test x$uname_system = xDarwin])
AM_CONDITIONAL([PLATFORM_LINUX], [test x$uname_system = xLinux])
AM_CONDITIONAL([PLATFORM_WSL], [uname -r | grep -q Microsoft])

AC_HEADER_STDC

AC_CHECK_HEADERS([sys/sendfile.h])
AC_CHECK_HEADERS([sys/resource.h])
AC_CHECK_HEADERS([sys/inotify.h])
AC_CHECK_HEADERS([sys/limits.h])
AC_CHECK_HEADERS([sys/mman.h])
AC_CHECK_HEADERS([sys/utsname.h])
AC_CHECK_HEADERS([security/pam_appl.h])
AC_CHECK_HEADERS([bzlib.h])
AC_CHECK_HEADERS([dlfcn.h])
AC_CHECK_HEADERS([execinfo.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([pwd.h])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([signal.h])
AC_CHECK_HEADERS([syslog.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([zlib.h])

AC_CHECK_DECLS([pthread_setname_np], [], [], [[#include <pthread.h>]])
AC_CHECK_DECLS([pthread_setaffinity_np], [], [], [[#include <pthread.h>]])

AC_CHECK_FUNCS([inotify_init1])
AC_CHECK_FUNCS([chroot])
AC_CHECK_FUNCS([pathconf])
AC_CHECK_FUNCS([sendfile])
AC_CHECK_FUNCS([posix_fadvise])
AC_CHECK_FUNCS([readahead])
AC_CHECK_FUNCS([pread])
AC_CHECK_FUNCS([nanosleep])
AC_CHECK_FUNCS([daemon])
AC_CHECK_FUNCS([sysconf])
AC_CHECK_FUNCS([pipe2])
AC_CHECK_FUNCS([dup2])
AC_CHECK_FUNCS([fork])
AC_CHECK_FUNCS([backtrace])
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_FUNCS([mkostemps])
AC_CHECK_FUNCS([backtrace])

# ----------------------------------------------------------------------------
# libxzero configuration

AC_ARG_WITH([clusterdir],
            AS_HELP_STRING([--with-clusterdir],
                           [Path to HTTP cluster config directory]))
AS_IF([test "x$with_clusterdir" = "x"],
      [XZERO_CLUSTERDIR="$localstatedir/cluster"],
      [XZERO_CLUSTERDIR="$with_clusterdir"])
AC_SUBST([XZERO_CLUSTERDIR])

# ----------------------------------------------------------------------------
# x0d configuration

AC_ARG_WITH([configfile],
            AS_HELP_STRING([--with-configfile],
                           [Path to x0d.conf file]))
AS_IF([test "x$with_configfile" = "x"],
      [X0D_CONFIGFILE="$sysconfdir/cluster"],
      [X0D_CONFIGFILE="$with_configfile"])
AC_SUBST([X0D_CONFIGFILE])

AC_ARG_WITH([logdir],
            AS_HELP_STRING([--logdir],
                           [x0d log dir]))
AS_IF([test "x$with_logdir" = "x"],
      [X0D_LOGDIR="$localstatedir/log"],
      [X0D_LOGDIR="$with_logdir"])
AC_SUBST([X0D_LOGDIR])

AC_ARG_WITH([pidfile], AS_HELP_STRING([--pidfile], [Path to x0d.pid file]))
AS_IF([test "x$with_pidfile" = "x"],
      [X0D_PIDFILE="$localstatedir/run/x0d.pid"],
      [X0D_PIDFILE="$with_pidfile"])
AC_SUBST([X0D_PIDFILE])

# ----------------------------------------------------------------------------
# Conditional Features

AC_ARG_ENABLE([inotify],
              AS_HELP_STRING([--disable-inotify],
                             [Disables inotify support.]))
AS_IF([test "x$enable_inotify" != xno],
      [AC_DEFINE([ENABLE_INOTIFY], [1], [to inotify or not to inotify])])

AC_ARG_ENABLE([accept4],
              AS_HELP_STRING([--disable-accept4],
                             [Disables accept4() support, using accept() + fcntl() instead.]))
AS_IF([test "x$enable_accept4" != xno],
      [AC_DEFINE([ENABLE_ACCEPT4], [1], [Enables use of accept4()])])

AC_ARG_ENABLE([pipe2],
              AS_HELP_STRING([--disable-pipe2],
                             [Disables pipe2() support, using pipe() + fcntl() instead.]))
AS_IF([test "x$enable_pipe2" != xno],
      [AC_DEFINE([ENABLE_PIPE2], [1], [Enables use of pipe2()])])

AC_ARG_ENABLE([multi-accept],
              AS_HELP_STRING([--disable-multi-accept],
                             [Disables multiple-accept() call support.]))
AS_IF([test "x$enable_multi_accept" != xno],
      [AC_DEFINE([ENABLE_MULTI_ACCEPT], [1], [Enables use of multiple accept() calls])])

AC_ARG_ENABLE([pcre],
              AS_HELP_STRING([--disable-pcre],
                             [Disables use of PCRE.]))
AS_IF([test "x$enable_pcre" != xno],
      [AC_DEFINE([ENABLE_PCRE], [1], [Enables use of PCRE library])])
PKG_CHECK_MODULES([LIBPCRE], [libpcre], [HAVE_LIBPCRE=1])
AC_SUBST([HAVE_LIBPCRE])

AC_ARG_ENABLE([tcp-defer-accept],
              AS_HELP_STRING([--disable-tcp-defer-accept],
                             [Disables use of TCP_DEFER_ACCEPT feature.]))
AS_IF([test "x$enable_tcp_defer_accept" != xno],
      [AC_DEFINE([ENABLE_TCP_DEFER_ACCEPT], [1], [Enables use of TCP_DEFER_ACCEPT])])

AC_ARG_ENABLE([o_tmpfile],
              AS_HELP_STRING([--disable-o_tmpfile],
                             [Disables use of O_TMPFILE feature.]))
AS_IF([test "x$enable_o_tmpfile" != xno],
      [AC_DEFINE([ENABLE_O_TMPFILE], [1], [Enables use of O_TMPFILE])])

# ----------------------------------------------------------------------------
# some more dependencies

dnl OpenSSL
# PKG_CHECK_MODULES([OPENSSL], [openssl], [HAVE_OPENSSL=1])
# AC_SUBST([HAVE_OPENSSL])
AS_IF([test "x$uname_system" = xDarwin],
      [OPENSSL_CFLAGS="-I/usr/local/opt/openssl/include"
       OPENSSL_LDADD="/usr/local/opt/openssl/lib/libssl.dylib /usr/local/opt/openssl/lib/libcrypto.dylib"],
      [OPENSSL_CFLAGS=""
       OPENSSL_LDADD="-lssl -lcrypto"])
AC_SUBST(OPENSSL_CFLAGS)
AC_SUBST(OPENSSL_LDADD)

PAM_LDFLAGS="-lpam" # TODO
AC_SUBST(PAM_LDFLAGS)

BZLIB_LIBS="" # TODO
AC_SUBST([BZLIB_LIBS])

PKG_CHECK_MODULES([ZLIB], [zlib], [HAVE_ZLIB=1])
AC_SUBST([HAVE_ZLIB])

####### Platform Flags ########################################################

AC_RUN_IFELSE([
  AC_LANG_SOURCE([[
    #include <utsname.h>
    #include <string.h>
    #include <stdio.h>
    int main() {
      struct utsname uts;
      uname(&uts);
      if (strstr(uts.release, "Microsoft") != NULL) {
        return 0;
      } else {
        return 1;
      }
    }
    ]])],
    [is_wsl=no; AC_MSG_NOTICE([Yay, no WSL])],
    [is_wsl=yes; AC_MSG_NOTICE([Microsoft Windows Subsystem for Linux])]
)
AS_IF([test "x$is_wsl" != xno],
      [AC_DEFINE([XZERO_WSL], [1], [Works around WSL oddities (Windows Subsystem for Linux)])])

AS_IF([test "x$uname_system" = xDarwin],
      [PLATFORM_CXXFLAGS="-stdlib=libc++"]) 
AS_IF([test "x$uname_system" = xLinux],
      [PLATFORM_LDFLAGS="-lrt"]) 
AC_SUBST([PLATFORM_CXXFLAGS])
AC_SUBST([PLATFORM_LDFLAGS])

####### OUTPUT ################################################################

BASE_CXXFLAGS="-DXOPEN_SOURCE=600 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE \
 -D_LARGEFILE_SOURCE -D_LARGE_FILES -Wall -Wextra -std=c++14 \
 -Wno-unused-variable -Wno-unused-parameter \
 $PLATFORM_CXXFLAGS $LIBPCRE_CFLAGS $OPENSSL_CFLAGS"
AC_SUBST([BASE_CXXFLAGS])

BASE_LIBS="$PLATFORM_LDFLAGS $OPENSSL_LDADD $LIBPCRE_LIBS $OPENSSL_LIBS \
 $ZLIB_LIBS $BZLIB_LIBS -lresolv"
AC_SUBST([BASE_LIBS])

AC_CONFIG_HEADERS(src/xzero/sysconfig.h)
AC_CONFIG_FILES(Makefile XzeroBase.pc XzeroFlow.pc XzeroTest.pc)
AC_OUTPUT
