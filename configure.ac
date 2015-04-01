#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([alarm-manager], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h:config.hin])

AC_CONFIG_MACRO_DIR([m4])
# Init XO
PLATFORM_INIT

# Checks for programs.
# AC_PROG_CXX
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_GCC_TRADITIONAL
AC_PROG_LIBTOOL

# checks arch
AM_CONDITIONAL(DEVELOP_VER, test $DEVELOP_VER = yes)

#AM_CONDITIONAL([ARCH_IS_ARM], [test "x$ARCH" = "xarm"])
if test "x$ARCH" = "xarm" ; then
	ALARM_CPPFLAGS="-D__ALARM_BOOT"
else
	ALARM_CPPFLAGS="-D__ALARM_BOOT"
fi
		
AC_SUBST(ALARM_CPPFLAGS)

ALARM_CFLAGS="-fvisibility=hidden"
AC_SUBST(ALARM_CFLAGS)

dnl AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")
# Checks for libraries.
PKG_CHECK_MODULES(ALARM_MANAGER, dbus-1 glib-2.0 dbus-glib-1 gobject-2.0 dlog bundle appsvc libtzplatform-config)
AC_SUBST(ALARM_MANAGER_CFLAGS)
AC_SUBST(ALARM_MANAGER_LIBS)

PKG_CHECK_MODULES(ALARM_MANAGER_SERVER, dbus-1 glib-2.0 dbus-glib-1 pmapi dlog heynoti aul bundle db-util appsvc libtzplatform-config)
AC_SUBST(ALARM_MANAGER_SERVER_CFLAGS)
AC_SUBST(ALARM_MANAGER_SERVER_LIBS)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_STRUCT_TM
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
#AC_FUNC_MKTIME
AC_FUNC_STAT
AC_CHECK_FUNCS([localtime_r memset])

AC_CONFIG_FILES([Makefile alarm-service.pc])
AC_OUTPUT
