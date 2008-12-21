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

/** \file memory.c
 * Garbage-collector and allocator implementation */

#include "wrappers.h"

#include "class.h"
#include "header.h"
#include "loader.h"
#include "memory.h"
#include "thread.h"
#include "util.h"
#include "vm.h"

#include "java_lang_ref_WeakReference.h"

/******************************************************************************
 * Type definitions                                                           *
 ******************************************************************************/

/** An object in the finalizer list */

struct finalizable_t {
    struct finalizable_t *next; /**< Next object in the list */
    uintptr_t ref; /**< Reference to the finalized object */
};

/** Typedef for the ::struct finalizable_t */
typedef struct finalizable_t finalizable_t;

/** Represents a free chunk of memory */

struct chunk_t {
    struct chunk_t *next; /**<  Next chunk in the list */
    size_t size; /**<  Size of the chunk in bytes */
};

/** Typedef for the ::struct chunk_t */
typedef struct chunk_t chunk_t;

/** Size in words of a chunk_t structure */
#define CHUNK_SIZE ((sizeof(chunk_t) + sizeof(jword_t) - 1) / sizeof(jword_t))

/** Minimum chunk size in words */
#define MIN_BIN_SIZE (CHUNK_SIZE)

/** Maximum small chunk size in words */
#define MAX_BIN_SIZE (17)

/** Defines the number of entries in the chunk-bin */
#define BIN_ENTRIES (MAX_BIN_SIZE - MIN_BIN_SIZE + 1)

/** Initial fraction of the provided heap to use */
#define HEAP_INIT_FRACTION (16)

/** Number of slots to add to the temporary root stack when growing it */
#define TEMP_ROOT_INC (4)

/** Structure representing the heap object
 *
 * Note that the last field not only points to the last word in the Java-heap
 * but also to a fake object-header used as a sentinel by the garbage-collector
 */

struct heap_t {
    bool collect; ///< True if the collector can run
    size_t size; ///< Current heap size (in jwords)
    size_t max_size; ///< Maximum heap size (in jwords)

    void *memory; ///< Generic heap used by the VM
    jword_t *start; ///< First word of the heap
    jword_t *end; ///< First word after the end of the heap

    uint8_t *bitmap; ///< Memory bitmap

    java_lang_ref_WeakReference_t *weakref_list; ///< Weak references list

    chunk_t *last_chunk; ///< Last chunk, adjacent to the heap boundary
    chunk_t *large_bin; ///< Bin used for storing large free chunks
    chunk_t *bin[BIN_ENTRIES]; ///< Bin used for storing small free chunks

#if JEL_FINALIZER
    finalizable_t *finalizable; ///< List of finalizer objects still alive
    finalizable_t *finalizing; ///< Objects waiting to be finalized
#   if JEL_THREAD_POSIX
    pthread_cond_t pthread_cond; ///< POSIX threads wait condition
#   elif JEL_THREAD_PTH
    pth_cond_t pth_cond; ///< GNU/Pth wait condition
#   endif
#endif // JEL_FINALIZER
};

/** Typedef for the ::struct heap_t */
typedef struct heap_t heap_t;

/******************************************************************************
 * Local functions prototypes                                                 *
 ******************************************************************************/

static uintptr_t gc_alloc(size_t);
static void purge_bin( void );
static void mark( void );
static void mark_finalizable( void );
static void sweep( void );
static void purge_weakref_list( void );
static void grow(size_t);

// Bitmap management functions

static inline void bitmap_set(uint8_t *, size_t);
static inline void bitmap_clear(uint8_t *, size_t);
static inline bool bitmap_get(uint8_t *, size_t);

// Chunk management functions

static chunk_t *get_chunk(size_t);
static void put_chunk(chunk_t *);

#ifndef NDEBUG
void print_bin( void );
#endif // !NDEBUG

/******************************************************************************
 * Local declarations                                                         *
 ******************************************************************************/

/** The virtual machine heap */
static heap_t heap;

/******************************************************************************
 * Heap implementation                                                        *
 ******************************************************************************/

/** Initializes the heap, throws an exception upon failure
 *
 * Note that if a heap of less than 32 KiB is requested the function will
 * default to 32 KiB anyway as mandated by the CLDC configuration. If you want
 * to use a heap smaller than 32 KiB you have to explicitely change this
 * function, do it at your own risk. All the pointers in the heap structure are
 * explicitely aligned to 4 or 8-bytes boundaries depending on the target
 * machine
 *
 * \param size Size in bytes of the unified heap */

void gc_init(size_t size)
{
    chunk_t *new_chunk;
    void *unified_heap;
    uint8_t *bitmap;
    size_t init_size;
    size_t heap_size; // Actual size of the heap in jwords

    // Grow the heap size if it is smaller than the canonical 32KiB CLDC heap
    size = (size < 32768) ? 32768 : size;

    /* Truncate the size to a multiple of 4 or 8 bytes, so we won't bother about
     * alignment problems, we assume that malloc() will already return a
     * properly aligned block */
    size = size_ceil_round(size, sizeof(jword_t));
    heap_size = (size * sizeof(jword_t) * 8) / ((sizeof(jword_t) * 8) + 1);
    heap_size = heap_size / sizeof(jword_t);
    init_size = heap_size / HEAP_INIT_FRACTION;

    unified_heap = malloc(size);
    memset(unified_heap, 0, size);
    bitmap = ((uint8_t *) unified_heap) + (heap_size * sizeof(jword_t));

    if (unified_heap == NULL) {
        dbg_error("Out of memory, cannot allocate the unified heap.");
        vm_fail();
    }

    // Initialize the heap structure
    heap.size = init_size;
    heap.max_size = heap_size;
    heap.memory = unified_heap;
    heap.start = (jword_t *) unified_heap;
    heap.end = ((jword_t *) unified_heap) + init_size;
    heap.weakref_list = NULL;
    heap.bitmap = bitmap;
    heap.last_chunk = NULL;
    heap.large_bin = NULL;

    memset(heap.bin, 0, sizeof(chunk_t *) * BIN_ENTRIES);

    // Initialize the first free chunk
    new_chunk = (chunk_t *) unified_heap;
    new_chunk->size = init_size;
    put_chunk(new_chunk);

    /* Disable the collector, we will re-enable it once all the other global
     * structures have been initialized */
    heap.collect = false;

#if JEL_FINALIZER
    heap.finalizable = NULL;
    heap.finalizing = NULL;
#   if JEL_THREAD_POSIX
    pthread_cond_init(&heap.pthread_cond, NULL);
#   elif JEL_THREAD_PTH
    pth_cond_init(&heap.pth_cond);
#   endif
#endif // JEL_FINALIZER
} // gc_init()

/** Disposes the unified heap */

void gc_teardown( void )
{
#if JEL_FINALIZER && JEL_THREAD_POSIX
    pthread_cond_destroy(&heap.pthread_cond);
#endif // JEL_FINALIZER && JEL_THREAD_POSIX
    free(heap.memory);
} // gc_teardown()

/** Enables or disables the collector
 * \param enable new status of the collector */

void gc_enable(bool enable)
{
    heap.collect = enable;
} // gc_enable()

/** Allocates a managed object of the specified class
 *
 * If not enough memory is avaible an exception is thrown, note that the
 * returned reference points to the object header, not to the first word of the
 * object.
 *
 * \param cl A pointer to the object class
 * \returns A reference to the newly created object, throws an exception upon
 * failure */

uintptr_t gc_new(class_t *cl)
{
    uintptr_t ptr;
    header_t *header; // Pointer to the new object's header
    size_t size; // Size of the object (in words) to be allocated
    uint32_t ref_n;

    tm_lock();

#if SIZEOF_VOID_P == (SIZEOF_JWORD_T / 2)
    ref_n = size_ceil_div(class_get_ref_n(cl), 2);
#else
    ref_n = class_get_ref_n(cl);
#endif

    size = ref_n + HEADER_SIZE
           + size_ceil_div(class_get_nref_size(cl), sizeof(jword_t));

    // A class without fields could cause trouble...
    size = (size < MIN_BIN_SIZE) ? MIN_BIN_SIZE : size;

    ptr = gc_alloc(size);
    ptr += ref_n * sizeof(uintptr_t); // Point to the header
    bitmap_set(heap.bitmap, (jword_t *) ptr - heap.start);
    header = (header_t *) ptr;
    *header = header_create_java(cl);

#if JEL_PRINT
    if (opts_get_print_memory()) {
        fprintf(stderr, "NEW PTR: %p SIZE: %zu\n", header, size);
    }
#endif // JEL_PRINT

    tm_unlock();

    return ptr;
} // gc_new()

/** Creates a new array of the specified type
 * \param type The basic array class type requested
 * \param count The number of elements
 * \returns A reference to the newly created array */

uintptr_t gc_new_array_nonref(array_type_t type, int32_t count)
{
    class_t *cl = bcl_get_class_by_id(type);
    size_t size = count * array_elem_size(type);
    array_t *array;
    uintptr_t ptr;

    if (type == T_BOOLEAN) {
        size = size_ceil_div(size, 8);
    }

    size = size_ceil_div(size + sizeof(array_t), sizeof(jword_t));
    ptr = gc_alloc(size);
    bitmap_set(heap.bitmap, (jword_t *) ptr - heap.start);
    array = (array_t *) ptr;
    array->header = header_create_java(cl);
    array->length = count;

    return ptr;
} // gc_new_array_nonref()

/** Creates a new array of references
 * \param cl A pointer to the corresponding class
 * \param count The number of elements
 * \returns A pointer to the newly created array */

uintptr_t gc_new_array_ref(class_t *cl, int32_t count)
{
    ref_array_t *array;
    uintptr_t ptr;
    size_t size;

#if SIZEOF_VOID_P == (SIZEOF_JWORD_T / 2)
    size = size_ceil_div(count, 2);
#else
    size = count;
#endif // SIZEOF_VOID_P == (SIZEOF_JWORD_T / 2)

    ptr = gc_alloc(REF_ARRAY_SIZE + size);
    ptr += size * sizeof(uintptr_t);
    bitmap_set(heap.bitmap, (jword_t *) ptr - heap.start);
    array = (ref_array_t *) ptr;
    array->header = header_create_java(cl);
    array->length = count;

    return (uintptr_t) array;
} // gc_new_array_ref()

/** Helper function used for creating multi-dimensional arrays
 * \param cl A pointer to the corresponding class
 * \param dimensions The number of dimensions to be created
 * \param counts A pointer to the element counts
 * \param mark True if the newly allocated array should be pushed among the
 * temporary roots, false otherwise
 * \returns A pointer to the newly created array */

uintptr_t gc_new_multiarray(class_t *cl, uint8_t dimensions, jword_t *counts,
                            bool mark)
{
    /* This array is initialized so that the array types correspond to the
     * basic types of the array elements */
    const uint8_t type_conv[] = {
        T_BYTE, T_BOOLEAN, T_CHAR, T_SHORT, T_INT, T_FLOAT, T_LONG, T_DOUBLE
    };

    uintptr_t *references;
    uintptr_t ref;
    int32_t count = *((int32_t *) counts);
    int32_t i;

    if (count == 0) {
        return JNULL;
    }

    if (dimensions == 1) {
        if (cl->elem_type != PT_REFERENCE) {
            return gc_new_array_nonref(type_conv[cl->elem_type], count);
        } else {
            return gc_new_array_ref(cl, count);
        }
    } else {
        ref = gc_new_array_ref(cl, count);
        references = array_ref_get_data((array_t *) ref);

        if (mark) {
            thread_push_root(&ref);
        }

        for (i = 0; i < count; i++) {
            references[-i] = gc_new_multiarray(cl->elem_class, dimensions - 1,
                                               counts + 1, false);
        }

        if (mark) {
            thread_pop_root();
        }

        return ref;
    }
} // gc_new_multiarray()

#if JEL_FINALIZER

/** Registers a newly created finalizable object
 * \param ref A reference to the object to be registered */

void gc_register_finalizable(uintptr_t ref)
{
    finalizable_t *fin;

    thread_push_root(&ref);
    tm_lock();
    fin = gc_malloc(sizeof(finalizable_t));
    fin->ref = ref;
    fin->next = heap.finalizable;
    heap.finalizable = fin;
    thread_pop_root();

    tm_unlock();
} // gc_register_finalizable()

/** Returns the first finalizable object in the list
 * \returns A reference to the first object waiting to be finalized, JNULL if
 * none are pending */

uintptr_t gc_get_finalizable( void )
{
    uintptr_t ref = JNULL;
    finalizable_t *fin;

    tm_lock();
    fin = heap.finalizing;

    if (heap.finalizing == NULL) {
#if JEL_THREAD_POSIX
        pthread_mutex_t *vm_lock = tm_get_lock();
        pthread_cond_wait(&heap.pthread_cond, vm_lock);
#elif JEL_THREAD_PTH
        pth_mutex_t *vm_lock = tm_get_lock() ;
        pth_cond_await(&heap.pth_cond, vm_lock, NULL);
#endif
    }

    assert(heap.finalizing != NULL);
    fin = heap.finalizing;
    heap.finalizing = fin->next;
    ref = fin->ref;
    gc_free(fin);

    tm_unlock();
    return ref;
} // gc_get_finalizable()

#endif // JEL_FINALIZER

/** Adds a new object to the weak reference list
 * \param ref A reference to the object to be added to the weak reference list
 */

void gc_register_weak_ref(java_lang_ref_WeakReference_t *ref)
{
    tm_lock();
    ref->next = heap.weakref_list;
    heap.weakref_list = ref;
    tm_unlock();
} // gc_register_weak_ref()

/** Launches the garbage collector on the provided heap structure */

void gc_collect( void )
{
    tm_lock();

#if JEL_PRINT
    if (opts_get_print_memory()) {
       fprintf(stderr, "GARBAGE COLLECTION\n");
    }
#endif // JEL_PRINT

    if (heap.collect != false) {
        tm_stop_the_world(); // Wait for all threads to stop
        purge_bin(); // Turn free areas into fake dead objects

        // Mark garbage collected objects
        mark();
        mark_finalizable();

        // Mark and purge of non-garbage collected structures
        purge_weakref_list();
        jsm_purge();
        tm_purge();

        sweep();
    }

    tm_unlock();
} // gc_collect()

/** Helper function used for implementing Runtime.freeMemory(), returns
 * the amount of free memory in bytes
 * \returns The amount of free memory in bytes */

size_t gc_free_memory( void )
{
    chunk_t *chunk;
    size_t size;
    uint32_t i;

    tm_lock();
    size = 0;

    for (i = 0; i < BIN_ENTRIES; i++) {
        chunk = heap.bin[i];

        while (chunk != NULL) {
            size += chunk->size;
            chunk = chunk->next;
        }
    }

    chunk = heap.large_bin;

    while (chunk != NULL) {
        size += chunk->size;
        chunk = chunk->next;
    }

    tm_unlock();
    return size * sizeof(jword_t);
} // gc_free_memory()

/** Helper function used for implementing Runtime.totalMemory(), returns
 * the amount of memory available to the VM in bytes
 * \returns The amount of memory available to the VM in bytes */

size_t gc_total_memory( void )
{
    return heap.size * sizeof(jword_t);
} // gc_total_memory()

/** Check if a reference is a pointer and recursively mark it if it is
 * \param ref A reference to a Java object */

void gc_mark_potential(uintptr_t ref)
{
    /* If it is not aligned to a JWORD_SIZE boundary then this is definetely
     * not a reference to a Java object */
    if (ref & (sizeof(jword_t) - 1)) {
        return;
    }

    // If it exceeds the heap bounds this is not a reference to a Java object
    if ((ref < (uintptr_t) heap.start)
            || (ref >= (uintptr_t) heap.end))
    {
        return;
    }

    /* If the corresponding entry in the bitmap is not marked then this is not
     * a reference to a Java (or C) object */
    if (! bitmap_get(heap.bitmap, ((jword_t *) ref) - heap.start)) {
        return;
    }

    /* If we got here then we have something that may be a pointer and actually
     * points to something, let's mark it */
    gc_mark_reference(ref);
} // gc_mark_potential()

/** Marks the object pointed by ref and recursively all the objects
 * pointed by the references it contains
 * \param ref A reference to a Java object */

#if !JEL_POINTER_REVERSAL

void gc_mark_reference(uintptr_t ref)
{
    class_t *cl;
    header_t *header, *new_header;
    uintptr_t *references;
    uintptr_t new_ref;
    uint32_t ref_n;

    if (ref == JNULL) {
        return;
    }

    header = (header_t *) ref;

    if (header_is_cobject(header)) {
        return;
    }

    cl = header_get_class(header);

    if (header_is_marked(header)) {
        return;
    } else {
        header_set_mark(header);
    }

    if (class_is_array(cl) && (cl->elem_type == PT_REFERENCE)) {
        ref_n = array_get_ref_n((array_t *) header);
    } else {
        ref_n = class_get_ref_n(cl);
    }

    references = ((uintptr_t *) header) - ref_n;

    for (size_t i = 0; i < ref_n; i++) {
        new_ref = references[i];

        if (new_ref == JNULL) {
            continue;
        } else {
            new_header = (header_t *) new_ref;

            if (header_is_marked(new_header)) {
                continue;
            } else {
                gc_mark_reference(new_ref);
            }
        }
    }
} // gc_mark_reference()

#else

void gc_mark_reference(uintptr_t ref)
{
    class_t *cl;
    uintptr_t prev, curr, tmp;
    header_t *header, *tmp_header;
    jword_t count;
    uint32_t ref_n;
    bool ref_array;

    if (ref == JNULL) {
        return;
    }

    header = (header_t *) ref;

    if (header_is_cobject(header) || header_is_marked(header)) {
        return;
    }

    curr = ref;
    prev = JNULL;

    header = (header_t *) curr;
    header_create_gc_counter(header);

    while (1) {
        cl = bcl_get_class_by_id(header_get_class_index(header));

        if (class_is_array(cl) && (cl->elem_type == PT_REFERENCE)) {
            ref_array = true;
            ref_n = ((array_t *) header)->length;
        } else {
            ref_array = false;
            ref_n = class_get_ref_n(cl);
        }

        count = header_get_count(header, ref_array);

        if (count < ref_n) {
            // We're not done yet with the current object
            tmp = *((uintptr_t *) curr - count - 1);

            if (tmp == JNULL) {
                // Null reference, skip to the next object
                header_set_count(header, count + 1, ref_array);
                continue;
            }

            tmp_header = (header_t *) tmp;

            if (! header_is_marked(tmp_header)) {
                /* This object is not marked, mark it, push the previous object
                 * and set this one as the current */
                *((uintptr_t *) curr - count - 1) = prev;
                prev = curr;
                curr = tmp;
                header = tmp_header;
                header_create_gc_counter(header);
                continue;
            } else {
                // This object has already been marked, skip to the next one
                header_set_count(header, count + 1, ref_array);
                continue;
            }
        } else {
            // We're done with the current object, pop the previous
            tmp = curr;
            curr = prev;
            header = (header_t *) curr;

            // We returned to the root pointer, return to the caller
            if (curr == JNULL) {
                return;
            }

            cl = bcl_get_class_by_id(header_get_class_index(header));

            if (class_is_array(cl) && (cl->elem_type == PT_REFERENCE)) {
                ref_array = true;
            } else {
                ref_array = false;
            }

            count = header_get_count(header, ref_array);
            prev = *((uintptr_t *) curr - count - 1);
            *((uintptr_t *) curr - count - 1) = tmp;
            header_set_count(header, count + 1, ref_array);
        }
    }
} // gc_mark_reference()

#endif // !JEL_POINTER_REVERSAL

/** Allocates a chunk of the specified size (in jwords) from the bin, calls the
 * garbage collector if necessary and throws an exception if unable to satisfy
 * the request. The returned memory will have already been cleared
 * \param size The chunk size in jwords
 * \returns A chunk of memory carved from the garbage collected heap */

static uintptr_t gc_alloc(size_t size)
{
    chunk_t *chunk = get_chunk(size);
    chunk_t *new_chunk;
    uintptr_t ptr;

    if (chunk == NULL) {
        // get_chunk() failed... collect and retry
        gc_collect();
        chunk = get_chunk(size);

        if (chunk == NULL) {
            grow(size);
            chunk = get_chunk(size);

            if (chunk == NULL) {
                dbg_error("Out of memory. Try giving the VM a larger heap "
                          "with the --size <size_in_bytes> option.");
                vm_fail();
            }
        }
    }

    ptr = (uintptr_t) chunk;

    if (chunk->size - size >= CHUNK_SIZE) {
        new_chunk = (chunk_t *) (ptr + size * sizeof(jword_t));
        new_chunk->size = chunk->size - size;
        put_chunk(new_chunk);
    } else {
        // We're wasting memory here
        memset((jword_t *) (ptr + size * sizeof(jword_t)), 0,
               sizeof(jword_t) * (chunk->size - size));
    }

    memset((jword_t *) ptr, 0, size * sizeof(jword_t));
    return ptr;
} // gc_alloc()

/** Purges the free chunks bin
 *
 * This function also clears the free chunks left in the bin so that the
 * garbage collector can reclaim the space they hold and aggregate them with
 * other potentially free chunks surrounding them */

static void purge_bin( void ) {
    chunk_t *temp;
    char *free_space;
    jword_t *start = heap.start;
    uint8_t *bitmap = heap.bitmap;
    size_t free_size;
    header_t header;

    // Empty the fixed-size bins

    for (size_t i = 0; i < BIN_ENTRIES; i++) {
        temp = heap.bin[i];
        heap.bin[i] = NULL;

        while (temp != NULL) {
            free_space = (char *) temp;
            free_size = temp->size;
            temp = temp->next;

            // Create a fake, fred C object so that the GC will reclaim it
            header = header_create_c(free_size - HEADER_SIZE);
            // HACK: We use memcpy() to a char * to avoid aliasing problems
            memcpy(free_space, &header, sizeof(header_t));
            bitmap_set(bitmap, (jword_t *) free_space - start);
        }
    }

    // Empty the large bin
    temp = heap.large_bin;
    heap.large_bin = NULL;

    while (temp != NULL) {
        free_space = (char *) temp;
        free_size = temp->size;
        temp = temp->next;

        // Create a fake, fred C object so that the GC will reclaim it
        header = header_create_c(free_size - HEADER_SIZE);
        // HACK: We use memcpy() to a char * to avoid aliasing problems
        memcpy(free_space, &header, sizeof(header_t));
        bitmap_set(bitmap, (jword_t *) free_space - start);
    }

    heap.last_chunk = NULL; // Clear the last chunk
} // purge_bin()

/** Executes the 'mark' phase of the garbage collector
 *
 * The mark phase consists in scanning the root objects (threads and stacks
 * basically) for references to objects in the heap. Each of these object is
 * then marked as live and its references explored in turn and so on until all
 * the live objects are found and marked. The recursive descent algorythm uses
 * the pointer-reversal trick to avoid using an implicit/explicit stack,
 * the gc is thus completely self-contained and cannot fail due to a stack
 * overflow */

static void mark( void )
{
    bcl_mark();
    jsm_mark();
    tm_mark();
} // mark()

/** After the mark phase executed checks the finalizable obejcts. The ones which
 * haven't been marked and thus are dead are 'resurrected' by marking them and
 * moved to the list of objects to be finalized. Once the finalizer thread has
 * processed them they will be lost forever */

static void mark_finalizable( void )
{
#if JEL_FINALIZER
    finalizable_t *curr = heap.finalizable;
    finalizable_t *prev = NULL;
    finalizable_t *next;

    while (curr != NULL) {
        if (!header_is_marked((header_t *) (curr->ref))) {
            // Add the object to the queue of the objects being finalized
            next = curr->next;
            curr->next = heap.finalizing;
            heap.finalizing = curr;

            if (curr->next == NULL) {
#if JEL_THREAD_POSIX
                pthread_cond_signal(&heap.pthread_cond);
#elif JEL_THREAD_PTH
                pth_cond_notify(&heap.pth_cond, FALSE);
#endif
            }

            if (prev == NULL) {
                heap.finalizable = next;
            } else {
                prev->next = next;
            }

            curr = next;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    // Resurrect the objects which are being finalized
    curr = heap.finalizing;

    while (curr != NULL) {
        gc_mark_reference(curr->ref);
        curr = curr->next;
    }
#endif // JEL_FINALIZER
} // mark_finalizable()

/** Scans the heap for live objects and reclaims dead ones replenishing
 * the free list */

static void sweep( void )
{
    class_t *cl;
    header_t *header;
    size_t nref_words, ref_n, fred_space;
    size_t in_use = 0;
    size_t reclaimed = 0;
    size_t wasted = 0;
    jword_t *dead = NULL;
    jword_t *end = heap.end;
    jword_t *scan = heap.start;
    jword_t *start = heap.start;
    chunk_t *chunk;
    uint8_t *bitmap = heap.bitmap;
    bool is_java;
#if JEL_POINTER_REVERSAL
    uint32_t id;
#endif // JEL_POINTER_REVERSAL

    while (scan < end) {
        if (! bitmap_get(bitmap, scan - start)) {
            scan++;
            continue;
        }

        header = (header_t *) scan; // We found a header

        if (header_is_cobject(header)) {
            is_java = false;
            ref_n = 0;
            nref_words = header_get_size(header) / sizeof(jword_t);
        } else {
            is_java = true;

#if JEL_POINTER_REVERSAL
            if (header_is_marked(header)) {
                id = header_get_class_index(header);
                header_restore(header, bcl_get_class_by_id(id));
            }
#endif // JEL_POINTER_REVERSAL

            cl = header_get_class(header);

            if (class_is_array(cl)) {
                nref_words = array_get_nref_words((array_t *) header);
                ref_n = array_get_ref_n((array_t *) header);
            } else {
                nref_words = class_get_nref_words(cl);
                ref_n = class_get_ref_n(cl);
            }
        }

#if SIZEOF_VOID_P == (SIZEOF_JWORD_T / 2)
        ref_n = size_ceil_div(ref_n, 2);
#endif // SIZEOF_VOID_P == (SIZEOF_JWORD_T / 2)

        if (header_is_marked(header)) {
            if (is_java) {
                header_clear_mark(header);
            }

            if (dead != NULL) {
                // Create a new free chunk if possible
                fred_space = (scan - ref_n) - dead;

                if (fred_space >= CHUNK_SIZE) {
                    chunk = (chunk_t *) dead;
                    chunk->size = fred_space;
                    put_chunk(chunk);
                    reclaimed += fred_space * sizeof(jword_t);
                } else {
                    // Clear the wasted space
                    memset(dead, 0, fred_space * sizeof(jword_t));
                    wasted += fred_space * sizeof(jword_t);
                }

                dead = NULL;
            }

            // Skip to the next object
            scan += HEADER_SIZE + nref_words;
            in_use += (ref_n + HEADER_SIZE + nref_words) * sizeof(jword_t);
        } else {
            // If this is a dead object remove it from the bitmap
            bitmap_clear(bitmap, scan - start);

            if (dead == NULL) {
                dead = scan - ref_n;
            }

            scan += HEADER_SIZE + nref_words;
        }
    }

    // Add the last free chunk
    if (dead != NULL) {
        // Create a new free chunk if possible
        fred_space = end - dead;

        if (fred_space >= CHUNK_SIZE) {
            chunk = (chunk_t *) dead;
            chunk->size = fred_space;
            put_chunk(chunk);
            reclaimed += fred_space * sizeof(jword_t);
        } else {
            // Clear the wasted space
            memset(dead, 0, fred_space * sizeof(jword_t));
            wasted += fred_space * sizeof(jword_t);
        }
    }

    // Check if we have fred enough memory, otherwise we grow the heap
    if (reclaimed < (in_use >> 1)) {
        grow(((in_use >> 1) - reclaimed) / sizeof(jword_t));
    }

#if JEL_PRINT
    if (opts_get_print_memory()) {
       fprintf(stderr,
               "GARBAGE COLLECTION in_use = %zu reclaimed = %zu wasted = %zu\n",
               in_use, reclaimed, wasted);
    }
#endif // JEL_PRINT
} // sweep()

/** Tries to grow the heap by the specified amount
 * \param size The number of jwords requested */

void grow(size_t size)
{
    chunk_t *chunk;

    if ((heap.size + size) > heap.max_size) {
        size = heap.max_size - heap.size;
    }

    chunk = heap.last_chunk;

    if ((chunk != NULL) && (chunk->size > MAX_BIN_SIZE)) {
        chunk->size += size;
        heap.size += size;
        heap.end += size;
    } else {
        if (chunk == NULL) {
            // The last chunk will be replaced by a new one
            heap.last_chunk = NULL;
        }

        if (size < CHUNK_SIZE) {
            return;
        }

        chunk = (chunk_t *) heap.end;
        chunk->size = size;
        heap.size += size;
        heap.end += size;

        put_chunk(chunk);
    }
} // grow()

/** Allocates a chunk of memory for holding C objects, throws an
 * exception upon failure, the returned memory has already been zeroed
 *
 * Contrary to the behaviour of the ANSI library malloc() function an allocation
 * of 0 bytes is considered an error, if debugging is on this will trigger an
 * assertion. The allocation size is rounded to a multiple of 4 or 8 bytes
 * depending on the target
 *
 * \param size The size in bytes of the chunk to be allocated
 * \returns A pointer to the newly allocated memory */

void *gc_malloc(size_t size)
{
    assert(size != 0);

    uintptr_t ptr;
    header_t *header;

    size = size_ceil_div(size, sizeof(jword_t)) + HEADER_SIZE; // Round the size
    tm_lock();
    ptr = gc_alloc(size);
    header = (header_t *) ptr;
    *header = header_create_c(size - HEADER_SIZE);
    header_set_mark(header);
    bitmap_set(heap.bitmap, (jword_t *) ptr - heap.start);

#if JEL_PRINT
    if (opts_get_print_memory()) {
       fprintf(stderr, "MALLOC PTR: %p SIZE: %zu bytes\n",
               (void *) (ptr + sizeof(jword_t)),
               (size - HEADER_SIZE) * sizeof(jword_t));
    }
#endif // JEL_PRINT

    tm_unlock();

    return (void *) (ptr + HEADER_SIZE * sizeof(jword_t));
} // gc_malloc()

/** Frees a previously allocated C object.
 * gc_free() doesn't acquire the VM lock as it does not access shared structures
 * \param ptr A pointer to the object to be fred */

void gc_free(void *ptr)
{
    assert(ptr != NULL);

    header_t *header = (header_t *) ((uintptr_t) ptr
                                     - HEADER_SIZE * sizeof(jword_t));

#ifdef JEL_PRINT
    if (opts_get_print_memory()) {
       fprintf(stderr, "FREE PTR: %p SIZE = %zu bytes\n",
               ptr, header_get_size(header) * sizeof(jword_t));
    }
#endif // JEL_VERBOSE_GC

    header_clear_mark(header);
} // gc_free()

/** Purges the weak reference list by cleaning all the weak references
 * which point to weakly reachable (or non-reachable) objects and removing all
 * the non-reachable weak references */

static void purge_weakref_list( void )
{
    java_lang_ref_WeakReference_t *curr, *prev, list;
    header_t *referent;

    prev = &list;
    prev->next = NULL;
    curr = heap.weakref_list;

    while (curr != NULL) {
        if (header_is_marked(&(curr->header))) {
            /*
             * This weak reference is marked, if the referent is marked too
             * we can leave it alone, otherwise the referent is only weakly
             * reacheable so we have to clear the weak reference pointing to it
             */
            referent = (header_t *) curr->referent;

            if (! header_is_marked(referent)) {
                curr->referent = JNULL;
            }

            prev = curr;
        } else {
            // This reference is not marked, remove it from the queue
            prev->next = curr->next;
        }

        curr = curr->next;
    }

    heap.weakref_list = list.next;
} // heap_purge_weakref_list()

/** Set an entry in the bitmap to 1
 * \param bitmap A pointer to the memory bitmap
 * \param offset An offset from the base address */

static inline void bitmap_set(uint8_t *bitmap, size_t offset)
{
    bitmap[offset >> 3] |= 1 << (offset & 0x7);
} // bitmap_set()

/** Clear an entry in the bitmap
 * \param bitmap A pointer to the memory bitmap
 * \param offset An offset from the base address */

static inline void bitmap_clear(uint8_t *bitmap, size_t offset)
{
    bitmap[offset >> 3] &= ~(1 << (offset & 0x7));
} // bitmap_clear()

/** Read an entry of the bitmap
 * \param bitmap A pointer to the memory bitmap
 * \param offset An offset from the base address */

static inline bool bitmap_get(uint8_t *bitmap, size_t offset)
{
    uint8_t value;

    value = bitmap[offset >> 3] >> (offset & 0x7);

    return (value & 1);
} // bitmap_get()

/** Pulls a chunk from the bin
 * \param size The minimum size (in words) requested
 * \returns A chunk if one large enough is avaible, otherwise NULL */

static chunk_t *get_chunk(size_t size)
{
    assert(size >= MIN_BIN_SIZE);

    chunk_t *best = NULL;
    chunk_t *curr, *prev;
    uint32_t id;

    if (size <= MAX_BIN_SIZE) {
        // Try finding an exact match
        id = size - MIN_BIN_SIZE;

        if (heap.bin[id] != NULL) {
            // We got an exact match, remove it and return it
            best = heap.bin[id];
            heap.bin[id] = best->next;

            if (best == heap.last_chunk) {
                heap.last_chunk = NULL;
            }

            return best;
        }

        // Try finding a larger chunk
        if (heap.large_bin != NULL) {
            // We found a larger block, return it
            best = heap.large_bin;
            heap.large_bin = best->next;

            if (best == heap.last_chunk) {
                heap.last_chunk = NULL;
            }

            return best;
        }

        /* Try finding a fitting chunk in the small bin even if it
         * means potentially wasting memory */
        id++;

        while (id < BIN_ENTRIES) {
            if (heap.bin[id] != NULL) {
                best = heap.bin[id];
                heap.bin[id] = best->next;

                if (best == heap.last_chunk) {
                    heap.last_chunk = NULL;
                }

                return best;
            } else {
                id++;
            }
        }
    } else {
        // Find an appropriate block in the large chunk list
        prev = NULL;
        curr = heap.large_bin;

        while (curr != NULL) {
            if (curr->size >= size) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    heap.large_bin = curr->next;
                }

                if (curr == heap.last_chunk) {
                    heap.last_chunk = NULL;
                }

                return curr;
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
    }

    return NULL;
} // get_chunk()

/** Puts a chunk in the bin
 * \param chunk A pointer to the chunk to be put in the bin */

static void put_chunk(chunk_t *chunk)
{
    uint32_t id;
    size_t size = chunk->size;

    assert(size >= MIN_BIN_SIZE);

    if (size <= MAX_BIN_SIZE) {
        id = size - MIN_BIN_SIZE;
        chunk->next = heap.bin[id];
        heap.bin[id] = chunk;
    } else {
        chunk->next = heap.large_bin;
        heap.large_bin = chunk;
    }

    if (heap.last_chunk == NULL) {
        if ((((jword_t *) chunk) + chunk->size) == heap.end)
            heap.last_chunk = chunk;
    }
} // put_chunk()

#ifndef NDEBUG

/** Prints information on the free chunk's bin, used for debug purposes */

void print_bin( void )
{
    chunk_t *temp;

    fprintf(stderr, "print_bin()\n");

    for (size_t i = 0, j = 0; i < BIN_ENTRIES; i++) {
        temp = heap.bin[i];

        while (temp != NULL) {
            j++;
            temp = temp->next;
        }

       fprintf(stderr, "heap->bin[size = %zu words] = %zu\n", i + 2, j);
    }

    fprintf(stderr, "heap->large_bin = \n");
    temp = heap.large_bin;

    while (temp != NULL) {
        fprintf(stderr, "\tsize = %zu\n", temp->size);
        temp = temp->next;
    }

   fprintf(stderr, "\n");
} // print_bin()

#endif // !NDEBUG
