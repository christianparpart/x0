AM_CXXFLAGS = \
	-DXOPEN_SOURCE=600 -D_FILE_OFFSET_BITS=64 \
	-D_GNU_SOURCE \
	-D_LARGEFILE_SOURCE -D_LARGE_FILES \
	-Wall -Wextra -std=c++11 \
	$(PLATFORM_CXXFLAGS) \
	$(LIBPCRE_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	-I$(top_srcdir)/src -I$(top_builddir)/src

AM_LDFLAGS =  \
	$(PLATFORM_LDFLAGS)
	$(LIBPCRE_LIBS) \
	$(OPENSSL_LIBS) \
	-lz -lbzlib

bin_PROGRAMS = x0d

x0d_LDADD = ../xzero/libxzero.a \
	../xzero-flow/libxzeroflow.a \
	$(OPENSSL_LDADD)

x0d_SOURCES = \
	XzeroContext.cc \
	XzeroContext.h \
	XzeroDaemon.cc \
	XzeroDaemon.h \
	XzeroEventHandler.cc \
	XzeroEventHandler.h \
	XzeroModule.cc \
	XzeroModule.h \
	XzeroState.cc \
	XzeroState.h \
	main.cc
