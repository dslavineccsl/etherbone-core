dnl Set these variables before each release:
m4_define(MAJOR,2)    dnl Increment if removed/changed public symbols since previous release
m4_define(MINOR,1)    dnl Increment if added public symbols; reset to 0 if MAJOR changed
m4_define(REVISION,0) dnl Increment on each release; reset to 0 if MAJOR/MINOR changed
m4_define(SONAME,5)   dnl Whenever MAJOR is incremented, add MINOR+1 to this variable

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

AC_PATH_PROG([UNAME], [uname], [false])
AC_PATH_PROG([LSB_RELEASE], [lsb_release], [false])
AC_PATH_PROG([SW_VERS], [sw_vers], [false])

AC_CACHE_CHECK([for operating system], [ac_cv_OS], [AS_CASE([`${UNAME} -s`], 
  [Linux], [AS_IF(["${LSB_RELEASE}" -d -s 2>/dev/null >/dev/null], 
                  [os=`"${LSB_RELEASE}" -d -s`; os="${os%\"}"; ac_cv_OS="${os#\"}"],
                  [AC_MSG_ERROR([lsb_release is required])])],
  [Darwin], [AS_IF(["${SW_VERS}" 2>/dev/null >/dev/null],
                   [ac_cv_OS="`${SW_VERS} -productName` `${SW_VERS} -productVersion`"],
                   [AC_MSG_ERROR([sw_vers is required])])],
  [ac_cv_OS="`${UNAME} -s`"])])

AC_DEFINE_UNQUOTED([OPERATING_SYSTEM], ["$ac_cv_OS"], [The operating system of the build system])
AC_DEFINE_UNQUOTED([HOSTNAME], ["${HOSTNAME:-unknown}"], [The hostname of the build system])
AC_DEFINE_UNQUOTED([USERNAME], ["${USERNAME:-${LOGNAME:-${USER:-unknown}}}"], [The username of the person compiling saftlib])

AC_PATH_PROGS([DOCBOOK2MAN], [docbook2man])
AC_PATH_PROGS([GIT], [git])
AC_ARG_ENABLE([build-man], 
  [  --enable-build-man      enable man page generation from docbook files],
  [BUILD_MAN=${enableval}], [BUILD_MAN=no])
AM_CONDITIONAL([REBUILD_MAN_PAGES], [test \( x$DOCBOOK2MAN != x \) -a \( x$BUILD_MAN != xno \) ])
AM_CONDITIONAL([GIT_TREE], [test x$GIT != x -a -e ../.git])
if test -e ../.git -a "x$DOCBOOK2MAN" = "x"; then
  AC_MSG_ERROR([Building from a git checkout requires docbook2man])
fi

AC_SUBST([ETHERBONE_MAJOR], MAJOR)
AC_SUBST([ETHERBONE_MINOR], MINOR)
AC_SUBST([ETHERBONE_REVISION], REVISION)
AC_SUBST([ETHERBONE_API], [$((SONAME+MINOR))]) dnl API incremented on every change
AC_SUBST([ETHERBONE_SONAME], SONAME)

AC_DEFINE_UNQUOTED([ETHERBONE_MAJOR],    MAJOR,    [The library major version])
AC_DEFINE_UNQUOTED([ETHERBONE_MINOR],    MINOR,    [The library minor version])
AC_DEFINE_UNQUOTED([ETHERBONE_REVISION], REVISION, [The library revision])
AC_DEFINE_UNQUOTED([ETHERBONE_SONAME],   SONAME,   [The soname number of the built library])

AC_CONFIG_FILES([Makefile etherbone.pc])
AC_OUTPUT
