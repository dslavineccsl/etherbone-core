dnl Set these variables before each release:
m4_define(MAJOR,1)    dnl Increment if removed/changed public symbols since previous release
m4_define(MINOR,2)    dnl Increment if added public symbols; reset to 0 if MAJOR changed
m4_define(REVISION,2) dnl Increment on each release; reset to 0 if MAJOR/MINOR changed
m4_define(SONAME,1)   dnl Whenever MAJOR is incremented, add MINOR+1 to this variable

AC_PREREQ(2.63)

AC_INIT([etherbone], MAJOR.MINOR.REVISION)
AC_CONFIG_SRCDIR([glue/socket.c])
AC_CONFIG_MACRO_DIR([auto])
AC_CONFIG_AUX_DIR([auto])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([subdir-objects dist-xz no-dist-gzip])
AM_MAINTAINER_MODE

AC_PROG_LIBTOOL
AC_PROG_INSTALL

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CC_C99

AC_MSG_CHECKING(whether compiler understands -Wall)
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall -Wmissing-declarations -Wmissing-prototypes"
AC_TRY_COMPILE([],[],
  AC_MSG_RESULT(yes),
  AC_MSG_RESULT(no)
  CFLAGS="$old_CFLAGS")

AC_CACHE_CHECK([for lsb_release supporting -d -s], [ac_cv_path_LSB_RELEASE_CMD],
 [AC_PATH_PROGS_FEATURE_CHECK([LSB_RELEASE_CMD], [lsb_release],
  [[$ac_path_LSB_RELEASE_CMD -d -s > /dev/null && ac_cv_path_LSB_RELEASE_CMD=$ac_path_LSB_RELEASE_CMD]],
  [AC_MSG_ERROR([could not find acceptable lsb_release])])])
AC_CACHE_CHECK([for operating system], [ac_cv_OPERATING_SYSTEM],
  [os="$($ac_cv_path_LSB_RELEASE_CMD -d -s)"
   os="${os%\"}"
   ac_cv_OPERATING_SYSTEM="${os#\"}"])

AC_DEFINE_UNQUOTED([OPERATING_SYSTEM], ["$ac_cv_OPERATING_SYSTEM"], [The operating system of the build system])
AC_DEFINE_UNQUOTED([HOSTNAME], ["${HOSTNAME:-unknown}"], [The hostname of the build system])
AC_DEFINE_UNQUOTED([USERNAME], ["${USERNAME:-${LOGNAME:-${USER:-unknown}}}"], [The username of the person compiling saftlib])

AC_PATH_PROGS([DOCBOOK2MAN], [docbook2man])
AC_PATH_PROGS([GIT], [git])
AM_CONDITIONAL([REBUILD_MAN_PAGES], [test x$DOCBOOK2MAN != x])
AM_CONDITIONAL([GIT_TREE], [test x$GIT != x -a -e ../.git])
if test -e ../.git -a "x$DOCBOOK2MAN" = "x"; then
  AC_MSG_ERROR([Building from a git checkout requires docbook2man])
fi

AC_SUBST([ETHERBONE_MAJOR], MAJOR)
AC_SUBST([ETHERBONE_MINOR], MINOR)
AC_SUBST([ETHERBONE_REVISION], REVISION)
AC_SUBST([ETHERBONE_API], [$((SONAME+MINOR))]) dnl API incremented on every change
AC_SUBST([ETHERBONE_SONAME], SONAME)

AC_CONFIG_FILES([Makefile etherbone.pc])
AC_OUTPUT
