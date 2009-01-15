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

/** \file jelatine_VMResourceStream.h
 * Java Class 'jelatine.VMResourceStream' declaration */

/** \def JELATINE_JELATINE_VMRESOURCESTREAM_H
 * jelatine_VMResourceStream.h inclusion macro */

#ifndef JELATINE_JELATINE_VMRESOURCESTREAM_H
#   define JELATINE_JELATINE_VMRESOURCESTREAM_H (1)

#if JEL_JARFILE_SUPPORT

#include "wrappers.h"

#if JEL_JARFILE_SUPPORT
#   include <zzip/lib.h>
#endif // JEL_JARFILE_SUPPORT

#include "array.h"
#include "header.h"

/** Structure mirroring an instance of the jelatine.VMResourceStream class */

struct jelatine_VMResourceStream_t {
    uintptr_t resource; ///< A Java String holding the name of the resource
    header_t header; ///< The object header
    ZZIP_FILE *handle; ///< A pointer to the JAR (ZIP) file structure
    int32_t available; ///< Number of bytes available
};

/** Typedef for ::struct jelatine_VMResourceStream */
typedef struct jelatine_VMResourceStream_t jelatine_VMResourceStream_t;

/** Precomputed offset of the 'resource' field of a jelatine.VMResourceStream
 * instance */
#define JELATINE_VMRESOURCESTREAM_RESOURCE_OFFSET \
    ((ptrdiff_t) offsetof(jelatine_VMResourceStream_t, resource) \
    - (ptrdiff_t) offsetof(jelatine_VMResourceStream_t, header))

/** Precomputed offset of the 'handle' field of a jelatine.VMResourceStream
 * instance */
#define JELATINE_VMRESOURCESTREAM_HANDLE_OFFSET \
    ((ptrdiff_t) offsetof(jelatine_VMResourceStream_t, handle) \
    - (ptrdiff_t) offsetof(jelatine_VMResourceStream_t, header))

/** Precomputed offset of the 'available' field of a jelatine.VMResourceStream
 * instance */
#define JELATINE_VMRESOURCESTREAM_AVAILABLE_OFFSET \
    ((ptrdiff_t) offsetof(jelatine_VMResourceStream_t, available) \
    - (ptrdiff_t) offsetof(jelatine_VMResourceStream_t, header))

/** Number of references in a jelatine.VMResourceStream instance */
#define JELATINE_VMRESOURCESTREAM_REF_N (1)

/** Size in bytes of the non-reference are in a jelatine.VMResourceStream
 * instance */
#define JELATINE_VMRESOURCESTREAM_NREF_SIZE \
        (sizeof(jelatine_VMResourceStream_t) \
         - offsetof(jelatine_VMResourceStream_t, handle))

/** Turns a reference to a Java VMResourceStream object into a C pointer */
#define JELATINE_VMRESOURCESTREAM_REF2PTR(r) \
    ((jelatine_VMResourceStream_t *) ((r) \
     - offsetof(jelatine_VMResourceStream_t, header)))

/** Turns a C pointer into a reference to a Java VMResourceStream object */
#define JELATINE_VMRESOURCESTREAM_PTR2REF(p) \
    ((uintptr_t) (p) + offsetof(jelatine_VMResourceStream_t, header))

#endif // JEL_JARFILE_SUPPORT

#endif // !JELATINE_JELATINE_VMRESOURCESTREAM_H
