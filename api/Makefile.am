pkgdir = @libddir@/pkgconfig
ACLOCAL_AMFLAGS = -I auto

include_HEADERS = etherbone.h
lib_LTLIBRARIES = libetherbone.la
bin_PROGRAMS    = tools/eb-read tools/eb-write tools/eb-put tools/eb-get tools/eb-snoop tools/eb-ls tools/eb-find #tools/eb-tunnel tools/eb-discover
EXTRA_PROGRAMS  = test/sizes test/loopback test/etherbonetest
pkg_DATA	= etherbone.pc
EXTRA_DIST	= etherbone.pc.in glue/version.h.in glue/version.h

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
	transport/tunnel.c		\
	glue/cplusplus.cpp

if GIT_TREE
glue/version.h: glue/version.h.in $(SOURCES)
	git log -n1 --pretty="format:%H" . > git.version_full
	git log -n1 --pretty="format:%ci" . > git.date_full
	cut -b1-8 < git.version_full > git.version_short
	date +%Y%m%d -f git.date_full > git.date_short
	sed 's/@EB_VERSION_FULL@/'"`cat git.version_full`"'/;s/@EB_VERSION_SHORT@/'"`cat git.version_short`"'/;s/@EB_DATE_FULL@/'"`cat git.date_full`"'/;s/@EB_DATE_SHORT@/'"`cat git.date_short`"'/' < $< > $@.tmp
	rm -f $@.tmp git.version_full git.date_full git.version_short git.date_short
	mv $@.tmp $@
endif

libetherbone_la_LDFLAGS = -version-info 2:1:0
libetherbone_la_SOURCES = $(SOURCES) glue/version.h
#FLAGS	:= $(FLAGS) -DEB_USE_DYNAMIC    # deterministic untill table overflow (default)
#FLAGS	:= $(FLAGS) -DEB_USE_STATIC=200 # fully deterministic
#FLAGS	:= $(FLAGS) -DEB_USE_MALLOC     # non-deterministic
#FLAGS	:= $(FLAGS) -DDISABLE_SLAVE
#FLAGS	:= $(FLAGS) -DDISABLE_MASTER

LDADD = libetherbone.la

tools_eb_read_SOURCES	= tools/eb-read.c
tools_eb_write_SOURCES	= tools/eb-write.c
tools_eb_put_SOURCES	= tools/eb-put.c
tools_eb_get_SOURCES	= tools/eb-get.c
tools_eb_snoop_SOURCES	= tools/eb-snoop.c
tools_eb_ls_SOURCES	= tools/eb-ls.c
tools_eb_find_SOURCES	= tools/eb-find.c
#tools_eb_tunnel_SOURCES	= tools/eb-tunnel.c
#tools_eb_discover_SOURCES= tools/eb-discover.c

test_sizes_SOURCES	= test/sizes.c
test_loopback_SOURCES	= test/loopback.cpp
test_etherbonetest_SOURCES = test/etherbonetest.cpp
