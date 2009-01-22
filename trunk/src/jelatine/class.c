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

/** \file class.c
 * Class related functions implementation */

#include "wrappers.h"

#include "array.h"
#include "class.h"
#include "memory.h"
#include "method.h"
#include "header.h"
#include "util.h"

/******************************************************************************
 * Local declarations                                                         *
 ******************************************************************************/

/** \var bootstrap_classes
 * Names of the different bootstrap classes */

static char *bootstrap_classes[] =
{
    "",
    "",
    "",
    "",
    "[Z",
    "[C",
    "[F",
    "[D",
    "[B",
    "[S",
    "[I",
    "[J",
    "java/lang/Object",
    "java/lang/Class",
    "java/lang/String",
    "java/lang/Thread",
    "java/lang/Throwable",
    "java/lang/Error",
    "java/lang/NoClassDefFoundError",
    "java/lang/VirtualMachineError",
    "java/lang/Exception",
    "java/lang/RuntimeException",
    "java/lang/IllegalMonitorStateException",
    "java/lang/ArithmeticException",
    "java/lang/ArrayStoreException",
    "java/lang/IndexOutOfBoundsException",
    "java/lang/ArrayIndexOutOfBoundsException",
    "java/lang/NullPointerException",
    "java/lang/NegativeArraySizeException",
    "java/lang/ClassCastException",
    "java/lang/ClassNotFoundException",
    "java/lang/ref/Reference",
    "java/lang/ref/WeakReference"
};

/******************************************************************************
 * Interface manager implementation                                           *
 ******************************************************************************/

/** Creates an interface manager
 * \returns A new interface manager */

interface_manager_t *im_create( void )
{
    interface_manager_t *im = gc_malloc(sizeof(interface_manager_t));

    im->ll = ll_create();
    im->entries = 0;
    im->interfaces = NULL;

    return im;
} // im_create()

/** Adds a new interface to the manager
 * \param im A pointer to the interface manager
 * \param cl A pointer to an interface */

void im_add(interface_manager_t *im, class_t *cl)
{
    ll_add_unique(im->ll, cl);
} // im_add()

/** Flattens the interface manager internal representation turning it from a
 * linked list to an array
 * \param im A pointer to the interface manager */

void im_flatten(interface_manager_t *im)
{
    uint32_t i = 0;
    uint32_t count = ll_size(im->ll);
    ll_iterator_t itr;
    class_t **interfaces;

    itr = ll_itr(im->ll);
    interfaces = gc_malloc(sizeof(class_t *) * count);

    while (ll_itr_has_next(itr)) {
        interfaces[i] = (class_t *) ll_itr_get_next(&itr);
        i++;
    }

    im->interfaces = interfaces;
    im->entries = count;
    ll_dispose(im->ll);
    im->ll = NULL;
} // im_flatten()

/** Checks if an interface is present in the interface manager
 * \param im A pointer to the interface manager
 * \param interface A pointer to a class
 * \returns true if the specified interface is present, false otherwise */

bool im_is_present(interface_manager_t *im, class_t *interface)
{
    for (size_t i = 0; i < im->entries; i++) {
        if (im->interfaces[i] == interface) {
            return true;
        }
    }

    return false;
} // im_is_present()

/******************************************************************************
 * Class implementation                                                       *
 ******************************************************************************/

/** Checks if a class is a bootstrap class and returns its id
 * \param name The class' name
 * \returns An id if the class is a bootstrap class or -1 */

int32_t class_bootstrap_id(const char *name)
{
    for (size_t i = 0; i < JELATINE_PREDEFINED_CLASSES_N; i++) {
        if (strcmp(bootstrap_classes[i], name) == 0) {
            return i;
        }
    }

    return -1;
} // class_bootstrap_id()

/** Return the name corresponding to the class id of a bootstrap class
 * \param id The class id
 * \returns The class name if the id corresponds to a bootstrap class */

char *class_bootstrap_name(uint32_t id)
{
    assert(id < JELATINE_PREDEFINED_CLASSES_N);

    return bootstrap_classes[id];
} // class_bootstrap_name()

/** Returns true if \a child is a child class of \a parent
 * \param parent The potentially parent class
 * \param child The potentially child class
 * \returns true if \a child inherits from \a parent, false otherwise  */

bool class_is_parent(class_t *parent, class_t *child)
{
    do {
        if (parent == child->parent) {
            return true;
        }

        child = child->parent; // Climb up in the hierarchy
    } while (child != NULL);

    return false;
} // class_is_parent()

/** This function unloads the class initializer method
 * \param cl A pointer to a class_t structure */

void class_purge_initializer(class_t *cl)
{
    method_purge(mm_get(cl->method_manager, "<clinit>", "()V"));
} // class_purge_initializer()

