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

/** \file loader.c
 * Bootstrap class-loader implementation */

#include "wrappers.h"

#include "bytecode.h"
#include "class.h"
#include "classfile.h"
#include "constantpool.h"
#include "field.h"
#include "jstring.h"
#include "interpreter.h"
#include "loader.h"
#include "memory.h"
#include "method.h"
#include "opcodes.h"
#include "utf8_string.h"
#include "util.h"
#include "vm.h"

#include "java_lang_Class.h"
#include "java_lang_ref_Reference.h"
#include "java_lang_ref_WeakReference.h"
#include "java_lang_String.h"
#include "java_lang_Thread.h"
#include "java_lang_Throwable.h"
#include "jelatine_VMResourceStream.h"

/******************************************************************************
 * Type declarations                                                          *
 ******************************************************************************/

/** Number of entries in the class table after startup */
#define CLASS_TABLE_INIT ((16) + (JELATINE_PREDEFINED_CLASSES_N))

/** Increment of the class-table entries */
#define CLASS_TABLE_INC (16)

/** Represents the bootstrap class loader structure and its related data like
 * the class-table */

struct loader_t {
    class_t **class_table; ///< Current class table
    uint32_t used; ///< Used slots in the class table
    uint32_t capacity; ///< Available slots in the class table
    uint32_t interface_methods; ///< Counter used for method interfaces
};

/** Typedef for ::struct loader_t */
typedef struct loader_t loader_t;

/******************************************************************************
 * Globals                                                                    *
 ******************************************************************************/

/** Bootstrap class loader */
static loader_t bcl;

/*******************************************************************************
 * Class-related local function prototypes                                     *
 ******************************************************************************/

static void load_class(class_t *);
static void derive_class(class_t *, class_file_t *);
static void load_interfaces(class_t *, class_file_t *);
static void load_attributes(class_t *, class_file_t *);
static bool same_package(const class_t *, const class_t *);
static void grow_class_table( void );
static class_t *resolve_class(class_t *, uint16_t);
static void initialize_class(thread_t *, class_t *);

/*******************************************************************************
 * Field related local function prototypes                                     *
 ******************************************************************************/

static void load_fields(class_t *, class_file_t *);
static void load_field_attributes(class_t *, class_file_t *,
                                  field_attributes_t *);
static void load_field_attribute_ConstantValue(class_t *, class_file_t *,
                                               field_attributes_t *);
static void layout_fields(class_t *);
static field_t *resolve_instance_field(class_t *, uint16_t);
static static_field_t *resolve_static_field(class_t *, uint16_t);

/*******************************************************************************
 * Method related local function prototypes                                    *
 ******************************************************************************/

static uint32_t next_interface_id( void );
static void load_methods(class_t *, class_file_t *, bool *);
static void load_method_attributes(class_t *, class_file_t *,
                                   method_attributes_t *);
static void load_method_attribute_Code(class_t *, class_file_t *,
                                       method_attributes_t *);
static void load_attribute_Code_attributes(class_t *, class_file_t *,
                                           method_attributes_t *);
static void load_method_attribute_Exceptions(class_t *, class_file_t *,
                                             method_attributes_t *);
static void assign_interface_indexes(method_manager_t *);
static void create_dispatch_table(class_t *);
static void create_interface_dispatch_table(class_t *);
static uint8_t *load_bytecode(class_t *, method_t *);
static method_t *resolve_method(class_t *, uint16_t, bool);

/*******************************************************************************
 * Class loader related functions                                              *
 ******************************************************************************/

/** Initializes the bootstrap class loader */

void bcl_init( void )
{
    bcl.class_table = gc_malloc(sizeof(class_t *) * CLASS_TABLE_INIT);
    bcl.used = JELATINE_PREDEFINED_CLASSES_N;
    bcl.capacity = CLASS_TABLE_INIT;
    memset(bcl.class_table, 0, sizeof(class_t *) * bcl.capacity);
    bcl.interface_methods = 0;
} // bcl_init()

/** Returns a pointer to the class corresponding to the given id
 * \param id The id of the class
 * \returns A pointer to the class corresponding to \a id */

class_t *bcl_get_class_by_id(uint32_t id)
{
    class_t *cl;

    tm_lock();
    cl = bcl.class_table[id];
    tm_unlock();
    return cl;
} // bcl_get_class_by_id()

/** Asks the class loader to mark all its internal structures allocated on the
 * Java heap or holding Java references */

void bcl_mark( void )
{
    uint32_t count = bcl.used;
    class_t **ct = bcl.class_table;

    for (size_t i = 0; i < count; i++) {
        if (ct[i] != NULL) {
            gc_mark_reference(class_get_object(ct[i]));

            // Preloaded classes and array classes lack a field manager
            if (ct[i]->field_manager != NULL) {
                fm_mark_static(ct[i]->field_manager);
            }
        }
    }
} // bcl_mark()

/** Checks if the class \a src may be assigned to the class \a dest
 * \param src A pointer to the source class
 * \param dest A pointer to the destination class
 * \returns True if the types are assignable, false otherwise */

bool bcl_is_assignable(class_t *src, class_t *dest)
{
    if (class_is_array(src)) {
        /* If S is a class representing the array type SC[], that is, an array
         * of components of type SC, then: */

        if (class_is_array(dest)) {
            /* If T is an array type TC[], that is, an array of components of
             * type TC, then one of the following must be true: */

            if ((src->elem_type == PT_REFERENCE)
                && (dest->elem_type == PT_REFERENCE))
            {
                /* TC and SC are reference types (§2.4.6), and type SC can be
                 * cast to TC by recursive application of these rules. */

                return bcl_is_assignable(src->elem_class, dest->elem_class);
            } else if (src->elem_type == dest->elem_type) {
                // TC and SC are the same primitive type (§2.4.1).

                return true;
            } else {
                return false;
            }
        } else if (class_is_interface(dest)) {
            /* If T is an interface type, T must be one of the interfaces
             * implemented by arrays (§2.15). */

            return false; /* No array interfaces are present in the CLDC */
        } else {
            // If T is a class type, then T must be Object (§2.4.7).

            if (dest->id == JAVA_LANG_OBJECT) {
                return true;
            } else {
                return false;
            }
        }
    } else if (class_is_interface(src)) {
        // If S is an interface type, then:

        if (class_is_interface(dest)) {
            /* If T is an interface type, then T must be the same interface as
             * S or a superinterface of S (§2.13.2). */
            if (src == dest) {
                return true;
            }

            return im_is_present(src->interface_manager, dest);
        } else {
            // If T is a class type, then T must be Object (§2.4.7).

            if (dest->id == JAVA_LANG_OBJECT) {
                return true;
            } else {
                return false;
            }
        }
    } else {
        // If S is an ordinary (nonarray) class, then:

        if (class_is_interface(dest)) {
            /* If T is an interface type, then S must implement (§2.13)
             * interface T. */

            return im_is_present(src->interface_manager, dest);
        } else {
            /* If T is a class type, then S must be the same class (§2.8.1) as
             * T, or a subclass of T. */

            if (src == dest) {
                return true;
            }

            while (src->parent != NULL) {
                if (src->parent == dest) {
                    return true;
                } else {
                    src = src->parent;
                }
            }

            return false;
        }
    }
} // bcl_is_assignable()

/** Asks the class to be preloaded w/o linking it or pulling any data from
 * storage
 *
 * This function is used for preloading specific classes which need to be
 * available during the bootstrap cycle, it also initializes (for selected
 * classes) the ref_n and nref_size fields so that if objects of those classes
 * are created before the actual classes are loaded the garbage-collector can
 * still safely handle them.
 *
 * \param name The class name
 * \returns A pointer to the preloaded class */

class_t *bcl_preload_class(const char *name)
{
    class_t *cl;
    int32_t id;

    tm_lock();
    cl = bcl_find_class_by_name(name);

    if (cl == NULL) {
        cl = gc_palloc(sizeof(class_t));
        id = class_bootstrap_id(name);

        if (id < 0) {
            if (bcl.used >= bcl.capacity) {
                grow_class_table();
            }

            id = bcl.used;
            cl->id = bcl.used;
            bcl.used++;
            bcl.class_table[id] = cl;
        } else {
            if (id == JAVA_LANG_STRING) {
                cl->ref_n = JAVA_LANG_STRING_REF_N;
                cl->nref_size = JAVA_LANG_STRING_NREF_SIZE;
            } else if (id == JAVA_LANG_CLASS) {
                cl->ref_n = JAVA_LANG_CLASS_REF_N;
                cl->nref_size = JAVA_LANG_CLASS_NREF_SIZE;
            } else if (id == JAVA_LANG_THREAD) {
                cl->ref_n = JAVA_LANG_THREAD_REF_N;
                cl->nref_size = JAVA_LANG_THREAD_NREF_SIZE;
            }

            cl->id = id;
            bcl.class_table[id] = cl;
        }

        cl->name = utf8_intern(name, strlen(name));
        class_set_state(cl, CS_PRELOADED);
    }

    tm_unlock();
    return cl;
} // bcl_preload_class()

/*******************************************************************************
 * Class related local functions                                               *
 ******************************************************************************/

/** Loads and links a class
 * \param cl A pointer to the class to be loaded */

static void load_class(class_t *cl)
{
    class_file_t *cf;
    class_t *elem;
    class_t *jcl;
    char *str;
    char *jname;
    size_t len;
    size_t i;

    class_set_state(cl, CS_LINKING); // Put the class in the linking state

    len = strlen(cl->name);

    if (len == 0) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR, "Malformed class name");
    }

    if (cl->name[0] != '[') {
        // Regular class
        cf = cf_open(cl->name);
        derive_class(cl, cf);
        cf_close(cf);
    } else {
        // Array class
        if (len < 2) {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Malformed array class name");
        }

        for (i = 0; cl->name[i] == '['; i++) {
            ;
        }

        cl->dimensions = i;

        if (cl->name[1] == '[') {
            // Multi-dimensional array
            cl->elem_type = PT_REFERENCE;

            str = gc_malloc(sizeof(char) * len);
            memcpy(str, cl->name + 1, len - 1);
            str[len - 1] = 0;

            elem = bcl_resolve_class_by_name(NULL, str);
            cl->elem_class = elem;
            cl->access_flags = elem->access_flags | ACC_ARRAY;

            gc_free(str);
        } else if (cl->name[1] == 'L') {
            // Reference array
            if ((len < 4) || (cl->name[len - 1] != ';')) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Malformed array class name");
            }

            cl->elem_type = PT_REFERENCE;

            str = gc_malloc(sizeof(char) * (len - 2));
            memcpy(str, cl->name + 2, len - 3);
            str[len - 3] = 0;

            elem = bcl_resolve_class_by_name(NULL, str);
            cl->elem_class = elem;
            cl->access_flags = elem->access_flags | ACC_ARRAY;

            gc_free(str);
        } else {
            // Primitive array
            switch (cl->name[1]) {
                case 'B': cl->elem_type = PT_BYTE; break;
                case 'C': cl->elem_type = PT_CHAR; break;
#if JEL_FP_SUPPORT
                case 'D': cl->elem_type = PT_DOUBLE; break;
                case 'F': cl->elem_type = PT_FLOAT; break;
#endif // JEL_FP_SUPPORT
                case 'I': cl->elem_type = PT_INT; break;
                case 'J': cl->elem_type = PT_LONG; break;
                case 'S': cl->elem_type = PT_SHORT; break;
                case 'Z': cl->elem_type = PT_BOOL; break;
                default:
                    c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                            "Malformed array class name");
            }

            cl->elem_class = NULL;
            cl->access_flags = ACC_FINAL | ACC_ABSTRACT | ACC_PUBLIC
                               | ACC_ARRAY;
        }

        /* Set up the other fields as if the class was inheriting from
         * java.lang.Object, make sure that class initialization starts by
         * resolving java.lang.Object otherwise this will fail */
        cl->parent = bcl.class_table[JAVA_LANG_OBJECT];

        // Initialize the rest of the array class structure
        cl->const_pool = cp_create_dummy();
        cl->field_manager = NULL;
        cl->method_manager = NULL;
        cl->interface_manager = NULL;
        cl->ref_n = 0;
        cl->nref_size = 0;
        cl->dtable = bcl.class_table[JAVA_LANG_OBJECT]->dtable;
        cl->itable_count = 0;
        cl->inames = NULL;
        cl->itable = NULL;
    }

    // Create the embedded Java class object
    jcl = bcl_find_class_by_name("java/lang/Class");
    cl->obj = gc_new(jcl);
    JAVA_LANG_CLASS_REF2PTR(cl->obj)->id = cl->id;
    JAVA_LANG_CLASS_REF2PTR(cl->obj)->is_array = class_is_array(cl) ? 1 : 0;
    JAVA_LANG_CLASS_REF2PTR(cl->obj)->is_interface =
        class_is_interface(cl) ? 1 : 0;

    jname = gc_malloc(sizeof(char) * (len + 1));

    for (i = 0; i < len; i++) {
        if (cl->name[i] == '/') {
            jname[i] = '.';
        } else {
            jname[i] = cl->name[i];
        }
    }

    jname[len] = 0;
    JAVA_LANG_CLASS_REF2PTR(cl->obj)->name =
        JAVA_LANG_STRING_PTR2REF(jstring_create_literal(jname));
    gc_free(jname);

    // Put the class in the linked state
    class_set_state(cl, CS_LINKED);
} // load_class()

/** Resolves a class reference
 * \param orig The originating class
 * \param class_index Indes in the constant-pool were the class descriptor lies
 * \returns A pointer to the class */

static class_t *resolve_class(class_t *orig, uint16_t index)
{
    class_t *cl;
    char *name;

    if (cp_get_tag(orig->const_pool, index) == CONSTANT_Class_resolved) {
        return cp_get_resolved_class(orig->const_pool, index);
    }

    name = cp_get_class_name(orig->const_pool, index);
    cl = bcl_resolve_class_by_name(orig, name);
    cp_set_tag_and_data(orig->const_pool, index, CONSTANT_Class_resolved, cl);

    return cl;
} // resolve_class()

/** Resolves a class using its name
 * \param orig The originating class
 * \param name The class' name
 * \returns A pointer to the class */

class_t *bcl_resolve_class_by_name(class_t *orig, const char *name)
{
    class_t *cl, *temp;
    int exception;

    tm_lock();
    cl = bcl_find_class_by_name(name);

    c_try {
        if (cl != NULL) {
            /*
             * Only one thread can enter the class loader at once, so if the
             * resolution process meets a class which is still being linked
             * there must be a circularity in the class graph, thus class
             * circularity exceptions would be thrown here by a non-CLDC
             * implementation
             */
            if (class_is_being_linked(cl)) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Circular dependency found in the class graph");
            } else {
                if (class_is_preloaded(cl)) {
                    load_class(cl);
                }

                /* If the class is not in the preloaded state it is either
                 * linked, initializing or initialized, thus we can return
                 * safely */
            }
        } else {
            cl = bcl_preload_class(name);
            load_class(cl);
        }

        // Check the permissions
        if (!class_is_public(cl) && (orig != NULL)) {
            // If this class is an array climb down to the array element type
            temp = cl;

            while (class_is_array(temp)) {
                temp = temp->elem_class;
            }

            if (!same_package(temp, orig)) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Class %s cannot reference class %s",
                        orig->name, cl->name);
            }
        }
    } c_catch(exception) {
        tm_unlock();
        c_rethrow(exception);
    }

    tm_unlock();
    return cl;
} // bcl_resolve_class_by_name()

/** Executes the static initialization of class
 * \param thread The thread trying to initialize the class
 * \param cl The class which will be initialized */

static void initialize_class(thread_t *thread, class_t *cl)
{
    method_t *clinit;

    monitor_enter(thread, class_get_object(cl));

    if (class_is_being_initialized(cl)) {
        if (thread != cl->init_thread) {
            // Another thread is initializing the class, let's wait for it
            thread_wait(thread, class_get_object(cl), 0, 0);
        } else {
            // This is a recursive call
            monitor_exit(thread, class_get_object(cl));
            return;
        }
    }

    if (class_is_initialized(cl)) {
        // We're done
        monitor_exit(thread, class_get_object(cl));
        return;
    } else {
        // Register this thread as the one initializing this class
        class_set_state(cl, CS_INITIALIZING);
        cl->init_thread = thread;
        monitor_exit(thread, class_get_object(cl));
    }

    if ((cl->id != JAVA_LANG_OBJECT) && !class_is_initialized(cl->parent)) {
        /* If this class has a parent class initialize it */
        initialize_class(thread, cl->parent);

        if (thread->exception != JNULL) {
            monitor_enter(thread, class_get_object(cl));
            class_set_state(cl, CS_ERRONEOUS);
            thread_notify(thread, class_get_object(cl), true);
            monitor_exit(thread, class_get_object(cl));
            return;
        }
    }

    if (cl->id != JAVA_LANG_OBJECT) {
        // Initialize the class
        clinit = mm_get(cl->method_manager, "<clinit>", "()V");

        if (clinit != NULL) {
            interpreter(clinit);

            if (thread->exception != JNULL) {
                /* TODO: Regular Java requires a more complex handling of this
                 * exception, we could think about it later */
                monitor_enter(thread, class_get_object(cl));
                class_set_state(cl, CS_ERRONEOUS);
                thread_notify(thread, class_get_object(cl), true);
                monitor_exit(thread, class_get_object(cl));
                return;
            }

            // We're done with the initializer so free it
            class_purge_initializer(cl);
        }
    }

    monitor_enter(thread, class_get_object(cl));
    class_set_state(cl, CS_INITIALIZED);
    thread_notify(thread, class_get_object(cl), true);
    monitor_exit(thread, class_get_object(cl));
    return;
} // initialize_class()

/**  Derives the provided class structure from a class file
 * \param cl The class structure which will hold the new class
 * \param cf The class-file of the class */

static void derive_class(class_t *cl, class_file_t *cf)
{
    uint16_t u2_data;
    uint32_t u4_data;
    const_pool_t *cp;
    bool finalizer;

    /* Load the first elements of the class-file and check that they contain
     * correct values */
    u4_data = cf_load_u4(cf);

    if (u4_data != 0xCAFEBABE) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Malformed class, 0xCAFEBABE magic value is missing");
    }

    /* Check the class file format major version number and ignore the minor
     * version number, the spec mandates that all should be supported */

    u2_data = cf_load_u2(cf); // Minor number
    u2_data = cf_load_u2(cf); // Major number

    if ((u2_data < 45) || (u2_data > 50)) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR, "Unsupported class version");
    }

    // Load the constant pool entries
    cp = cp_create(cl, cf);
    cp_set_tag_and_data(cp, 0, 0, cl);
    cl->const_pool = cp;

    // Load the access flags and check them
    cl->access_flags = (cf_load_u2(cf) & CLASS_ACC_FLAGS_MASK);

    if (class_is_interface(cl)) {
        if (!class_is_abstract(cl) || class_is_final(cl)) {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Interface class has ACC_ABSTRACT not set or "
                    "ACC_FINAL set");
        }
    } else {
        if (class_is_abstract(cl) && class_is_final(cl)) {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Abstract class has ACC_FINAL flag set");
        }
    }

    // Check if the class-name is valid and matches the provided one

    u2_data = cf_load_u2(cf);

    if (strcmp(cl->name, cp_get_class_name(cp, u2_data)) != 0) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Internal class name does not match the provided one");
    }

    // Load the parent class name

    u2_data = cf_load_u2(cf);

    if (u2_data == 0) {
        /*
         * Object doesn't have a parent class and must be public, non-final,
         * not abstract nor an interface
         */
        cl->parent = NULL;

        if (class_is_final(cl)
                || ! class_is_public(cl)
                || class_is_interface(cl)
                || class_is_abstract(cl))
        {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "java.lang.Object is either final, non-public, abstract or "
                    "an interface");
        }
    } else {
        cl->parent = resolve_class(cl, u2_data);
    }

    if (class_is_interface(cl)) {
        if (class_get_id(cl->parent) != JAVA_LANG_OBJECT) {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Interface has a parent different from java.lang.Object");
        }
    } else if (cl->parent != NULL) {
        if (class_is_interface(cl->parent) || class_is_final(cl->parent)
            || class_is_array(cl->parent))
        {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Parent class is either an interface, final or an array "
                    "class");
        }
    }

    // Load the interfaces, these will be resolved at link time
    load_interfaces(cl, cf);

    // Load and layout the fields
    load_fields(cl, cf);
    layout_fields(cl);

    /* Load the methods and create the dispatch tables, also check if the class
     * declares a finalize() method */
    load_methods(cl, cf, &finalizer);

#if JEL_FINALIZER
    if (cl->parent != NULL) {
        /* Note that java.lang.Object declares a finalize() method but since
         * this method is empty it won't be flaged as declaring one */

        if (class_has_finalizer(cl->parent) || finalizer) {
            /* If the parent has a finalize() method the child class should
             * call it too when garbage collected even if it doesn't declare
             * its own finalize() method */
            cl->access_flags |= ACC_HAS_FINALIZER;
        }
    }
#endif // JEL_FINALIZER

    if (class_is_interface(cl)) {
        assign_interface_indexes(cl->method_manager);
    } else {
        create_dispatch_table(cl); // Create the virtual dispatch table
        create_interface_dispatch_table(cl);
    }

    // Load the class attributes (basically ignore them ATM)
    load_attributes(cl, cf);
} // derive_class()

/** Loads and verifies the consistency of the interfaces from the
 * specified class-file
 * \param cl A pointer to a class structure
 * \param cf A pointer to the class-file representing the class */

static void load_interfaces(class_t *cl, class_file_t *cf)
{
    interface_manager_t *im = im_create();
    interface_iterator_t itr;
    class_t *interface;
    uint32_t interface_count;
    uint16_t index;

    // Add the interfaces implemented by the parent class

    if (cl->parent != NULL) {
        itr = interface_itr(cl->parent->interface_manager);

        while (interface_itr_has_next(itr)) {
            im_add(im, interface_itr_get_next(&itr));
        }
    }

    // Add all the interfaces implemented by this class plus their parents

    interface_count = cf_load_u2(cf);

    for (size_t i = 0; i < interface_count; i++) {
        index = cf_load_u2(cf);
        interface = resolve_class(cl, index);

        if (class_is_interface(interface)) {
            im_add(im, interface);
            itr = interface_itr(interface->interface_manager);

            while (interface_itr_has_next(itr)) {
                im_add(im, interface_itr_get_next(&itr));
            }
        } else {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Non-interface class implemented as an interface");
        }
    }

    im_flatten(im);
    cl->interface_manager = im;
} // load_interfaces()

/** Loads a class attributes
 *
 * All class attributes are ignored ATM
 *
 * \param cl A pointer to a class structure
 * \param cf A pointer to the class file */

static void load_attributes(class_t *cl, class_file_t *cf)
{
    uint32_t attr_count = cf_load_u2(cf);
    uint32_t attr_length;
    long real_length;
    uint16_t attr_name;

    for (size_t i = 0; i < attr_count; i++) {
        attr_name = cf_load_u2(cf);
        attr_length = cf_load_u4(cf);
        real_length = cf_tell(cf);

        // Silently ignore all other attributes
        cf_seek(cf, attr_length, SEEK_CUR);

        real_length = cf_tell(cf) - real_length;

        if (real_length != attr_length) {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Actual length of an attribute is different from the "
                    "length provided in the class file");
        }
    }
} // load_attributes()

/** Checks that two classes belong to the same package
 * \param cl1 A class pointer
 * \param cl2 A class pointer
 * \returns True if both classes belong to the same package */

static bool same_package(const class_t *cl1, const class_t *cl2)
{
    uint32_t i = 0;
    uint32_t sl1 = 0;
    uint32_t sl2 = 0;
    const char *str1 = cl1->name;
    const char *str2 = cl2->name;

    while (str1[i] != 0) {
        if (str1[i] == '/') {
            sl1 = i;
        }

        i++;
    }

    i = 0;

    while (str2[i] != 0) {
        if (str2[i] == '/') {
            sl2 = i;
        }

        i++;
    }

    if (sl1 != sl2) {
        return false;
    }

    if ((sl1 == 0) && (sl2 == 0)) {
        return true;
    } else if (memcmp(str1, str2, sl1 + 1) == 0) {
        return true;
    }

    return false;
} //* same_package()

/** Finds a class by its name
 * \param name The class' name
 * \returns A pointer to the class or NULL if none is found */

class_t *bcl_find_class_by_name(const char *name)
{
    class_t *res = NULL;

    tm_lock();

    for (size_t i = 0; i < bcl.used; i++) {
        if (bcl.class_table[i] != NULL) {
            if (strcmp(name, bcl.class_table[i]->name) == 0) {
                res = bcl.class_table[i];
                break;
            }
        }
    }

    tm_unlock();
    return res;
} /* bcl_find_class_by_name() */

/** Grows the class, dispatch and interface dispatch table and updates all the
 * threads cached table pointers */

static void grow_class_table( void )
{
    class_t **new_ct;

    bcl.capacity += CLASS_TABLE_INC;

    // Allocate a new class table and fill it
    new_ct = gc_malloc(bcl.capacity * sizeof(class_t *));
    memcpy(new_ct, bcl.class_table, bcl.used * sizeof(class_t *));

    gc_free(bcl.class_table); // Free the old table
    bcl.class_table = new_ct; // Update the table pointer
} // grow_class_table()

/*******************************************************************************
 * Method related functions                                                    *
 ******************************************************************************/

/** Returns the next index for an interface method
 * \returns The next index for an interface method */

static uint32_t next_interface_id( void )
{
    uint32_t res = bcl.interface_methods;

    bcl.interface_methods++;

    return res;
} // next_interface_id()

/** Creates the method table from the class-file representation
 * \param cl A pointer to the class structure
 * \param cf A pointer to the class file
 * \param finalizer The pointed variable will hold true if the class has a
 * finalizer, false otherwise */

static void load_methods(class_t *cl, class_file_t *cf, bool *finalizer)
{
    const_pool_t *cp = cl->const_pool;
    method_manager_t *mm;
    method_t *method;
    method_iterator_t itr;
    char *name, *descriptor;
    method_attributes_t attr;
    uint32_t count; /* Number of methods */
    uint16_t access_flags;
    uint16_t name_index;
    uint16_t descriptor_index;

    count = cf_load_u2(cf);
    mm = mm_create(count);
#if JEL_FINALIZER
    *finalizer = false;
#endif // JEL_FINALIZER

    for (size_t i = 0; i < count; i++) {
        access_flags = cf_load_u2(cf) & METHOD_ACC_FLAGS_MASK;
        name_index = cf_load_u2(cf);
        descriptor_index = cf_load_u2(cf);

        name = cp_get_string(cp, name_index);
        descriptor = cp_get_string(cp, descriptor_index);
        load_method_attributes(cl, cf, &attr);

        mm_add(mm, name, descriptor, access_flags, cp, &attr);
    }

    itr = method_itr(mm);

    // Link native methods immediately

    while (method_itr_has_next(itr)) {
        method = method_itr_get_next(&itr);

        if (method_is_native(method)) {
            method_link_native(method, cl->name);
        }
    }

    /* If the current class is an interface check that it's methods are all
     * public and abstract */

    itr = method_itr(mm);

    if (class_is_interface(cl)) {
        while (method_itr_has_next(itr)) {
            method = method_itr_get_next(&itr);

            if (method_is_static(method)) {
                if ((strcmp(method->name, "<clinit>") == 0)
                    && (strcmp(method->descriptor, "()V") == 0))
                {
                    /* Interface classes are allowed to have a class
                     * initialization method */
                    continue;
                }

                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Interface has a static method");
            }

            if (! method_is_public(method)) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Interface has non-public method");
            } else if (! method_is_abstract(method)) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Interface has non-abstract method");
            }
        }
    } else {
#if JEL_FINALIZER
        while (method_itr_has_next(itr)) {
            method = method_itr_get_next(&itr);

            if ((strcmp(method->name, "finalize") == 0)
                && (strcmp(method->descriptor, "()V") == 0))
            {
                // This class has a non-empty finalizer method
                *finalizer = true;
            }
        }
#endif // JEL_FINALIZER
    }

    cl->method_manager = mm;
} // load_methods()

/** Load a method attributes
 *
 * The function stores the retrieved attribute data in the structure pointed by
 * the \a attr parameter
 * \param cl A pointer to the class being loaded
 * \param cf A pointer to the class file
 * \param attr A pointer to an method_attribute_t structure */

static void load_method_attributes(class_t *cl, class_file_t *cf,
                                   method_attributes_t *attr)
{
    const_pool_t *cp = cl->const_pool;
    uint32_t attributes_count, attr_length;
    long real_length;
    uint16_t attr_name;
    bool code_found = false;
    bool exceptions_found = false;

    attributes_count = cf_load_u2(cf);

    for (size_t i = 0; i < attributes_count; i++) {
        attr_name = cf_load_u2(cf);
        attr_length = cf_load_u4(cf);
        real_length = cf_tell(cf);

        if (strcmp(cp_get_string(cp, attr_name), "Code") == 0) {
            // Code attribute
            if (code_found) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Duplicated Code attribute found");
            }

            load_method_attribute_Code(cl, cf, attr);
            code_found = true;
        } else if (strcmp(cp_get_string(cp, attr_name), "Exceptions") == 0) {
            // Exceptions attribute
            if (exceptions_found) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Duplicated Exception attribute found");
            }

            load_method_attribute_Exceptions(cl, cf, attr);
            exceptions_found = true;
        } else {
            // Silently ignore all other attributes
            cf_seek(cf, attr_length, SEEK_CUR);
        }

        real_length = cf_tell(cf) - real_length;

        if (real_length != attr_length) {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Actual length of an attribute is different from the "
                    "length provided in the class file");
        }
    }

    attr->code_found = code_found;
    attr->exceptions_found = exceptions_found;
} // load_method_attributes()

/** Load a method's 'Code' attribute
 * \param cl A pointer to the current class
 * \param cf A pointer to the class file
 * \param attr A pointer to method_attributes_t structure used to store the
 * attribute data */

static void load_method_attribute_Code(class_t *cl, class_file_t *cf,
                                       method_attributes_t *attr)
{
    uint32_t code_length;

    /* A method's bytecode is not loaded at this stage, the offset in the class
     * file which points to it is stored in the extra data union of the method
     * structure. At link time (i.e. when the method is actually called for the
     * first time) it is loaded */
    attr->max_stack = cf_load_u2(cf);
    attr->max_locals = cf_load_u2(cf);

    code_length = cf_load_u4(cf);

    if ((code_length > 65536) || (code_length == 0)) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Code length is either 0 or more than 65536");
    }

    attr->code_length = code_length;
    attr->code_offset = cf_tell(cf);

    // The exception handlers are also loaded at link time
    cf_seek(cf, attr->code_length, SEEK_CUR);
    attr->exception_table_length = cf_load_u2(cf);
    cf_seek(cf, attr->exception_table_length * 8, SEEK_CUR);

    // Load the code attribute attributes
    load_attribute_Code_attributes(cl, cf, attr);
} // load_method_attribute_Code()

/** Loads the attributes of a 'Code' attribute
 * \param cl A pointer to the current class
 * \param cf A pointer to the class file
 * \param attr A pointer to a method_attributes_t structure used to hold the
 * attribute data */

static void load_attribute_Code_attributes(class_t *cl, class_file_t *cf,
                                           method_attributes_t *attr)
{
    long real_length;
    uint32_t attributes_count, attr_length;
    uint16_t attr_name;

    attributes_count = cf_load_u2(cf);

    for (size_t i = 0; i < attributes_count; i++) {
        attr_name = cf_load_u2(cf);
        attr_length = cf_load_u4(cf);
        real_length = cf_tell(cf);
        // Silently ignore all other attributes
        cf_seek(cf, attr_length, SEEK_CUR);
        real_length = cf_tell(cf) - real_length;

        if (real_length != attr_length) {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Actual length of an attribute is different from the "
                    "length provided in the class file");
        }
    }
} // load_attribute_Code_attributes()

/** Loads the Exceptions attribute of a method
 *
 * Jelatine doesn't use this attribute actually even if it recognizes it as the
 * virtual machine specification mentions that only the compiler should check
 * which exceptions are thrown and which aren't
 *
 * \param cl A pointer to the current class
 * \param cf A pointer to the class file
 * \param attr A pointer to a method_attributes_t structure used to store the
 * attribute data */

static void load_method_attribute_Exceptions(class_t *cl, class_file_t *cf,
                                             method_attributes_t *attr)
{
    uint16_t number_of_exceptions;
    uint16_t *exceptions;

    number_of_exceptions = cf_load_u2(cf);
    exceptions = gc_malloc(sizeof(uint16_t) * number_of_exceptions);

    for (size_t i = 0; i < number_of_exceptions; i++) {
        exceptions[i] = cf_load_u2(cf);
    }

    gc_free(exceptions);
} // load_method_attribute_Exceptions()

/** Assigns the interface indexes to an interface methods'
 * \param mm A pointer to the method manager */

static void assign_interface_indexes(method_manager_t *mm)
{
    method_iterator_t itr = method_itr(mm);
    method_t *method;

    while (method_itr_has_next(itr)) {
        method = method_itr_get_next(&itr);
        method_set_index(method, next_interface_id());
    }
} // assign_interface_indexes()

/** Creates the virtual dispatch table
 * \param cl A pointer to the current class */

static void create_dispatch_table(class_t *cl)
{
    method_iterator_t itr = method_itr(cl->method_manager);
    method_t *method;
    method_t **parent_dtable, **new_dtable;
    uint32_t new_count;
    bool found;

    if (cl->parent == NULL) {
        /* java.lang.Object is the only class which doesn't have a parent
         * class and so a parent dispatch table */
        new_count = 0;

        while (method_itr_has_next(itr)) {
            method = method_itr_get_next(&itr);
            /* Skip static methods, initialization methods or private methods,
             * as they are called directly and virtual dispatch does not apply
             * to them */
            if (method_is_static(method)
                || method_is_init(method)
                || method_is_private(method))
            {
                method_set_index(method, 0);
                continue;
            }

            method_set_index(method, new_count);
            new_count++;
        }
    } else {
        new_count = cl->parent->dtable_count;
        parent_dtable = cl->parent->dtable;

        while (method_itr_has_next(itr)) {
            method = method_itr_get_next(&itr);

            /* Skip static methods, initialization methods or private methods,
             * as they are called directly and virtual dispatch does not apply
             * to them */
            if (method_is_static(method)
                || method_is_init(method)
                || method_is_private(method))
            {
                method_set_index(method, 0);
                continue;
            }

            found = false;

            // Look for the current method in the parent dispatch table
            for (size_t i = 0; i < cl->parent->dtable_count; i++) {
                if (method_compare(method, parent_dtable[i])) {
                    /* This method overrides another method, check that it is
                     * not final */
                    if (method_is_final(parent_dtable[i])) {
                        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                                "A method overrides a final method");
                    }

                    if ((method_is_public(parent_dtable[i])
                         && !method_is_public(method))
                        || (method_is_protected(parent_dtable[i])
                            && !(method_is_protected(method)
                                 || method_is_public(method))))
                    {
                        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                                "A method is overriden by another method with "
                                "weaker access privileges");
                    }

                    method_set_index(method, i);
                    found = true;
                    break;
                }
            }

            if (found == false) {
                method_set_index(method, new_count);
                new_count++;
            }
        }
    }

    new_dtable = gc_palloc(sizeof(method_t *) * new_count);

    // Copy the old method table in the new one
    if (cl->parent != NULL) {
        memcpy(new_dtable, cl->parent->dtable,
               sizeof(method_t *) * cl->parent->dtable_count);
    }

    /* Add the new methods to the new method table eventally replacing the
     * overridden ones */
    itr = method_itr(cl->method_manager);

    while (method_itr_has_next(itr)) {
        method = method_itr_get_next(&itr);

        if (! (method_is_static(method)
            || method_is_private(method)
            || method_is_init(method)))
        {
            new_dtable[method_get_index(method)] = method;
        }
    }

    cl->dtable_count = new_count;
    cl->dtable = new_dtable;
} // create_dispatch_table()

/**  Creates the interface dispatch table of a class
 * \param cl A pointer to the current class */

static void create_interface_dispatch_table(class_t *cl)
{
    class_t *interface;
    interface_iterator_t i_itr = interface_itr(cl->interface_manager);
    method_iterator_t m_itr;
    method_t **new_itable;
    uint16_t *new_inames;
    uint32_t new_itable_count = 0;
    uint32_t dtable_count = cl->dtable_count;
    uint32_t j;

    while (interface_itr_has_next(i_itr)) {
        interface = interface_itr_get_next(&i_itr);
        new_itable_count += mm_get_count(interface->method_manager);
    }

    if (new_itable_count == 0) {
        cl->itable_count = 0;
        cl->inames = NULL;
        cl->itable = NULL;
        return;
    }

    new_itable = gc_palloc(sizeof(method_t *) * new_itable_count);
    new_inames = gc_palloc(sizeof(uint16_t) * new_itable_count);
    j = 0;
    i_itr = interface_itr(cl->interface_manager);

    while (interface_itr_has_next(i_itr)) {
        interface = interface_itr_get_next(&i_itr);
        m_itr = method_itr(interface->method_manager);

        while (method_itr_has_next(m_itr)) {
            new_itable[j] = method_itr_get_next(&m_itr);
            j++;
        }
    }

    for (size_t i = 0; i < new_itable_count; i++) {
        /* Replace the pointers to the interface methods with the actual methods
         * implementing them */
        new_inames[i] = new_itable[i]->index;

        for (j = 0; j < dtable_count; j++) {
            if (method_compare(new_itable[i], cl->dtable[j])) {
                new_itable[i] = cl->dtable[j];
            }
        }
    }

    sort_asc_uint16_ptrs(new_inames, (void **) new_itable, new_itable_count);

    cl->itable_count = new_itable_count;
    cl->inames = new_inames;
    cl->itable = new_itable;
} // create_interface_dispatch_table()

/**  Resolves a method if it wasn't already done
 * \param src A pointer to the class requesting the resolution
 * \param index An index in the class' constant pool holding the
 * CONSTANT_Methodref or CONSTANT_InterfaceMethodsref structure describing the
 * method to be resolved
 * \param interface Set to true if the requested method belongs to an interface
 * \returns A pointer to the method, throws an exception if the method cannot be
 * resolved or isn't found */

static method_t *resolve_method(class_t *src, uint16_t index, bool interface)
{
    const_pool_t *cp = src->const_pool;
    class_t *cl, *temp, *interface_cl;
    interface_iterator_t itr;
    method_t *method = NULL;
    char *name, *desc;
    uint16_t class_index;

    // Check if the method has already been resolved
    if (interface) {
        if (cp_get_tag(cp, index) == CONSTANT_InterfaceMethodref_resolved) {
            return cp_get_resolved_interfacemethod(cp, index);
        }
    } else if (cp_get_tag(cp, index) == CONSTANT_Methodref_resolved) {
        return cp_get_resolved_method(cp, index);
    }

    if (interface) {
        name = cp_get_interfacemethodref_name(cp, index);
        desc = cp_get_interfacemethodref_descriptor(cp, index);
        class_index = cp_get_interfacemethodref_class(cp, index);
    } else {
        name = cp_get_methodref_name(cp, index);
        desc = cp_get_methodref_descriptor(cp, index);
        class_index = cp_get_methodref_class(cp, index);
    }

    cl = resolve_class(src, class_index);
    initialize_class(thread_self(), cl);
    temp = cl;

    if (interface == false) {
        if (class_is_interface(cl)) {
            c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                    "Trying to resolve a method from an interface");
        }
    } else {
        if (! class_is_interface(cl)) {
            c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                    "Trying to resolve an interface method from a class");
        }
    }

    /* Look for the method in the current class and then in all its parent
     * classes until it is found or we reach java.lang.Object at which point
     * the resolution procedure fails */
    while (true) {
        method = mm_get(cl->method_manager, name, desc);

        if (method != NULL) {
            break;
        } else {
            if (cl->id == JAVA_LANG_OBJECT) {
                break;
            }

            cl = cl->parent;
        }
    }

    if (method == NULL) {
        cl = temp;

        itr = interface_itr(cl->interface_manager);

        while (interface_itr_has_next(itr)) {
            interface_cl = interface_itr_get_next(&itr);
            method = mm_get(interface_cl->method_manager, name, desc);

            if (method != NULL) {
                cl = interface_cl;
                break;
            }
        }

        if (method == NULL) {
            c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                    "Unable to resolve method, method not found");
        }
    }

    /* If we got here we actually found a method, let's check if we are allowed
     * to access it */

    if (method_is_init(method)) {
        /* Initialization methods must be resolved from the class directly
         * referenced by the instruction, a NoSuchMethodError should be
         * thrown if this does not happen */
        if (cl != temp) {
            c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                    "Error while resolving an initialization method");
        }
    }

    if (method_is_private(method) && src != cl) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Trying to access a private method from an external class");
    } else if (method_is_protected(method)) {
        if (!((src == cl)
              || class_is_parent(cl, src)
              || same_package(cl, src)))
        {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Trying to access a protected method from a non-child "
                    "class of a different package");
        }
    } else if (!method_is_public(method) && !same_package(cl, src)) {
        // Field is package visible, if ACC_PUBLIC is set no check is needed
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Trying to access a package-visible method from a different "
                "package");
    }

    if (interface) {
        cp_set_tag_and_data(cp, index,
                            CONSTANT_InterfaceMethodref_resolved, method);
    } else {
        cp_set_tag_and_data(cp, index, CONSTANT_Methodref_resolved, method);
    }

    return method;
} // resolve_method()

/**  Loads the bytecode of a method (and the exception handlers too) and
 * return it
 * \param cl A pointer to the class owning the method
 * \param method A pointer to the method to be loaded
 * \returns The bytecode of the method */

static uint8_t *load_bytecode(class_t *cl, method_t *method)
{
    class_file_t *cf;
    uint8_t *code;
    uint32_t i, code_length;

    cf = cf_open(cl->name);
    cf_seek(cf, method->data.offset, SEEK_SET);

    code_length = method_get_code_length(method);
    code = gc_malloc(code_length);

    if (method_is_synchronized(method)) {
        i = 1; // The first instruction will be added later
    } else {
        i = 0;
    }

    // Load the actual code
    while (i < code_length) {
        code[i] = cf_load_u1(cf);
        i++;
    }

    cf_close(cf);
    return code;
} // load_bytecode()

/** Loads the exception handlers of a method
 * \param cl A pointer to the class owning the method
 * \param method A pointer to the method to be loaded
 * \param code A pointer to the method code
 * \returns The exception handlers of the method */

static exception_handler_t *load_exception_handlers(class_t *cl,
                                                    method_t *method,
                                                    uint8_t *code)
{
    class_file_t *cf;
    exception_handler_t *handlers;
    uint16_t index;
    uint32_t offset = method_is_synchronized(method) ? 1 : 0;

    cf = cf_open(cl->name);
    cf_seek(cf, method->data.offset, SEEK_SET);

    // Skip the code and the 'exception_table_length' field
    cf_seek(cf, method->code_length + 2, SEEK_CUR);

    // Load the exception handlers and resolve the relevant classes
    handlers = gc_malloc(sizeof(exception_handler_t)
                         * method->exception_table_length);

    for (size_t i = 0; i < method->exception_table_length; i++) {
        handlers[i].start_pc = cf_load_u2(cf) + offset;
        handlers[i].end_pc = cf_load_u2(cf) + offset;
        handlers[i].handler_pc = code + cf_load_u2(cf) + offset;
        index = cf_load_u2(cf);

        if (index == 0) {
            // Catch any exception
            handlers[i].catch_type =
                bcl_resolve_class_by_name(cl, "java/lang/Object");
        } else {
            // Catch only the specified type
            handlers[i].catch_type = resolve_class(cl, index);
        }
    }

    cf_close(cf);
    return handlers;
} // load_exception_handlers()


/*******************************************************************************
 * Field related functions                                                     *
 ******************************************************************************/

/** Loads the class fields
 * \param cl A pointer to a class_t structure
 * \param cf A pointer to a class file */

static void load_fields(class_t *cl, class_file_t *cf)
{
    char *name, *descriptor;
    field_manager_t *fm;
    field_attributes_t attr = { false, 0 };
    long field_offset;
    uint32_t count;
    uint16_t instance_count = 0;
    uint16_t static_count = 0;
    uint16_t access_flags, name_index, descriptor_index;

    count = cf_load_u2(cf);
    field_offset = cf_tell(cf);

    for (size_t i = 0; i < count; i++) {
        access_flags = cf_load_u2(cf) & FIELD_ACC_FLAGS_MASK;

        if (access_flags & ACC_STATIC) {
            static_count++;
        } else {
            instance_count++;
        }

        cf_seek(cf, 4, SEEK_CUR);
        load_field_attributes(cl, cf, &attr);
    }

    cf_seek(cf, field_offset, SEEK_SET);

    fm = fm_create(instance_count, static_count);

    for (size_t i = 0; i < count; i++) {
        access_flags = cf_load_u2(cf) & FIELD_ACC_FLAGS_MASK;
        name_index = cf_load_u2(cf);
        descriptor_index = cf_load_u2(cf);

        name = cp_get_string(cl->const_pool, name_index);
        descriptor = cp_get_string(cl->const_pool, descriptor_index);
        check_field_access_flags(access_flags, class_is_interface(cl));

        load_field_attributes(cl, cf, &attr);

        if (access_flags & ACC_STATIC) {
            if (attr.constant_value_found == true) {
                fm_add_static(fm, name, descriptor, access_flags,
                              cl->const_pool, attr.constant_value_index);
            } else {
                fm_add_static(fm, name, descriptor, access_flags, NULL, 0);
            }
        } else {
            fm_add_instance(fm, name, descriptor, access_flags);
        }
    }

    cl->field_manager = fm;
} // load_fields()

/**  Loads a field's attributes
 * \param cl A pointer to the class being loaded
 * \param cf A pointer to a class file
 * \param attr A pointer to a field_attributes_t structure
 *
 * The function stores the loaded attributes in the relevant fields of the
 * \a attr parameter */

static void load_field_attributes(class_t *cl, class_file_t *cf,
                                 field_attributes_t *attr)
{
    uint32_t attr_count, attr_length;
    uint16_t attr_name;
    long real_length;
    bool const_value_found = false;

    /* Load and check the attributes, the only attribute that a JVM is required
     * to recognize for a field is the ConstantValue */
    attr_count = cf_load_u2(cf);

    for (size_t i = 0; i < attr_count; i++) {
        attr_name = cf_load_u2(cf);
        attr_length = cf_load_u4(cf);
        real_length = cf_tell(cf);

        if (strcmp(cp_get_string(cl->const_pool, attr_name),
                   "ConstantValue") == 0)
        {
            if (const_value_found) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "More than one ConstantValue attribute found");
            }

            load_field_attribute_ConstantValue(cl, cf, attr);
            const_value_found = true;
        } else {
            // Unrecognized attribute, skip it
            cf_seek(cf, attr_length, SEEK_CUR);
        }

        real_length = cf_tell(cf) - real_length;

        if (real_length != attr_length) {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Actual length of an attribute is different from the "
                    "length provided in the class file");
        }
    }

    attr->constant_value_found = const_value_found;
} // load_field_attributes()

/** Loads the field's ConstantValue attribute
 * \param cl A pointer to the current class
 * \param cf A pointer to the class file
 * \param attr A pointer to a field_attributes_t structure used for storing the
 * attribute data */

static void load_field_attribute_ConstantValue(class_t *cl, class_file_t *cf,
                                               field_attributes_t *attr)
{
    attr->constant_value_index = cf_load_u2(cf);
} // load_attribute_ConstantValue()

/** Layout the instance fields
 * \param cl A pointer to the class structure */

static void layout_fields(class_t *cl)
{
    field_t *curr;
    field_iterator_t itr;

    uint32_t par_ref_n; // Parent's number of references
    uint32_t par_nref_size; // Parent's non-reference area size in bytes
    uint32_t new_ref_n; // New total number of references
    uint32_t new_nref_size; // New size of the non-reference area in bytes

    uint32_t ref_n; // Number of references
    uint32_t bit_size; // Number of bit-sized fields
    uint32_t byte_size; // Number of byte-sized fields
    uint32_t short_size; // Number of short-sized fields
    uint32_t int_size; // Number of int-sized fields
    uint32_t long_size; // Number of long-sized fields

    uint32_t ref_offset; // Offset of the current reference
    uint32_t bit_offset; // Offset of the current bit-sized field
    uint32_t byte_offset; // Offset of the current byte-sized field
    uint32_t short_offset; // Offset of the current short-sized field
    uint32_t int_offset; // Offset of the current int-sized field
    uint32_t long_offset; // Offset of the current long-sized field

    if (cl->parent != NULL) {
        par_ref_n = class_get_ref_n(cl->parent);
        par_nref_size = class_get_nref_size(cl->parent);
    } else {
        par_ref_n = 0;
        par_nref_size = 0;
    }

    ref_n = 0;
    bit_size = 0;
    byte_size = 0;
    short_size = 0;
    int_size = 0;
    long_size = 0;

    itr = field_itr(cl->field_manager);

    while (field_itr_has_next(itr)) {
        curr = field_itr_get_next(&itr);

        switch (curr->descriptor[0]) {
            case '[':
            case 'L':
                if (strcmp(curr->descriptor, "Ljelatine/VMPointer;") == 0) {
#if SIZEOF_VOID_P == 8
                    long_size += 8;
#else // SIZEOF_VOID_P == 4 || SIZEOF_VOID_P == 2
                    int_size += 4;
#endif // SIZEOF_VOID_P == 8
                } else {
                    ref_n++;
                }

                break;

            case 'B':
                byte_size++;
                break;

            case 'Z':
                bit_size++;
                break;

            case 'C':
            case 'S':
                short_size += 2;
                break;

            case 'I':
#if JEL_FP_SUPPORT
            case 'F':
#endif // JEL_FP_SUPPORT
                int_size += 4;
                break;

            case 'J':
#if JEL_FP_SUPPORT
            case 'D':
#endif // JEL_FP_SUPPORT
                long_size += 8;
                break;

            default:
                dbg_unreachable();
        }
    }

    /* Ok, now we've got all the fields, it's layout time!
     * Calculate the new size of the instance, round the size of the
     * non-reference area to the jword size */
    new_ref_n = par_ref_n + ref_n;

    if (long_size != 0) {
        par_nref_size = size_ceil(par_nref_size, sizeof(jword_t));
    } else if (int_size != 0) {
        par_nref_size = size_ceil(par_nref_size, 4);
    } else if (short_size != 0) {
        par_nref_size = size_ceil(par_nref_size, 2);
    }

    new_nref_size = par_nref_size + long_size + int_size + short_size
                    + byte_size + ((bit_size + 7) / 8);

    // And store them in the class structure
    cl->ref_n = new_ref_n;
    cl->nref_size = new_nref_size;

    // If the class exceeds the limits of the VM exit gracefully
    if (new_nref_size > 32767 - size_ceil(sizeof(header_t), sizeof(jword_t)))
    {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Number of non-reference fields exceed the VM limits");
    } else if (new_ref_n * sizeof(uintptr_t) > 32768) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Number of reference fields exceed the VM limits");
    }

    ref_offset = par_ref_n;
    long_offset = par_nref_size + size_ceil(sizeof(header_t), sizeof(jword_t));
    int_offset = long_offset + long_size;
    short_offset = int_offset + int_size;
    byte_offset = short_offset + short_size;
    bit_offset = (byte_offset + byte_size) << 3;

    itr = field_itr(cl->field_manager);

    while (field_itr_has_next(itr)) {
        curr = field_itr_get_next(&itr);

        switch (curr->descriptor[0]) {
            case '[':
            case 'L':
                if (strcmp(curr->descriptor, "Ljelatine/VMPointer;") == 0) {
#if SIZEOF_VOID_P == 8
                    curr->offset = long_offset;
                    long_offset += 8;
#else // SIZEOF_VOID_P == 4 || SIZEOF_VOID_P == 2
                    curr->offset = int_offset;
                    int_offset += 4;
#endif // SIZEOF_VOID_P == 8
                } else {
                    curr->offset = -((ref_offset + 1) * sizeof(uintptr_t));
                    ref_offset++;
                }
                break;

            case 'B':
                curr->offset = byte_offset;
                byte_offset++;
                break;

            case 'Z':
                curr->offset = bit_offset;
                bit_offset++;
                break;

            case 'C':
            case 'S':
                curr->offset = short_offset;
                short_offset += 2;
                break;

            case 'I':
#if JEL_FP_SUPPORT
            case 'F':
#endif // JEL_FP_SUPPORT
                curr->offset = int_offset;
                int_offset += 4;
                break;

            case 'J':
#if JEL_FP_SUPPORT
            case 'D':
#endif // JEL_FP_SUPPORT
                curr->offset = long_offset;
                long_offset += 8;
                break;

            default:
                dbg_unreachable();
        }
    }

    if (bit_offset > 32767) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Number of bit-sized fields exceeds the VM limits");
    }

    // HACK: The java.lang.ref.Reference class needs manual patching
    if (strcmp(cl->name, "java/lang/ref/Reference") == 0) {
        assert(strcmp(cl->field_manager->instance_fields[0].name, "referent")
               == 0);
        cl->field_manager->instance_fields[0].offset = sizeof(header_t);
        cl->ref_n = 0;
        cl->nref_size = sizeof(uintptr_t);
    }
} // layout_fields()

/*******************************************************************************
 * Bytecode resolution methods                                                 *
 ******************************************************************************/

/**  Resolves an instance field
 * \param src A pointer to the class requesting the resolution
 * \param index An index in the class' constant pool holding the
 * CONSTANT_Fieldref structure describing the field to be resolved
 * \returns A pointer to the field, throws an exception if the field cannot be
 * resolved or isn't found */

static field_t *resolve_instance_field(class_t *src, uint16_t index)
{
    const_pool_t *cp = src->const_pool;
    class_t *cl;
    field_t *field;
    char *name, *desc;
    uint16_t class_index;

    if (cp_get_tag(cp, index) == CONSTANT_Fieldref_resolved) {
        return cp_get_resolved_instance_field(cp, index);
    }

    class_index = cp_get_fieldref_class(cp, index);
    name = cp_get_fieldref_name(cp, index);
    desc = cp_get_fieldref_type(cp, index);

    cl = resolve_class(src, class_index);

    /* Look for the field in the current class and then in all its parent
     * classes until it is found or we reach java.lang.Object at which point the
     * resolution procedure fails */
    while (true) {
        field = fm_get_instance(cl->field_manager, name, desc);

        if (field != NULL) {
            break;
        } else {
            if (cl->id == JAVA_LANG_OBJECT) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Unable to resolve field, field not found");
            }

            cl = cl->parent;
        }
    }

    /* If we got here we actually found a field, let's check if we are allowed
     * to access it */

    if (field_is_private(field) && src != cl) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Trying to access a private field from an external class");
    } else if (field_is_protected(field)) {
        if (!((src == cl)
              || class_is_parent(cl, src)
              || same_package(cl, src)))
        {
            c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                    "Trying to access a protected field from a non-child class "
                    "in a different package");
        }
    } else if (!field_is_public(field) && !same_package(cl, src)) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Trying to access a package-visible field from a different "
                "package");
    }

    return field;
} // resolve_instance_field()

/**  Resolves a static field
 * \param src A pointer to the class requesting the resolution
 * \param index An index in the class' constant pool holding the
 * CONSTANT_Fieldref structure describing the field to be resolved
 * \returns A pointer to the field, throws an exception if the field cannot be
 * resolved or isn't found */

static static_field_t *resolve_static_field(class_t *src, uint16_t index)
{
    const_pool_t *cp = src->const_pool;
    class_t *cl, *interface;
    static_field_t *field;
    char *name, *desc;
    uint16_t class_index;
    interface_iterator_t itr;

    if (cp_get_tag(cp, index) == CONSTANT_Fieldref_resolved) {
        return cp_get_resolved_static_field(cp, index);
    }

    class_index = cp_get_fieldref_class(cp, index);
    name = cp_get_fieldref_name(cp, index);
    desc = cp_get_fieldref_type(cp, index);

    cl = resolve_class(src, class_index);
    initialize_class(thread_self(), cl);

    /* Look for the field in the current class and then in all its parent
     * classes until it is found or we reach java.lang.Object at which point the
     * resolution procedure fails */
    while (true) {
        field = fm_get_static(cl->field_manager, name, desc);

        if (field != NULL) {
            break;
        } else {
            if (cl->id == JAVA_LANG_OBJECT) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "Unable to resolve field, field not found");
            }

            itr = interface_itr(cl->interface_manager);

            while (interface_itr_has_next(itr)) {
                interface = interface_itr_get_next(&itr);
                field = fm_get_static(interface->field_manager, name, desc);

                if (field != NULL) {
                    break;
                }
            }

            if (field != NULL) {
                break;
            } else {
                cl = cl->parent;
            }
        }
    }

    /* If we got here we actually found a field, let's check if we are allowed
     * to access it */

    if (sfield_is_private(field) && src != cl) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Trying to access a private field from an external class");
    } else if (sfield_is_protected(field)
               && !(cl == src
                    || class_is_parent(cl, src)
                    || same_package(cl, src)))
    {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Trying to access a protected field from a non-child class of "
                "a different package");
    } else if (!sfield_is_public(field) && !same_package(cl, src)) {
        /* Field is package visible, if ACC_PUBLIC is set no check is needed */
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Trying to access a package-visible field from a different "
                "package");
    }

    cp_set_tag_and_data(cp, index, CONSTANT_Fieldref_resolved,
                        sfield_get_data_ptr(field));

    return field;
} // resolve_static_field()

/** Executes the actual linking of a method, implements the METHOD_LOAD field
 * \param cl The class owning the requesting method
 * \param method The method to be linked */

void bcl_link_method(class_t *cl, method_t *method)
{
    uint8_t *code;
    exception_handler_t *handlers;

    tm_lock();

    if (!method_is_linked(method)) {
        method_set_linked(method);

        // Load the bytecode and the exception handlers
        code = load_bytecode(cl, method);
        handlers = load_exception_handlers(cl, method, code);
        // Translate the method's bytecode and eventually do verification
        translate_bytecode(cl, method, code, handlers);

        method->data.handlers = handlers;

        if (method_is_main(method)) {
            // If this is the main method we have to initialize its class
            initialize_class(thread_self(), cp_get_class(method->cp));
        }

        method->code = code;
    }

    tm_unlock();
} // bcl_link_method()

/** Turns a non-typed GETSTATIC_PRELINK, PUTSTATIC_PRELINK, GETFIELD_PRELINK or
 * PUTFIELD_PRELINK opcode into its typed version
 * \param opcode The source opcode, either GETSTATIC_PRELINK, PUTSTATIC_PRELINK,
 * GETFIELD_PRELINK or PUTFIELD_PRELINK
 * \param descriptor The field descriptor
 * \returns The new opcode */

static uint8_t get_type_specific_opcode(uint8_t opcode, char *descriptor)
{
    switch (descriptor[0]) {
        case 'B':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_BYTE;
                case PUTSTATIC_PRELINK: return PUTSTATIC_BYTE;
                case GETFIELD_PRELINK:  return GETFIELD_BYTE;
                case PUTFIELD_PRELINK:  return PUTFIELD_BYTE;
                default:                dbg_unreachable();
            }

            break;

        case 'Z':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_BOOL;
                case PUTSTATIC_PRELINK: return PUTSTATIC_BOOL;
                case GETFIELD_PRELINK:  return GETFIELD_BOOL;
                case PUTFIELD_PRELINK:  return PUTFIELD_BOOL;
                default:                dbg_unreachable();
            }

            break;

        case 'C':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_CHAR;
                case PUTSTATIC_PRELINK: return PUTSTATIC_CHAR;
                case GETFIELD_PRELINK:  return GETFIELD_CHAR;
                case PUTFIELD_PRELINK:  return PUTFIELD_CHAR;
                default:                dbg_unreachable();
            }

            break;

        case 'S':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_SHORT;
                case PUTSTATIC_PRELINK: return PUTSTATIC_SHORT;
                case GETFIELD_PRELINK:  return GETFIELD_SHORT;
                case PUTFIELD_PRELINK:  return PUTFIELD_SHORT;
                default:                dbg_unreachable();
            }

            break;

        case 'I':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_INT;
                case PUTSTATIC_PRELINK: return PUTSTATIC_INT;
                case GETFIELD_PRELINK:  return GETFIELD_INT;
                case PUTFIELD_PRELINK:  return PUTFIELD_INT;
                default:                dbg_unreachable();
            }

            break;

#if JEL_FP_SUPPORT

        case 'F':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_FLOAT;
                case PUTSTATIC_PRELINK: return PUTSTATIC_FLOAT;
                case GETFIELD_PRELINK:  return GETFIELD_FLOAT;
                case PUTFIELD_PRELINK:  return PUTFIELD_FLOAT;
                default:                dbg_unreachable();
            }

            break;

#endif // JEL_FP_SUPPORT

        case 'J':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_LONG;
                case PUTSTATIC_PRELINK: return PUTSTATIC_LONG;
                case GETFIELD_PRELINK:  return GETFIELD_LONG;
                case PUTFIELD_PRELINK:  return PUTFIELD_LONG;
                default:                dbg_unreachable();
            }

            break;

#if JEL_FP_SUPPORT

        case 'D':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_DOUBLE;
                case PUTSTATIC_PRELINK: return PUTSTATIC_DOUBLE;
                case GETFIELD_PRELINK:  return GETFIELD_DOUBLE;
                case PUTFIELD_PRELINK:  return PUTFIELD_DOUBLE;
                default:                dbg_unreachable();
            }

            break;

#endif // JEL_FP_SUPPORT

        case '[':
        case 'L':
            switch (opcode) {
                case GETSTATIC_PRELINK: return GETSTATIC_REFERENCE;
                case PUTSTATIC_PRELINK: return PUTSTATIC_REFERENCE;
                case GETFIELD_PRELINK:  return GETFIELD_REFERENCE;
                case PUTFIELD_PRELINK:  return PUTFIELD_REFERENCE;
                default:                dbg_unreachable();
            }

            break;

            default:
                ;
    }

    dbg_unreachable();
    return 0;
} // get_type_specific_opcode()

/** Link a *_PRELINK opcode. The function behaviour changes depending on the
 * opcode however all that is needed for the opcode to be linked properly is
 * done in this function (for example class initialization). The function
 * returns the same program counter it receives, this is done to 'cast away the
 * constness' of the program counter
 * \param method The method holding the opcode to be processed
 * \param lpc The program-counter pointing to the opcode to be linked
 * \param opcode The opcode value, this is used at the contents of the memory
 * pointed by the program-counter may change if more than one thread tries to
 * link it at the same time
 * \returns The program counter pointing to the linked opcode */

const uint8_t *bcl_link_opcode(const method_t *method, const uint8_t *lpc,
                               internal_opcode_t opcode)
{
    ptrdiff_t offset = lpc - method->code;
    uint8_t *pc = method->code + offset;
    uint16_t index;
    const_pool_t *cp = method->cp;
    class_t *cl = cp_get_class(cp);
    class_t *init, *super_cl;
    static_field_t *sfield;
    field_t *field;

    tm_lock();

    /* If the contents of the pc have changed since we read it another thread
     * has already processed this opcode */

    if (*pc != opcode) {
        tm_unlock();
        return pc;
    }

    if (opcode == NEWARRAY_PRELINK) {
        index = *(pc + 1);
    } else {
        index = (*(pc + 1) << 8) | *(pc + 2);
    }

    switch (opcode) {
        case GETSTATIC_PRELINK:
        case PUTSTATIC_PRELINK:
            sfield = resolve_static_field(cl, index);
            opcode = get_type_specific_opcode(opcode, sfield->descriptor);
            store_int16_un(pc + 1, index);
            break;

        case GETFIELD_PRELINK:
        case PUTFIELD_PRELINK:
            field = resolve_instance_field(cl, index);
            opcode = get_type_specific_opcode(opcode, field->descriptor);
            store_int16_un(pc + 1, field->offset);
            break;

        case INVOKEVIRTUAL_PRELINK:
            method = resolve_method(cl, index, false);

            if (method_is_static(method)) {
                c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                        "INVOKEVIRTUAL invokes a static method");
            }

            if (method->name[0] == '<') {
                c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                        "INVOKEVIRTUAL invokes an instance or class "
                        "initializer");
            }

            opcode = INVOKEVIRTUAL;
            store_int16_un(pc + 1, method_create_packed_index(method));
            break;

        case INVOKESPECIAL_PRELINK:
            method = resolve_method(cl, index, false);

            if (method_is_static(method)) {
                c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                        "INVOKESPECIAL invokes a static method");
            }

            super_cl = cp_get_class(method->cp);

            if ((super_cl != cl)
                && class_is_parent(super_cl, cl)
                && class_is_super(cl)
                && !method_is_init(method))
            {
                opcode = INVOKESUPER;
                store_int16_un(pc + 1, method_create_packed_index(method));
            } else {
                /* In this case INVOKESPECIAL is used to invoke a constructor,
                 * the index in the constant pool is stored directly as the
                 * method index as it will be read directly from the constant
                 * pool instead of the class virtual table. */
                opcode = INVOKESPECIAL;
                store_int16_un(pc + 1, index);
            }

            break;

        case INVOKESTATIC_PRELINK:
            method = resolve_method(cl, index, false);

            if (!method_is_static(method)) {
                c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                        "INOVKESTATIC invokes a non-static method");
            }

            opcode = INVOKESTATIC;
            store_int16_un(pc + 1, index);
            break;

        case INVOKEINTERFACE_PRELINK:
            method = resolve_method(cl, index, true);

            if (method_is_static(method)) {
                c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                        "INVOKEINTERFACE invokes a static method");
            }

            if (method->name[0] == '<') {
                c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                        "INVOKEINTERFACE invokes an instance or class "
                        "initializer");
            }

            opcode = INVOKEINTERFACE;
            store_int16_un(pc + 1, method_create_packed_index(method));
            break;

        case NEW_PRELINK:
            init = resolve_class(cl, index);
            initialize_class(thread_self(), init);

            if (class_is_abstract(init)) {
                c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                        "NEW tries to instantiate an abstract class");
            }

            store_int16_un(pc + 1, index);

#if JEL_FINALIZER
            opcode = class_has_finalizer(init) ? NEW_FINALIZER : NEW;
#else
            opcode = NEW;
#endif // JEL_FINALIZER
            break;

        case NEWARRAY_PRELINK:
            bcl_resolve_class_by_name(NULL, class_bootstrap_name(index));
            opcode = NEWARRAY;
            break;

        case ANEWARRAY_PRELINK:
        {
            size_t len;
            char *str, *temp;

            /* The ANEWARRAY opcode gives a symbolic reference to the type of
             * the array elements, we replace it with a symbolic reference to
             * the array type */

            if (cp_get_tag(cp, index) == CONSTANT_Class_resolved) {
                str = cp_get_resolved_class(cp, index)->name;
            } else {
                str = cp_get_class_name(cp, index);
            }

            len = strlen(str);

            if (str[0] == '[')  {
                temp = gc_malloc(len + 2);
                temp[0] = '[';
                memcpy(temp + 1, str, len + 1);
            } else {
                temp = gc_malloc(len + 4);
                temp[0] = '[';
                temp[1] = 'L';
                memcpy(temp + 2, str, len);
                temp[len + 2] = ';';
                temp[len + 3] = 0;
            }

            cl = bcl_resolve_class_by_name(cl, temp);
            gc_free(temp);
            opcode = ANEWARRAY;
            store_int16_un(pc + 1, class_get_id(cl));
        }
            break;

        case CHECKCAST_PRELINK:
            resolve_class(cl, index);
            opcode = CHECKCAST;
            store_int16_un(pc + 1, index);
            break;

        case INSTANCEOF_PRELINK:
            resolve_class(cl, index);
            opcode = INSTANCEOF;
            store_int16_un(pc + 1, index);
            break;

        case MULTIANEWARRAY_PRELINK:
        {
            uint8_t dimensions = *(pc + 3);
            cl = resolve_class(cl, index);

            if (class_get_dimensions(cl) < dimensions) {
                c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                        "MULTIANEWARRAY specifies an erroneous number of "
                        "dimensions");
            }

            opcode = MULTIANEWARRAY;
            store_int16_un(pc + 1, index);
        }
            break;

        default:
            dbg_unreachable();
    }

    *pc = opcode;
    tm_unlock();
    return pc;
} // bcl_link_opcode()
