#
# SYNOPSIS
#
#   ACX_CHECK_ATTRIBUTE(ATTRIBUTE, RETURN, PARAMS, [ATTR_PARAMS])
#
# DESCRIPTION
#
#   Check if the compiler supports one of gcc's function attributes. The
#   attributes may also be provided by other compilers. The ATTRIBUTE parameter
#   holds the name of the required attribute, RETURN is the return value of the
#   function to which the attribute is associated and PARAMS is the parameter
#   list of the said function. The optional ATTR_PARAMS parameter specifies the
#   attribute's parameter list. For example checking for the attribute `malloc'
#   can be done with:
#
#   ACX_CHECK_ATTRIBUTE([malloc], [void *], [void])
#
#   Checking for attribute alias:
#
#   ACX_CHECK_ATTRIBUTE([alias], [int], [void], ["main"])
#
# LAST MODIFICATION
#
#   2011-04-06
#
# COPYLEFT
#
#   Copyright © 2008-2011 Gabriele Svelto <gabriele.svelto@gmail.com>
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([ACX_CHECK_ATTRIBUTE],
         [AS_VAR_PUSHDEF([ac_var], [acx_cv_have_attribute_$1])
          acx_attribute_cflags="$CFLAGS"
          CFLAGS="$CFLAGS -Werror"
          AS_IF([test x$4 = x],
                [acx_attribute_t="extern $2 func($3) __attribute__(($1));"]
                [acx_attribute_t="extern $2 func($3) __attribute__(($1($4)));"])
          AC_CACHE_CHECK([for __attribute__(($1))], [ac_var],
                         [AC_LINK_IFELSE([AC_LANG_PROGRAM([$acx_attribute_t],
                                                          [])],
                                         [AS_VAR_SET([ac_var], [yes])],
                                         [AS_VAR_SET([ac_var], [no])])])
          AS_IF([test yes = AS_VAR_GET([ac_var])],
                [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_ATTRIBUTE_$1), 1,
                                    [Define to 1 if the system has the `$1' \
                                     built-in function])], [])
          CFLAGS="$acx_attribute_cflags"
          AS_VAR_POPDEF([ac_var])])
