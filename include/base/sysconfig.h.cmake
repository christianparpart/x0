// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_sysconfig_h
#define sw_x0_sysconfig_h (1)

// enable OS features
#cmakedefine ENABLE_PCRE
#cmakedefine ENABLE_ACCEPT4
#cmakedefine ENABLE_MULTI_ACCEPT
#cmakedefine ENABLE_TCP_DEFER_ACCEPT
#cmakedefine ENABLE_OPPORTUNISTIC_WRITE
#cmakedefine ENABLE_MYSQL

// enable app features
#cmakedefine BASE_QUEUE_LOCKFREE

// OS header files
#cmakedefine SD_FOUND

#cmakedefine HAVE_FCNTL_H
#cmakedefine HAVE_NETDB_H

#cmakedefine HAVE_SYS_UIO_H
#cmakedefine HAVE_SYS_SENDFILE_H
#cmakedefine HAVE_SENDFILE
#cmakedefine HAVE_POSIX_FADVISE
#cmakedefine HAVE_READAHEAD
#cmakedefine HAVE_PIPE2

#cmakedefine HAVE_SYS_RESOURCE_H
#cmakedefine HAVE_SYS_LIMITS_H
#cmakedefine HAVE_SYS_MMAN_H
#cmakedefine HAVE_SYSLOG_H
#cmakedefine HAVE_PWD_H
#cmakedefine HAVE_UUID_UUID_H
#cmakedefine HAVE_SYS_UTSNAME_H
#cmakedefine HAVE_DLFCN_H

#cmakedefine HAVE_FORK
#cmakedefine HAVE_PATHCONF
#cmakedefine HAVE_ACCEPT4
#cmakedefine HAVE_PTHREAD_SETNAME_NP
#cmakedefine HAVE_PTHREAD_SETAFFINITY_NP

#cmakedefine HAVE_ZLIB_H
#cmakedefine HAVE_BZLIB_H

#endif
