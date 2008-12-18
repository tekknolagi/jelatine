/***************************************************************************
 *   Copyright (C) 2005, 2006 by Gabriele Svelto                           *
 *   gabriele.svelto@gmail.com                                             *
 *                                                                         *
 *   This file is part of Jelatine.                                        *
 *                                                                         *
 *   Jelatine is free software; you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Jelatine is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Jelatine; if not, write to the Free Software Foundation,   *
 *   Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA        *
 ***************************************************************************/

#if HAVE_CONFIG_H
#    include <config.h>
#endif

#undef malloc

#include <sys/types.h>

void *malloc();

/* Allocate an N-byte block of memory from the heap. If N is zero, allocate a
 * 1-byte block. */

void *rpl_malloc(size_t n)
{
    if (n == 0)
        n = 1;
    
    return malloc(n);
} 
