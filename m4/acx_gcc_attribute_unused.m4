#
# SYNOPSIS
#
#   ACX_GCC_ATTRIBUTE_UNUSED
#
# DESCRIPTION
#
#   Check if the compiler supports gcc's unused variable attribute. Defines
#   HAVE___ATTRIBUTE__UNUSED if it is found
#
# LAST MODIFICATION
#
#   2011-04-28
#
# COPYLEFT
#
#   Copyright © 2008-2011 Gabriele Svelto <gabriele.svelto@gmail.com>
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([ACX_GCC_ATTRIBUTE_UNUSED],
         [AS_VAR_PUSHDEF([ac_var], [acx_cv_gcc_have_attribute_unused])
          acx_gau_cflags="$CFLAGS"
          CFLAGS="$CFLAGS -Werror"
          AC_CACHE_CHECK([for __attribute__((unused))], [ac_var],
                         [AC_LINK_IFELSE([AC_LANG_PROGRAM([
int func(int i __attribute__((unused))) { return 0; }
                                                          ], [])],
                                         [AS_VAR_SET([ac_var], [yes])],
                                         [AS_VAR_SET([ac_var], [no])])])
          AS_IF([test yes = AS_VAR_GET([ac_var])],
                [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE___ATTRIBUTE__UNUSED), 1,
                                    [Define to 1 if the compiler supports the \
                                     `unused' variable attribute])], [])
          CFLAGS="$acx_gau_cflags"
          AS_VAR_POPDEF([ac_var])])
