// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef xzero_sysconfig_h
#define xzero_sysconfig_h (1)

#cmakedefine LIBXZERO_VERSION "@LIBXZERO_VERSION@"
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

#cmakedefine XZERO_TMPDIR "@X0_TMPDIR@"

#cmakedefine XZERO_ENABLE_O_TMPFILE
#cmakedefine XZERO_ENABLE_POST_FN_OPTIMIZATION

/* libxzero features */
#cmakedefine XZERO_WORKER_POST_LIBEV 1             /* HttpWorker to use libev's ev_async (over locking queue) for its post API */
#cmakedefine XZERO_WORKER_RR 1                     /* use RR for worker-select instead of lowest-load */

#endif
