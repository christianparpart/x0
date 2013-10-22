/* <x0/sysconfig.h.cmake>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 *
 * (c) 2009-2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_sysconfig_h
#define sw_x0_sysconfig_h (1)

#cmakedefine HAVE_SYS_INOTIFY_H
#cmakedefine HAVE_SYS_SENDFILE_H
#cmakedefine HAVE_SYS_RESOURCE_H
#cmakedefine HAVE_SYS_LIMITS_H
#cmakedefine HAVE_SYS_MMAN_H
#cmakedefine HAVE_SYSLOG_H
#cmakedefine HAVE_PWD_H

#cmakedefine HAVE_NETDB_H
#cmakedefine HAVE_AIO_H
#cmakedefine HAVE_LIBAIO_H
#cmakedefine HAVE_ZLIB_H
#cmakedefine HAVE_BZLIB_H
#cmakedefine HAVE_GNUTLS_H
#cmakedefine HAVE_LUA_H
#cmakedefine HAVE_PCRE_H
#cmakedefine HAVE_SYS_UTSNAME_H
#cmakedefine HAVE_SECURITY_PAM_APPL_H

#cmakedefine HAVE_LDAP_H

#cmakedefine ENABLE_PLUGIN_AUTH_PAM

#cmakedefine ENABLE_INOTIFY
#cmakedefine ENABLE_PCRE
#cmakedefine ENABLE_ACCEPT4
#cmakedefine ENABLE_TCP_DEFER_ACCEPT
#cmakedefine ENABLE_MULTI_ACCEPT
#cmakedefine ENABLE_OPPORTUNISTIC_WRITE

#cmakedefine HAVE_INOTIFY_CREATE1
#cmakedefine HAVE_FORK
#cmakedefine HAVE_CHROOT
#cmakedefine HAVE_PATHCONF
#cmakedefine HAVE_SENDFILE
#cmakedefine HAVE_POSIX_FADVISE
#cmakedefine HAVE_READAHEAD
#cmakedefine HAVE_ACCEPT4
#cmakedefine HAVE_PTHREAD_SETNAME_NP
#cmakedefine HAVE_PTHREAD_SETAFFINITY_NP

#cmakedefine BUILD_STATIC

#cmakedefine SYSCONFDIR "@SYSCONFDIR@"
#cmakedefine PLUGINDIR "@PLUGINDIR@"

#cmakedefine VALGRIND 1

/* x0 features */
#cmakedefine X0_WORKER_RR 1                /* use RR for worker-select instead of lowest-load */
#cmakedefine X0_DIRECTOR_CACHE

#cmakedefine LLVM_VERSION_3_0
#cmakedefine LLVM_VERSION_3_1
#cmakedefine LLVM_VERSION_3_2
#cmakedefine LLVM_VERSION_3_3

#endif
