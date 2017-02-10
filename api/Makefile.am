pkgdir = @libdir@/pkgconfig
ACLOCAL_AMFLAGS = -I auto

include_HEADERS = etherbone.h
lib_LTLIBRARIES = libetherbone.la
EXTRA_PROGRAMS  = test/sizes test/loopback test/etherbonetest
pkg_DATA	= etherbone.pc
bin_PROGRAMS    = tools/eb-read      tools/eb-write      tools/eb-put      tools/eb-get      tools/eb-snoop      tools/eb-ls      tools/eb-find      tools/eb-tunnel      tools/eb-discover

if REBUILD_MAN_PAGES
dist_man_MANS   = tools/eb-read.1    tools/eb-write.1    tools/eb-put.1    tools/eb-get.1    tools/eb-snoop.1    tools/eb-ls.1    tools/eb-find.1    tools/eb-tunnel.1    tools/eb-discover.1
EXTRA_DIST      = tools/eb-read.sgml tools/eb-write.sgml tools/eb-put.sgml tools/eb-get.sgml tools/eb-snoop.sgml tools/eb-ls.sgml tools/eb-find.sgml tools/eb-tunnel.sgml tools/eb-discover.sgml \
		  etherbone.pc.in glue/version.c.in
endif

SOURCES =				\
	memory/dynamic.c		\
	memory/static.c			\
	memory/memory.h			\
	memory/memory-array.h		\
	memory/array.c			\
	memory/memory-malloc.h		\
	memory/malloc.c			\
	format/bigendian.h		\
	format/format.h			\
	format/slave.c			\
	format/master.c			\
	glue/cycle.h			\
	glue/cycle.c			\
	glue/device.h			\
	glue/device.c			\
	glue/format.c			\
	glue/handler.h			\
	glue/handler.c			\
	glue/operation.h		\
	glue/operation.c		\
	glue/readwrite.h		\
	glue/readwrite.c		\
	glue/sdb.h			\
	glue/sdb.c			\
	glue/socket.h			\
	glue/socket.c			\
	glue/strncasecmp.h		\
	glue/strncasecmp.c		\
	glue/widths.h			\
	glue/widths.c			\
	transport/dev.h			\
	transport/dev.c			\
	transport/posix-ip.h		\
	transport/posix-ip.c		\
	transport/posix-tcp.h		\
	transport/posix-tcp.c		\
	transport/posix-udp.h		\
	transport/posix-udp.c		\
	transport/run.c			\
	transport/transport.h		\
	transport/transports.c		\
	transport/tunnel.h		\
	transport/tunnel.c

if GIT_TREE
glue/version.c: glue/version.c.in Makefile $(SOURCES)
	git log -n1 --pretty="format:%ci" . > git.date
	date +'%b %e %Y %H:%M:%S' -f git.date > git.date_full
	date +'%Y%m%d'            -f git.date > git.date_short
	git log -n1 --pretty="format:%H" . > git.version
	git describe --always --tags --dirty > git.version_full
	cut -b1-8 < git.version > git.version_short
	sed 's/@EB_VERSION_FULL@/'"`cat git.version_full`"'/;s/@EB_VERSION_SHORT@/'"`cat git.version_short`"'/;s/@EB_DATE_FULL@/'"`cat git.date_full`"'/;s/@EB_DATE_SHORT@/'"`cat git.date_short`"'/' < $< > $@.tmp
	rm -f git.version_full git.date_full git.version_short git.date_short git.version git.date
	mv $@.tmp $@
debian:
	cp debian/libetherbone.install.in debian/libetherbone@ETHERBONE_SONAME@.install
	./config.status --file=debian/control
.PHONY: debian
endif

libetherbone_la_LDFLAGS = -version-info @ETHERBONE_API@:@ETHERBONE_REVISION@:@ETHERBONE_MINOR@ # @ETHERBONE_SONAME@
libetherbone_la_SOURCES = $(SOURCES) glue/version.h glue/version.c

#FLAGS	:= $(FLAGS) -DEB_USE_DYNAMIC    # deterministic until table overflow (default)
#FLAGS	:= $(FLAGS) -DEB_USE_STATIC=200 # fully deterministic
#FLAGS	:= $(FLAGS) -DEB_USE_MALLOC     # non-deterministic

LDADD = libetherbone.la

tools_eb_read_SOURCES	= tools/eb-read.c
tools_eb_write_SOURCES	= tools/eb-write.c
tools_eb_put_SOURCES	= tools/eb-put.c
tools_eb_get_SOURCES	= tools/eb-get.c
tools_eb_snoop_SOURCES	= tools/eb-snoop.c
tools_eb_ls_SOURCES	= tools/eb-ls.c
tools_eb_find_SOURCES	= tools/eb-find.c

tools_eb_tunnel_SOURCES = tools/eb-tunnel.c transport/posix-ip.c transport/posix-udp.c transport/posix-tcp.c glue/strncasecmp.c
tools_eb_tunnel_CFLAGS  = $(AM_CFLAGS)
tools_eb_tunnel_LDADD   =

tools_eb_discover_SOURCES= tools/eb-discover.c transport/posix-ip.c transport/posix-udp.c glue/strncasecmp.c
tools_eb_discover_CFLAGS = $(AM_CFLAGS)
tools_eb_discover_LDADD  =

test_sizes_SOURCES	= test/sizes.c
test_loopback_SOURCES	= test/loopback.cpp
test_etherbonetest_SOURCES = test/etherbonetest.cpp

# Use manpages in distribution tarball if docbook2man not found
if REBUILD_MAN_PAGES
.sgml.1:
	$(DOCBOOK2MAN) -o $(@D) $<
endif
