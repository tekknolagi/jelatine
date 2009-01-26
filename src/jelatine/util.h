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

/** \file util.h
 * Utility functions declarations */

/** \def JELATINE_UTIL_H
 * util.h inclusion macro */

#ifndef JELATINE_UTIL_H
#   define JELATINE_UTIL_H (1)

#include "wrappers.h"

/******************************************************************************
 * Debugging facilites                                                        *
 ******************************************************************************/

/** \def dbg_printf
 * Prints a debug message on the standard error stream if debugging is enabled
 */

/** \def dbg_abort
 * Call abort() only if debugging is enabled */

/** \def dbg_error
 * Prints an error message and then abort execution when debugging is enabled */

/** \def dbg_warning
 * Prints a warning message if debugging is enabled */

/** \def dbg_unreachable
 * If reached at runtime prints an error message and aborts execution when
 * debugging is enabled */

#ifndef NDEBUG

extern void dbg_printf_internal(const char *, int, const char *, ...);
#define dbg_printf( ... ) dbg_printf_internal(__FILE__, __LINE__, __VA_ARGS__)

extern void dbg_error_internal(const char *, int, const char *, ...);
#define dbg_error( ... ) dbg_error_internal(__FILE__, __LINE__, __VA_ARGS__)

extern void dbg_unreachable_internal(const char *, int);
#define dbg_unreachable() dbg_unreachable_internal(__FILE__, __LINE__)

extern void dbg_warning_internal(const char *, int, const char *, ...);
#define dbg_warning( ... ) dbg_warning_internal(__FILE__, __LINE__, __VA_ARGS__)

#define dbg_abort() abort()

#else

#define dbg_printf( ... )

#define dbg_error( ... ) \
do { \
    fprintf(stderr, "ERROR: "); \
    fprintf(stderr, __VA_ARGS__); \
    fputc('\n', stderr); \
} while (0) \

#define dbg_warning( ... )
#define dbg_unreachable()
#define dbg_abort()

#endif // !NDEBUG

/******************************************************************************
 * Utility functions                                                          *
 ******************************************************************************/

extern void sort_asc_uint16_ptrs(uint16_t *, void **, uint32_t);

/** Divides x by y rounding towards infinity
 * \param x The dividend
 * \param y The divisor
 * \returns x / y rounded towards infinity */

static inline size_t size_div_inf(size_t x, size_t y)
{
    return (x + y - 1) / y;
} // size_ceil_div()

/** Rounds x towards the smallest number larger than x and divisible by y
 * \param x The dividend
 * \param y The divisor
 * \returns the smallest number larger than x and divisible by y */

static inline size_t size_ceil(size_t x, size_t y)
{
    return ((x + y - 1) / y) * y;
} // size_ceil()

/** Rounds x towards the largest number smaller than x and divisible by y
 * \param x The dividend
 * \param y The divisor
 * \returns the largest number smaller than x and divisible by y */

static inline size_t size_floor(size_t x, size_t y)
{
    return (x / y) * y;
} // size_floor()

/** Returns the maximum value between \a x and \a y
 * \param x The first value
 * \param y The second value
 * \returns The maximum value between \a x and \a y */

static inline size_t size_max(size_t x, size_t y)
{
    return (x > y) ? x : y;
} // size_max()

/** Loads a 16-bit integer from a potentially unaligned location
 * \param src A potentially unaligned pointer to a 16-bit integer
 * \returns A 16-bit integer */

static inline int16_t load_int16_un_internal(const char *src)
{
    int16_t res;

    memcpy(&res, src, sizeof(int16_t));
    return res;
} // load_int16_un_internal()

/** Wrapper for load_int16_un_internal()
 * \param src Same as for load_int16_un_internal()
 * \returns Same as for load_int16_un_internal() */
#define load_int16_un(src) load_int16_un_internal((const char *) (src))

/** Loads a 16-bit unsigned integer from a potentially unaligned location
 * \param src A potentially unaligned pointer to a 16-bit integer
 * \returns A 16-bit unsigned integer */

static inline uint16_t load_uint16_un_internal(const char *src)
{
    uint16_t res;

    memcpy(&res, src, sizeof(uint16_t));
    return res;
} // load_uint16_un_internal()

/** Wrapper for load_uint16_un_internal()
 * \param src Same as for load_uint16_un_internal()
 * \returns Same as for load_uint16_un_internal() */
#define load_uint16_un(src) load_uint16_un_internal((const char *) (src))

/** Stores a 16-bit integer in a potentially unaligned location
 * \param dst A potentially unaligned pointer to a 16-bit integer
 * \param val A 16-bit integer value */

static inline void store_int16_un_internal(char *dst, int16_t val)
{
    memcpy(dst, &val, sizeof(int16_t));
} // store_int16_un_internal()

/** Wrapper for store_int16_un_internal()
 * \param dst Same as for store_int16_un_internal()
 * \param val Same as for store_int16_un_internal() */
#define store_int16_un(dst, val) \
        store_int16_un_internal((char *) (dst), (val))

/** Loads a 32-bit integer from a potentially unaligned location
 * \param src A potentially unaligned pointer to a 32-bit integer
 * \returns A 32-bit unsigned integer */

static inline int32_t load_int32_un_internal(const char *src)
{
    uint32_t res;

    memcpy(&res, src, sizeof(int32_t));
    return res;
} // load_int32_un_internal()

/** Wrapper for load_int32_un_internal()
 * \param src Same as for load_int32_un_internal()
 * \returns Same as for load_int32_un_internal() */
#define load_int32_un(src) load_int32_un_internal((const char *) (src))

/** Stores a 32-bit integer in a potentially unaligned location
 * \param dst A potentially unaligned pointer to a 32-bit integer
 * \param val A 32-bit integer value */

static inline void store_int32_un_internal(char *dst, int32_t val)
{
    memcpy(dst, &val, sizeof(int32_t));
} // store_int32_un_internal()

/** Wrapper for store_int32_un_internal()
 * \param dst Same as for store_int32_un_internal()
 * \param val Same as for store_int32_un_internal() */
#define store_int32_un(dst, val) \
        store_int32_un_internal((char *) (dst), (val))

/******************************************************************************
 * Exception handling                                                         *
 ******************************************************************************/

/** 'try' clause of a C pseudo-exception block. Pseudo-exceptions can be thrown
 * inside this block by using c_throw() */

#define c_try \
{                                                       \
    int __exc__;                                        \
    jmp_buf __buf__;                                    \
    jmp_buf *__prev__ = thread_self()->c_exception.buf; \
    thread_self()->c_exception.buf = &__buf__;          \
    thread_self()->c_exception.data = 0;                \
    __exc__ = setjmp(__buf__);                          \
    if (__exc__ != 0) {                                 \
        thread_self()->c_exception.data = __exc__;      \
    } else

/** 'catch' clause of a C pseudo-exception block. The variable \a exception
 * must be an integer which will hold the caught exception
 * \param exc An integer holding the exception if caught */

#define c_catch(exc) \
    thread_self()->c_exception.buf = __prev__; \
}                                              \
(exc) = thread_self()->c_exception.data; \
if (exc == 0) {                          \
} else

/** \def c_throw
 * Wrapper used around c_throw_dbg()
 * \param exc The exception number / type */

#if !NDEBUG
extern void c_throw_dbg(const char *, int, int, const char *, ...)
                        ATTRIBUTE_NORETURN;
#   define c_throw(exc, ...) c_throw_dbg(__FILE__, __LINE__, exc, __VA_ARGS__)
#else
extern void c_throw(int, const char *, ...) ATTRIBUTE_NORETURN;
#endif // !NDEBUG

extern void c_clear_exception( void );
extern void c_print_exception(int);

/** Rethrows an exception, must be called inside a c_catch clause
 * \param exc The exception */

#define c_rethrow(exc) longjmp(*(thread_self()->c_exception.buf), (exc))

/******************************************************************************
 * Non-failing wrappers for ANSI C stdio.h functions                          *
 ******************************************************************************/

extern FILE *efopen(const char *, const char *);
extern void efclose(FILE *);
extern void efseek(FILE *, long, int);
extern long eftell(FILE *);
extern void efread(void *, size_t, size_t, FILE *);

/******************************************************************************
 * Linked lists                                                               *
 ******************************************************************************/

/** Represents an element in a linked list */

struct ll_elem_t {
    struct ll_elem_t *next; ///< Points to the next element in the list
    void *data; ///< Points to the data associated with the element
};

/** Typedef for the ::struct ll_elem_t */
typedef struct ll_elem_t ll_elem_t;

/** Represents a linked list structure */

struct linked_list_t {
    ll_elem_t *head; ///< Points to the first element in the list
    ll_elem_t *tail; ///< Points to the last element in the list
    size_t count; ///< The number of elements inserted in the list
};

/** Typedef for the ::struct linked_list_t */
typedef struct linked_list_t linked_list_t;

linked_list_t *ll_create( void );
void ll_dispose(linked_list_t *);
void ll_empty(linked_list_t *);
void ll_prepend(linked_list_t *, void *);
void ll_append(linked_list_t *, void *);
void ll_add_unique(linked_list_t *, void *);
void *ll_get_first(linked_list_t *);
void *ll_peek_first(const linked_list_t *);
size_t ll_size(const linked_list_t *);

/******************************************************************************
 * Linked list iterator                                                       *
 ******************************************************************************/

/** Linked list iterator */

struct ll_iterator_t {
    ll_elem_t *next; ///< Next element in the list
};

/** Typedef for the ::struct ll_iterator_t type */
typedef struct ll_iterator_t ll_iterator_t;

/** Returns an iterator from a linked list, the behaviour of the iterator is
 * defined only if no other elements are added to the list while the iterator is
 * being used
 * \param ll A pointer to a linked list
 * \returns A new iterator for the list */

static inline ll_iterator_t ll_itr(linked_list_t *ll)
{
    ll_iterator_t itr;

    itr.next = ll->head;

    return itr;
} // ll_itr()

/** Returns true if there are more elements in the linked list, false otherwise
 * \param itr A linked list iterator
 * \returns true if another element is available in the linked list, false
 * otherwise */

static inline bool ll_itr_has_next(ll_iterator_t itr)
{
    if (itr.next != NULL) {
        return true;
    } else {
        return false;
    }
} // ll_itr_has_next()

/** Returns the next element in a linked list
 * \param itr A linked list iterator
 * \returns The data pointed by the next element */

static inline void *ll_itr_get_next(ll_iterator_t *itr)
{
    void *ptr;

    assert(itr->next != NULL);
    ptr = itr->next->data;
    itr->next = itr->next->next;

    return ptr;
} // ll_itr_get_next()

/******************************************************************************
 * Time related functionality                                                 *
 ******************************************************************************/

extern struct timespec get_time_with_offset(uint64_t ms, uint32_t nanos);

#endif // !JELATINE_UTIL_H
