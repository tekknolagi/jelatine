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

/** \file java_lang_String.h
 * Java Class 'java.lang.String' declaration */

/** \def JELATINE_JAVA_LANG_STRING_H
 * java_lang_String.h inclusion macro */

#ifndef JELATINE_JAVA_LANG_STRING_H
#   define JELATINE_JAVA_LANG_STRING_H (1)

#include "wrappers.h"

#include "array.h"

/** java.lang.String instance layout
 *
 * Notice that the 'next' pointer is in the non-reference area of the object,
 * this is due to the fact that it is handled explicitely by the garbage
 * collector */

struct java_lang_String_t {
    array_t *value; ///< Chars of the string

    header_t header; ///< Embedded header
    struct java_lang_String_t *next; ///< Next string in the bucket
#if SIZEOF_VOID_P == 2
    uint16_t padding; ///< Padding needed on machines with 16-bit pointers
#endif // SIZEOF_VOID_P == 2
    uint32_t count; ///< Number of characters
    uint32_t cachedHashCode; ///< Cached hash-code
    uint32_t offset; ///< Start offset inside the value array
};

/** Typedef for ::struct java_lang_String_t */
typedef struct java_lang_String_t java_lang_String_t;

/** Precomputed offset of the 'value' field of a java.lang.String instance */
#define JAVA_LANG_STRING_VALUE_OFFSET \
        ((ptrdiff_t) offsetof(java_lang_String_t, value) \
         - (ptrdiff_t) offsetof(java_lang_String_t, header))

/** Precomputed offset of the 'next' field of a java.lang.String instance */
#define JAVA_LANG_STRING_NEXT_OFFSET \
        (offsetof(java_lang_String_t, next) \
         - offsetof(java_lang_String_t, header))

/** Precomputed offset of the 'count' field of a java.lang.String instance */
#define JAVA_LANG_STRING_COUNT_OFFSET \
        (offsetof(java_lang_String_t, count) \
         - offsetof(java_lang_String_t, header))

/** Precomputed offset of the 'cachedHashCode' field of a java.lang.String
 * instance */
#define JAVA_LANG_STRING_CACHEDHASHCODE_OFFSET \
        (offsetof(java_lang_String_t, cachedHashCode) \
         - offsetof(java_lang_String_t, header))

/** Precomputed offset of the 'offset' field of a java.lang.String instance */
#define JAVA_LANG_STRING_OFFSET_OFFSET \
        (offsetof(java_lang_String_t, offset) \
         - offsetof(java_lang_String_t, header))

/** Number of references in a java.lang.String instance */
#define JAVA_LANG_STRING_REF_N (1)

/** Size in words of the non-reference area of a java.lang.String instance */
#define JAVA_LANG_STRING_NREF_WORDS \
        ((sizeof(java_lang_String_t) - offsetof(java_lang_String_t, next) \
          + sizeof(jword_t) - 1) \
         / sizeof(jword_t))

/** Turns a reference to a Java string into a C pointer */
#define JAVA_LANG_STRING_REF2PTR(r) \
        ((java_lang_String_t *) ((r) - offsetof(java_lang_String_t, header)))

/** Turns a C pointer to a Java string into a Java reference */
#define JAVA_LANG_STRING_PTR2REF(p) \
        ((uintptr_t) (p) + offsetof(java_lang_String_t, header))

#endif // !JELATINE_JAVA_LANG_STRING_H
