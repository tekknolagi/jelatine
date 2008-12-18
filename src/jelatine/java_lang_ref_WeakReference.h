/***************************************************************************
 *   Copyright © 2005, 2006, 2007, 2008 by Gabriele Svelto                 *
 *   gabriele.svelto@gmail.com                                             *
 *                                                                         *
 *   This file is part of Jelatine.                                        *
 *                                                                         *
 *   Jelatine is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Jelatine is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Jelatine.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

/** \file java_lang_ref_WeakReference.h
 * Java Class 'java.lang.ref.WeakReference' declaration */

/** \def JELATINE_JAVA_LANG_REF_WEAKREFERENCE_H
 * java_lang_ref_WeakReference.h inclusion macro */

#ifndef JELATINE_JAVA_LANG_REF_WEAKREFERENCE_H
#   define JELATINE_JAVA_LANG_REF_WEAKREFERENCE_H (1)

#include "wrappers.h"

/** java.lang.ref.WeakReference instance layout */

struct java_lang_ref_WeakReference_t {
    header_t header; ///< Parent class instance
    uintptr_t referent; ///< Referent of this weak reference
    struct java_lang_ref_WeakReference_t *next; ///< Next in the list
};

/** Typedef for ::struct java_lang_ref_WeakReference */
typedef struct java_lang_ref_WeakReference_t java_lang_ref_WeakReference_t;

/** Precomputed offset of the 'next' field of a java.lang.ref.WeakReference
 * instance */
#define JAVA_LANG_REF_WEAKREFERENCE_NEXT_OFFSET \
    (offsetof(java_lang_ref_WeakReference_t, next) \
    - offsetof(java_lang_ref_WeakReference_t, header))

/** Number of references of a java.lang.ref.WeakReference instance */
#define JAVA_LANG_REF_WEAKREFERENCE_REF_N (0)

/** Size in words of the non-reference area of a java.lang.ref.WeakReference
 * instance */
#define JAVA_LANG_REF_WEAKREFERENCE_NREF_WORDS \
        ((sizeof(java_lang_ref_WeakReference_t) \
          - offsetof(java_lang_ref_WeakReference_t, referent) \
          + sizeof(jword_t) - 1) \
         / sizeof(jword_t))

/** Turns a reference to a Java WeakReference object into a C pointer */
#define JAVA_LANG_REF_WEAKREFERENCE_REF2PTR(r) \
        ((java_lang_ref_WeakReference_t *) \
         ((r) - offsetof(java_lang_ref_WeakReference_t, header)))

/** Turns a C pointer into a reference to a Java WeakReference object */
#define JAVA_LANG_REF_WEAKREFERENCE_PTR2REF(p) \
        ((uintptr_t) (p) + offsetof(java_lang_ref_WeakReference_t, header))

#endif // !JELATINE_JAVA_LANG_REF_WEAKREFERENCE_H
