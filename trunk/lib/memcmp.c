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
#   include <config.h>
#endif

#if ! HAVE_MEMCMP

int memcmp(const void *s1, const void *s2, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (s1[i] != s2[i])
            return s1[i] - s2[i];
    }

    return 0;
}

#endif
