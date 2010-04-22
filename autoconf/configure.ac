#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([denyfs], [0.2.2], [erick@openchill.org])
AC_CONFIG_SRCDIR([src/denyfs.c])
AC_CONFIG_HEADERS([autoconf/config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-ldevmapper':
AC_CHECK_LIB([devmapper], [dm_task_create])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_FSEEKO
AC_CHECK_FUNCS([strerror strtol])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
