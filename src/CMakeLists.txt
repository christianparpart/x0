include(XzeroCommon)
include(CompileMimeTypes)
include(FindPkgConfig)
#include(FindDoxygen)
include(FindOpenSSL)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
#include(CMakeDetermineCCompiler)

include_directories(${XZERO_BASE_INCLUDE_DIRS})
include_directories(${XZERO_FLOW_INCLUDE_DIRS})

# workaround
#include_directories(${PCRE_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

CHECK_INCLUDE_FILES(security/pam_appl.h HAVE_SECURITY_PAM_APPL_H)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sysconfig.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.h)

MIMETYPES_GENERATE_CPP(MT_SRC ${PROJECT_SOURCE_DIR}/3rdparty/mime.types)

set(x0d_SRC
  ${MT_SRC}
  # core
  main.cc
  XzeroDaemon.cc
  XzeroModule.cc
  XzeroWorker.cc
  XzeroContext.cc

  # static modules
  modules/access.cc
  modules/accesslog.cc
  modules/auth.cc
#  modules/cgi.cc
  modules/compress.cc
  modules/core.cc
  modules/dirlisting.cc
  modules/empty_gif.cc
#  modules/proxy.cc
  modules/userdir.cc
)

set(LIBS pam)

add_executable(x0d ${x0d_SRC})
target_link_libraries(x0d xzero-base xzero-flow ${LIBS})

