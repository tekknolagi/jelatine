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

/** \file loader.h
 * Bootstrap class-loader definition */

/** \def JELATINE_LOADER_H
 * loader.h inclusion macro */

#ifndef JELATINE_LOADER_H
#   define JELATINE_LOADER_H (1)

#include "wrappers.h"

#if JEL_JARFILE_SUPPORT
#   include <zzip/lib.h>
#endif // JEL_JARFILE_SUPPORT

#include "class.h"
#include "util.h"

/******************************************************************************
 * Loader interface                                                           *
 ******************************************************************************/

extern void bcl_init(const char *, const char*);
extern void bcl_teardown( void );
extern class_t *bcl_get_class_by_id(uint32_t);
extern void bcl_mark( void );
extern bool bcl_is_assignable(class_t *, class_t *);
extern class_t *bcl_preload_class(const char *);
extern class_t *bcl_resolve_class_by_name(class_t *, const char *);
extern class_t *bcl_find_class_by_name(const char *);
extern void bcl_link_method(class_t *, method_t *);
extern const uint8_t *bcl_link_opcode(const method_t *, const uint8_t *,
                                      internal_opcode_t);

#if JEL_JARFILE_SUPPORT
extern ZZIP_FILE *bcl_get_resource(const char *);
#endif // JEL_JARFILE_SUPPORT

#endif // !JELATINE_LOADER_H
