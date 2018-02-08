ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4
AM_CFLAGS = -std=gnu11 -O3 -g -flto -fuse-linker-plugin -I$(srcdir)/src
AM_LDFLAGS = -static

EXTRA_DIST = \
CHANGES \
LICENSE \
README.rst

SOURCE_FILES = \
src/packet/packet.c \
src/packet/packet_frame.c

HEADER_FILES = \
src/packet/packet.h \
src/packet/packet_frame.h

AUTOMAKE_OPTIONS = subdir-objects
lib_LTLIBRARIES = libpacket.la
libpacket_la_SOURCES = $(SOURCE_FILES) $(HEADER_FILES)

headerfilesdir = $(includedir)/packet
headerfiles_HEADERS = $(HEADER_FILES)

mainheaderdir = $(includedir)
mainheader_HEADERS = src/packet.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpacket.pc

## examples ###

bin_PROGRAMS = \
example/forward \
example/flood \
example/ping

example_forward_LDFLAGS = -lpacket -lreactor -ldynamic
example_flood_LDFLAGS = -lpacket -lreactor -ldynamic
example_ping_LDFLAGS = -lpacket -lreactor -ldynamic
example_ping_SOURCES = example/ping.c example/packet_route.c

MAINTAINERCLEANFILES = aclocal.m4 config.h.in configure Makefile.in libpacket-?.?.?.tar.gz
maintainer-clean-local:; rm -rf autotools m4 libpacket-?.?.?

CLEANFILES = {.,src/packet,src/picohttpparser,test}/*.{gcno,gcda,gcov}
