/***************************************************************************
 *   Copyright © 2005-2009 by Gabriele Svelto                              *
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

/** \file class.h
 * C object mirroring the java.lang.Class class */

/** \def JELATINE_CLASS_H
 * class.h inclusion macro */

#ifndef JELATINE_CLASS_H
#   define JELATINE_CLASS_H (1)

#include "wrappers.h"

#include "classfile.h"
#include "constantpool.h"
#include "field.h"
#include "header.h"
#include "method.h"
#include "thread.h"
#include "util.h"

#include "java_lang_Class.h"

// Forward declarations

struct class_t;

/******************************************************************************
 * Interface manager type definitions                                         *
 ******************************************************************************/

/** Interface manager */

struct interface_manager_t {
    struct class_t **interfaces; ///< The interfaces implementd by the class
    uint32_t entries; ///< The number of interfaces
};

/** Typedef for ::struct interface_manager_t */
typedef struct interface_manager_t interface_manager_t;

/******************************************************************************
 * Interface manager interface                                                *
 ******************************************************************************/

extern interface_manager_t *im_create( void );
extern void im_add(interface_manager_t *, struct class_t *);
extern void im_flatten(interface_manager_t *);
extern bool im_is_present(interface_manager_t *, struct class_t *);

/******************************************************************************
 * Interface iterator                                                         *
 ******************************************************************************/

/** Interface iterator, usually allocated on the stack when used */

struct interface_iterator_t {
    struct class_t **interfaces; ///< Pointer to the implemented interfaces
    size_t entries; ///< Number of interfaces
    size_t index; ///< Current index in the interface array
};

/** Typedef for the ::struct interface_iterator_t type */
typedef struct interface_iterator_t interface_iterator_t;

/** Creates an interface iterator from the interface manager \a im
 * \param im A pointer to a class' interface manager
 * \returns An initialized instance interface iterator */

static inline interface_iterator_t interface_itr(interface_manager_t *im)
{
    interface_iterator_t itr;

    itr.interfaces = im->interfaces;
    itr.entries = im->entries;
    itr.index = 0;

    return itr;
} // interface_itr()

/** Returns true if another interface is available
 * \param iterator An interface iterator
 * \returns true if there are more interfaces, false otherwise */

static inline bool interface_itr_has_next(interface_iterator_t itr)
{
    return itr.index < itr.entries;
} // interface_itr_has_next()

/** Returns the next interface, if the iterator has just been
 * created it returns the data of the first interface (if it is present)
 * \param iterator A pointer to an interface iterator
 * \returns A pointer to an interface */

static inline struct class_t *interface_itr_get_next(interface_iterator_t *itr)
{
    assert(interface_itr_has_next(*itr));

    return itr->interfaces[itr->index++];
} // interface_itr_get_next()

/******************************************************************************
 * Class type definition                                                      *
 ******************************************************************************/

/** Declaration of the ids of some of the system classes */

enum classes_t {
    JELATINE_RESERVED0 = 0, ///< Reserved entry in the class table
    JELATINE_RESERVED1 = 1, ///< Reserved entry in the class table
    JELATINE_RESERVED2 = 2, ///< Reserved entry in the class table
    JELATINE_RESERVED3 = 3, ///< Reserved entry in the class table
    JELATINE_BOOLEAN_ARRAY = 4, ///< Array of booleans
    JELATINE_CHAR_ARRAY = 5, ///< Array of chars
    JELATINE_FLOAT_ARRAY = 6, ///< Array of floats
    JELATINE_DOUBLE_ARRAY = 7, ///< Array of doubles
    JELATINE_BYTE_ARRAY = 8, ///< Array of bytes
    JELATINE_SHORT_ARRAY = 9, ///< Array of shorts
    JELATINE_INT_ARRAY = 10, ///< Array of integers
    JELATINE_LONG_ARRAY = 11, ///< Array of longs
    JAVA_LANG_OBJECT = 12, ///< java.lang.Object class
    JAVA_LANG_CLASS = 13, ///< java.lang.Class class
    JAVA_LANG_STRING = 14, ///< java.lang.String class
    JAVA_LANG_THREAD = 15, ///< java.lang.Thread class
    JAVA_LANG_THROWABLE = 16, ///< java.lang.Throwable class
    JAVA_LANG_ERROR = 17, ///< java.lang.Error class
    JAVA_LANG_NOCLASSDEFFOUNDERROR = 18, ///< java.lang.NoClassDefFoundError class
    JAVA_LANG_VIRTUALMACHINEERROR = 19, ///< java.lang.VirtualMachineError class
    JAVA_LANG_EXCEPTION = 20, ///< java.lang.Exception class
    JAVA_LANG_RUNTIMEEXCEPTION = 21, ///< java.lang.RuntimeException class
    JAVA_LANG_ILLEGALMONITORSTATEEXCEPTION = 22, ///< java.lang.IllegalMonitorStateException class
    JAVA_LANG_ARITHMETICEXCEPTION = 23, ///< java.lang.ArtihmeticException class
    JAVA_LANG_ARRAYSTOREEXCEPTION = 24, ///< java.lang.ArrayStoreException class
    JAVA_LANG_INDEXOUTOFBOUNDSEXCEPTION = 25, ///< java.lang.IndexOutOfBoundsException class
    JAVA_LANG_ARRAYINDEXOUTOFBOUNDSEXCEPTION = 26, ///< java.lang.ArrayIndexOutOfBoundsException class
    JAVA_LANG_NULLPOINTEREXCEPTION = 27, ///< java.lang.NullPointerException class
    JAVA_LANG_NEGATIVEARRAYSIZEEXCEPTION = 28, ///< java.lang.NegativeArraySizeException class
    JAVA_LANG_CLASSCASTEXCEPTION = 29, ///< java.lang.ClassCastException class
    JAVA_LANG_CLASSNOTFOUNDEXCEPTION = 30, ///< java.lang.ClassNotFoundException class
    JAVA_LANG_REF_REFERENCE = 31, ///< java.lang.ref.Reference class
    JAVA_LANG_REF_WEAKREFERENCE = 32 ///< java.lang.ref.WeakReference class
};

/** Typedef for ::enum classes_t */
typedef enum classes_t classes_t;

/** Number of predefined classes */
#define JELATINE_PREDEFINED_CLASSES_N (JAVA_LANG_REF_WEAKREFERENCE + 1)

/** Loading state of a class */

enum class_state_t {
    CS_DUMMY = 0, ///< Initial state of a newly allocated & cleared class
    CS_PRELOADED = 1, ///< Preloading, this may happen during bootstrap
    CS_LINKING = 2, ///< Linking, this happens during class resolution
    CS_LINKED = 3, ///< Class has been resolved & linked but not initialized
    CS_INITIALIZING = 4, ///< The class is being initialized
    CS_INITIALIZED = 5, ///< The class has been initialized
    CS_ERRONEOUS = 6 ///< The class is an erroneous state
};

/** Typedef for ::enum class_state_t */
typedef enum class_state_t class_state_t;

/** Structure representing a Java class */

struct class_t {
    char *name; ///< Class name
    uintptr_t obj; ///< Java class
    struct class_t *parent; ///< Parent class, NULL if the class has no parent
    const_pool_t *const_pool; ///< Constant pool of this class
    uint16_t access_flags; ///< Access flags for this class
    uint16_t state; ///< Current class state
    uint16_t id; ///< Numerical id in the class table

    // Array related data
    uint8_t elem_type; ///< Primitive type for array classes
    uint8_t dimensions; ///< Number of dimensions for array classes
    struct class_t *elem_class; ///< Class of the elements for arrays

    // Fields used in the initialization process
    thread_t *init_thread; ///< Initializer thread

    // Field related data
    uint32_t ref_n; ///< Number of reference fields in the instance
    uint32_t nref_size; ///< Size in bytes of the non-reference area
    field_manager_t *field_manager;///< Field manager of this class

    // Method related data
    method_manager_t *method_manager; ///< Method manager of this class
    interface_manager_t *interface_manager; ///< Interface manager of this class
    uint32_t itable_count; ///< Number of interface table entries
    uint32_t dtable_count; ///< Number of entries in the dispatch table
    method_t **dtable; ///< Dispatch table
    uint16_t *inames; ///< Names of the implemented interface methods
    method_t **itable; ///< Interface dispatch table
};

/** Typedef for ::struct class_t */
typedef struct class_t class_t;

/******************************************************************************
 * Class interface                                                            *
 ******************************************************************************/

extern int32_t class_bootstrap_id(const char *);
extern char *class_bootstrap_name(uint32_t);
extern bool class_is_parent(class_t *, class_t *);
extern void class_purge_initializer(class_t *);

/******************************************************************************
 * Class inlined functions                                                    *
 ******************************************************************************/

/** Gets the number of reference fields of the given class' instances
 * \param cl A valid pointer to a class_t structure
 * \returns The number of reference fields of the class' instances */

static inline uint32_t class_get_ref_n(const class_t *cl)
{
    return cl->ref_n;
} // class_get_ref_n()

/** Gets the size (in bytes) of the non-reference area of the given class'
 * instances
 * \param cl A valid pointer to a class_t structure
 * \returns The size of the non-reference area of the class' instances */

static inline uint32_t class_get_nref_size(const class_t *cl)
{
    return cl->nref_size;
} // class_get_nref_size()

/** Gets a pointer to the parent class of the passed one
 * \param cl A valid pointer to a class_t structure
 * \returns A pointer to the parent class */

static inline class_t *class_get_parent(const class_t *cl)
{
    return cl->parent;
} // class_get_parent()

/** Gets the index of the passed class
 * \param cl A valid pointer to a class_t structure
 * \returns The id of the passed class */

static inline uint32_t class_get_id(const class_t *cl)
{
    return cl->id;
} // class_get_id()

/** Checks if a class is an array class
 * \param cl A valid pointer to a class_t structure
 * \returns true if the class is an array class, false otherwise */

static inline bool class_is_array(const class_t *cl)
{
    return cl->access_flags & ACC_ARRAY;
} // class_is_array()

/** Gets the number of dimensions of an array class
 * \param cl A pointer to an array class
 * \returns The number of dimensions of the array class */

static inline uint8_t class_get_dimensions(const class_t *cl)
{
    assert(class_is_array(cl));
    return cl->dimensions;
} // class_get_dimensions()

/** Checks if a class is an interface
 * \param cl A valid pointer to a class_t structure
 * \returns true if the class is an interface, false otherwise */

static inline bool class_is_interface(const class_t *cl)
{
    return cl->access_flags & ACC_INTERFACE;
} // class_is_interface()

/** Checks if a class is abstract
 * \param cl A valid pointer to a class_t structure
 * \returns true if the class is abstract, false otherwise */

static inline bool class_is_abstract(const class_t *cl)
{
    return cl->access_flags & ACC_ABSTRACT;
} // class_is_abstract()

/** Checks if a class is final
 * \param cl A valid pointer to a class_t structure
 * \returns true if the class is final, false otherwise */

static inline bool class_is_final(const class_t *cl)
{
    return cl->access_flags & ACC_FINAL;
} // class_is_final()

/** Checks if a class is public
 * \param cl A valid pointer to a class_t structure
 * \returns true if the class is public */

static inline bool class_is_public(const class_t *cl)
{
    return cl->access_flags & ACC_PUBLIC;
} // class_is_public()

/** Checks if a class has the ACC_SUPER flag set
 * \param cl A valid pointer to a class_t structure
 * \returns true if the class is public */

static inline bool class_is_super(const class_t *cl)
{
    return cl->access_flags & ACC_SUPER;
} // class_is_super()

#if JEL_FINALIZER

/** Checks if a class has the ACC_HAS_FINALIZER flag set
 * \param cl A valid pointer to a class_t structure
 * \returns true if the class is public */

static inline bool class_has_finalizer(const class_t *cl)
{
    return cl->access_flags & ACC_HAS_FINALIZER;
} // class_has_finalizer()

#endif // JEL_FINALIZER

/** Sets the linkage state of a class
 * \param cl A pointer to a class structure
 * \param state The new state */

static inline void class_set_state(class_t *cl, class_state_t state)
{
    /* Class state can only 'go forward' */
    assert(cl->state < state);
    cl->state = state;
} // class_set_state()

/** Checks if a class has been preloaded
 * \param cl A pointer to a class structure
 * \returns true if the class is in the preloaded state, false otherwise */

static inline bool class_is_preloaded(const class_t *cl)
{
    return cl->state == CS_PRELOADED;
} // class_is_preloaded()

/** Checks if a class is being linked
 * \param cl A pointer to a class structure
 * \returns true if the class is in the linking state, false otherwise */

static inline bool class_is_being_linked(const class_t *cl)
{
    return cl->state == CS_LINKING;
} // class_is_being_linked()

/** Checks if a class has been linked
 * \param cl A pointer to a class structure
 * \returns true if the class is in the linked state, false otherwise */

static inline bool class_is_linked(const class_t *cl)
{
    return cl->state == CS_LINKED;
} // class_is_linked()

/** Checks if the class is being initialized
 * \param cl A pointer to a class structure
 * \returns true if the class is in the initializing state, false otherwise */

static inline bool class_is_being_initialized(const class_t *cl)
{
    return cl->state == CS_INITIALIZING;
} // class_is_being_initialized()

/** Checks if the class is being initialized
 * \param cl A pointer to a class structure
 * \returns true if the class is in the initializing state, false otherwise */

static inline bool class_is_initialized(const class_t *cl)
{
    return cl->state == CS_INITIALIZED;
} // class_is_initialized()

/** Returns the java.lang.Class object associated with this class
 * \param cl A pointer to a class structure
 * \returns A reference to the java.lang.Class object associated with this class
 */

static inline uintptr_t class_get_object(const class_t *cl)
{
    return cl->obj;
} // class_get_object()

#endif // !JELATINE_CLASS_H
