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

/** \file native.c
 * Native methods' implementation */

#include "wrappers.h"

#include "jstring.h"
#include "kni.h"
#include "loader.h"
#include "native.h"
#include "thread.h"
#include "utf8_string.h"
#include "util.h"
#include "vm.h"

#include "java_lang_String.h"
#include "java_lang_Thread.h"

#if JEL_JARFILE_SUPPORT
#   include "jelatine_VMResourceStream.h"
#endif // JEL_JARFILE_SUPPORT

/******************************************************************************
 * Type definitions                                                           *
 ******************************************************************************/

/** Describes a native method */

struct native_method_desc_t {
    const char *class_name; ///< Name of the class
    const char *name; ///< Name of the method
    const char *descriptor; ///< Java descriptor of the method
    void *func; ///< Pointer to the function
};

/** Typedef for ::struct native_method_desc_t */
typedef struct native_method_desc_t native_method_desc_t;

/******************************************************************************
 * Local function prototypes                                                  *
 ******************************************************************************/

// java.lang.Class methods
static KNI_RETURNTYPE_OBJECT java_lang_Class_forName( void );
static KNI_RETURNTYPE_OBJECT java_lang_Class_newInstance( void );
static KNI_RETURNTYPE_BOOLEAN java_lang_Class_isInstance( void );
static KNI_RETURNTYPE_BOOLEAN java_lang_Class_isAssignableFrom( void );

#if JEL_FP_SUPPORT
// java.lang.Double methods
static KNI_RETURNTYPE_OBJECT java_lang_Double_toString( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Double_parseDouble( void );
static KNI_RETURNTYPE_LONG java_lang_Double_doubleToLongBits( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Double_longBitsToDouble( void );
#endif // JEL_FP_SUPPORT

#if JEL_FP_SUPPORT
// java.lang.Float methods
static KNI_RETURNTYPE_OBJECT java_lang_Float_toString( void );
static KNI_RETURNTYPE_INT java_lang_Float_floatToIntBits( void );
static KNI_RETURNTYPE_FLOAT java_lang_Float_intBitsToFloat( void );
#endif // JEL_FP_SUPPORT

#if JEL_FP_SUPPORT
// java.lang.Math methods
static KNI_RETURNTYPE_DOUBLE java_lang_Math_sin( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Math_cos( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Math_tan( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Math_sqrt( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Math_ceil( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Math_floor( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Math_log( void );
static KNI_RETURNTYPE_DOUBLE java_lang_Math_exp( void );
#endif // JEL_FP_SUPPORT

// java.lang.Object methods
static KNI_RETURNTYPE_OBJECT java_lang_Object_getClass( void );
static KNI_RETURNTYPE_VOID java_lang_Object_notify( void );
static KNI_RETURNTYPE_VOID java_lang_Object_notifyAll( void );
static KNI_RETURNTYPE_VOID java_lang_Object__wait( void );

// java.lang.Runtime methods
static KNI_RETURNTYPE_VOID java_lang_Runtime_exit( void );
static KNI_RETURNTYPE_LONG java_lang_Runtime_freeMemory( void );
static KNI_RETURNTYPE_LONG java_lang_Runtime_totalMemory( void );
static KNI_RETURNTYPE_VOID java_lang_Runtime_gc( void );

// java.lang.String methods
static KNI_RETURNTYPE_OBJECT java_lang_String_intern( void );

// java.lang.System methods
static KNI_RETURNTYPE_LONG java_lang_System_currentTimeMillis( void );
static KNI_RETURNTYPE_VOID java_lang_System_arraycopy( void );
static KNI_RETURNTYPE_INT java_lang_System_identityHashCode( void );

// java.lang.Throwable methods
static KNI_RETURNTYPE_VOID java_lang_Throwable_printStackTrace( void );

// java.lang.ref.WeakReference methods
static KNI_RETURNTYPE_VOID java_lang_ref_WeakReference_addToWeakReferenceList( void );

// jelatine.VMFinalizer methods
#if JEL_FINALIZER
static KNI_RETURNTYPE_OBJECT jelatine_VMFinalizer_getNextObject( void );
#endif // !JEL_THREAD_NONE

// jelatine.VMOutputStream methods
static KNI_RETURNTYPE_VOID jelatine_VMOutputStream_write_to_stderr( void );
static KNI_RETURNTYPE_VOID jelatine_VMOutputStream_write_to_stdout( void );

// jelatine.VMResourceStream methods
#if JEL_JARFILE_SUPPORT
static KNI_RETURNTYPE_BOOLEAN jelatine_VMResourceStream_open( void );
static KNI_RETURNTYPE_INT jelatine_VMResourceStream_read( void );
static KNI_RETURNTYPE_VOID jelatine_VMResourceStream_finalize( void );
#endif // JEL_JARFILE_SUPPORT

// jelatine.VMThread methods
static KNI_RETURNTYPE_OBJECT jelatine_VMThread_currentThread( void );
static KNI_RETURNTYPE_VOID jelatine_VMThread_yield( void );
static KNI_RETURNTYPE_VOID jelatine_VMThread_sleep( void );
static KNI_RETURNTYPE_OBJECT jelatine_VMThread_start( void );
static KNI_RETURNTYPE_INT jelatine_VMThread_activeCount( void );
static KNI_RETURNTYPE_VOID jelatine_VMThread_join( void );

/** Names and descriptions of the native methods */

native_method_desc_t native_desc[] = {
    // java.lang.Class methods
    {
        "java/lang/Class",
        "forName",
        "(Ljava/lang/String;)Ljava/lang/Class;",
        java_lang_Class_forName
    },
    {
        "java/lang/Class",
        "newInstance",
        "()Ljava/lang/Object;",
        java_lang_Class_newInstance
    },
    {
        "java/lang/Class",
        "isInstance",
        "(Ljava/lang/Object;)Z",
        java_lang_Class_isInstance
    },
    {
        "java/lang/Class",
        "isAssignableFrom",
        "(Ljava/lang/Class;)Z",
        java_lang_Class_isAssignableFrom
    },

    // java.lang.Double methods
#if JEL_FP_SUPPORT
    {
        "java/lang/Double",
        "toString",
        "(D)Ljava/lang/String;",
        java_lang_Double_toString
    },
    {
        "java/lang/Double",
        "parseDouble",
        "(Ljava/lang/String;)D",
        java_lang_Double_parseDouble
    },
    {
        "java/lang/Double",
        "doubleToLongBits",
        "(D)J",
        java_lang_Double_doubleToLongBits
    },
    {
        "java/lang/Double",
        "longBitsToDouble",
        "(J)D",
        java_lang_Double_longBitsToDouble
    },
#endif // JEL_FP_SUPPORT

    // java.lang.Float methods
#if JEL_FP_SUPPORT
    {
        "java/lang/Float",
        "toString",
        "(F)Ljava/lang/String;",
        java_lang_Float_toString
    },
    {
        "java/lang/Float",
        "floatToIntBits",
        "(F)I",
        java_lang_Float_floatToIntBits
    },
    {
        "java/lang/Float",
        "intBitsToFloat",
        "(I)F",
        java_lang_Float_intBitsToFloat
    },
#endif // JEL_FP_SUPPORT

    // java.lang.Math methods
#if JEL_FP_SUPPORT
    {
        "java/lang/Math",
        "sin",
        "(D)D",
        java_lang_Math_sin
    },
    {
        "java/lang/Math",
        "cos",
        "(D)D",
        java_lang_Math_cos
    },
    {
        "java/lang/Math",
        "tan",
        "(D)D",
        java_lang_Math_tan
    },
    {
        "java/lang/Math",
        "sqrt",
        "(D)D",
        java_lang_Math_sqrt
    },
    {
        "java/lang/Math",
        "ceil",
        "(D)D",
        java_lang_Math_ceil
    },
    {
        "java/lang/Math",
        "floor",
        "(D)D",
        java_lang_Math_floor
    },
    {
        "java/lang/Math",
        "log",
        "(D)D",
        java_lang_Math_log
    },
    {
        "java/lang/Math",
        "exp",
        "(D)D",
        java_lang_Math_exp
    },
#endif // JEL_FP_SUPPORT

    // java.lang.Object methods
    {
        "java/lang/Object",
        "getClass",
        "()Ljava/lang/Class;",
        java_lang_Object_getClass
    },
    {
        "java/lang/Object",
        "notify",
        "()V",
        java_lang_Object_notify
    },
    {
        "java/lang/Object",
        "notifyAll",
        "()V",
        java_lang_Object_notifyAll
    },
    {
        "java/lang/Object",
        "_wait",
        "(JI)V",
        java_lang_Object__wait
    },

    // java.lang.Runtime methods
    {
        "java/lang/Runtime",
        "exit",
        "(I)V",
        java_lang_Runtime_exit
    },
    {
        "java/lang/Runtime",
        "freeMemory",
        "()J",
        java_lang_Runtime_freeMemory
    },
    {
        "java/lang/Runtime",
        "totalMemory",
        "()J",
        java_lang_Runtime_totalMemory
    },
    {
        "java/lang/Runtime",
        "gc",
        "()V",
        java_lang_Runtime_gc
    },

    // java.lang.String methods
    {
        "java/lang/String",
        "intern",
        "()Ljava/lang/String;",
        java_lang_String_intern
    },

    // java.lang.System methods
    {
        "java/lang/System",
        "currentTimeMillis",
        "()J",
        java_lang_System_currentTimeMillis
    },
    {
        "java/lang/System",
        "arraycopy",
        "(Ljava/lang/Object;ILjava/lang/Object;II)V",
        java_lang_System_arraycopy
    },
    {
        "java/lang/System",
        "identityHashCode",
        "(Ljava/lang/Object;)I",
        java_lang_System_identityHashCode
    },

    // java.lang.Throwable methods
    {
        "java/lang/Throwable",
        "printStackTrace",
        "()V",
        java_lang_Throwable_printStackTrace
    },

    // java.lang.refWeakReference methods
    {
        "java/lang/ref/WeakReference",
        "addToWeakReferenceList",
        "(Ljava/lang/ref/WeakReference;)V",
        java_lang_ref_WeakReference_addToWeakReferenceList
    },


    // jelatine.VMFinalizer methods
#if JEL_FINALIZER
    {
        "jelatine/VMFinalizer",
        "getNextObject",
        "()Ljelatine/VMFinalizer;",
        jelatine_VMFinalizer_getNextObject
    },
#endif // JEL_FINALIZER

    // jelatine.VMOutputStream methods
    {
        "jelatine/VMOutputStream",
        "write_to_stderr",
        "(B)V",
        jelatine_VMOutputStream_write_to_stderr
    },
    {
        "jelatine/VMOutputStream",
        "write_to_stdout",
        "(B)V",
        jelatine_VMOutputStream_write_to_stdout
    },

    // jelatine.VMResourceStream methods
#if JEL_JARFILE_SUPPORT
    {
        "jelatine/VMResourceStream",
        "open",
        "()Z",
        jelatine_VMResourceStream_open
    },
    {
        "jelatine/VMResourceStream",
        "read",
        "()I",
        jelatine_VMResourceStream_read
    },
    {
        "jelatine/VMResourceStream",
        "finalize",
        "()V",
        jelatine_VMResourceStream_finalize
    },
#endif // JEL_JARFILE_SUPPORT

    // jelatine.VMThread methods
    {
        "jelatine/VMThread",
        "currentThread",
        "()Ljava/lang/Thread;",
        jelatine_VMThread_currentThread
    },
    {
        "jelatine/VMThread",
        "yield",
        "()V",
        jelatine_VMThread_yield
    },
    {
        "jelatine/VMThread",
        "sleep",
        "(J)V",
        jelatine_VMThread_sleep
    },
    {
        "jelatine/VMThread",
        "start",
        "(Ljava/lang/Thread;)Ljelatine/VMThread;",
        jelatine_VMThread_start
    },
    {
        "jelatine/VMThread",
        "activeCount",
        "()I",
        jelatine_VMThread_activeCount
    },
    {
        "jelatine/VMThread",
        "join",
        "(Ljava/lang/Thread;)V",
        jelatine_VMThread_join
    },

    { NULL, NULL, NULL, NULL } // Placeholder
};

/** Searches for a native method using the provided class name, method name and
 * signature
 * \param cl_name Name of the class
 * \param name Name of the method
 * \param desc Descriptor of the method
 * \returns A pointer to a native method structure or NULL if none is found
 * matching the specified criteria */

native_proto_t native_method_lookup(const char *cl_name, const char *name,
                                    const char *desc)
{
    for (size_t i = 0; native_desc[i].class_name != NULL; i++) {
        if ((strcmp(cl_name, native_desc[i].class_name) == 0)
                && (strcmp(name, native_desc[i].name) == 0)
                && (strcmp(desc, native_desc[i].descriptor) == 0))
        {
            return native_desc[i].func;
        }
    }

    return NULL;
} // native_method_lookup()

/** Implementation of java.lang.Class.forName() */

static KNI_RETURNTYPE_OBJECT java_lang_Class_forName( void )
{
    int exception;
    class_t *cl;
    uint16_t *data;
    size_t length;
    char *str, *tmp;

    KNI_StartHandles(2);
    KNI_DeclareHandle(str_ref);
    KNI_DeclareHandle(cl_ref);

    KNI_GetParameterAsObject(1, str_ref);

    if (KNI_IsNullHandle(str_ref)) {
        KNI_ThrowNew("java/lang/NullPointerException", NULL);
    } else {
        data = array_get_data(JAVA_LANG_STRING_REF2PTR(*str_ref)->value);
        data += JAVA_LANG_STRING_REF2PTR(*str_ref)->offset;
        length = JAVA_LANG_STRING_REF2PTR(*str_ref)->count;
        str = java_to_utf8(data, length);

        // Turn the dots in the classname into slashes
        for (tmp = str; *tmp != 0; tmp++) {
            if (*tmp == '.') {
                *tmp = '/';
            }
        }

        // Try to resolve the class
        c_try {
            cl = bcl_resolve_class_by_name(NULL, str);
            *cl_ref = class_get_object(cl);
        } c_catch (exception) {
            if (exception == JAVA_LANG_CLASSNOTFOUNDEXCEPTION) {
                c_clear_exception();
                KNI_ThrowNew("java/lang/ClassNotFoundException", NULL);
            } else {
                // HACK
                while (__declared_count__--) {
                    thread_pop_root();
                }

                // Unrecoverable error, simply rethrow
                gc_free(str);
                c_rethrow(exception);
            }
        }

        gc_free(str);
    }

    KNI_EndHandlesAndReturnObject(cl_ref);
} // java_lang_Class_forName()

/** Implementation of java.lang.Class.newInstance() */

static KNI_RETURNTYPE_OBJECT java_lang_Class_newInstance( void )
{
    class_t *cl;

    KNI_StartHandles(2);
    KNI_DeclareHandle(cl_ref);
    KNI_DeclareHandle(obj_ref);

    KNI_GetThisPointer(cl_ref);
    cl = bcl_get_class_by_id(JAVA_LANG_CLASS_REF2PTR(*cl_ref)->id);
    *obj_ref = gc_new(cl);

    // TODO: Invoke the constructor on obj!
    KNI_EndHandlesAndReturnObject(obj_ref);
} // java_lang_Class_newInstance()

/** Implementation of java.lang.Class.isInstance() */

static KNI_RETURNTYPE_BOOLEAN java_lang_Class_isInstance( void )
{
    jboolean res;

    KNI_StartHandles(2);
    KNI_DeclareHandle(cl_ref);
    KNI_DeclareHandle(obj_ref);

    KNI_GetThisPointer(cl_ref);
    KNI_GetParameterAsObject(1, obj_ref);
    res = KNI_IsInstanceOf(obj_ref, cl_ref);
    KNI_EndHandles();
    KNI_ReturnBoolean(res);
} // java_lang_Class_isInstance()

/** Implementation of java.lang.Class.isAssignableFrom() */

static KNI_RETURNTYPE_BOOLEAN java_lang_Class_isAssignableFrom( void )
{
    jboolean res;

    KNI_StartHandles(2);
    KNI_DeclareHandle(this_ref);
    KNI_DeclareHandle(cl_ref);

    KNI_GetThisPointer(this_ref);
    KNI_GetParameterAsObject(1, cl_ref);

    if (!KNI_IsNullHandle(cl_ref)) {
        res = KNI_IsAssignableFrom(cl_ref, this_ref);
    } else {
        KNI_ThrowNew("java/lang/NullPointerException", NULL);
        res = KNI_FALSE;
    }

    KNI_EndHandles();
    KNI_ReturnBoolean(res);
} // java_lang_Class_isAssignableFrom()

#if JEL_FP_SUPPORT

/** Implementation of java.lang.Double.toString() */

static KNI_RETURNTYPE_OBJECT java_lang_Double_toString( void )
{
    jdouble a;
    char *str;
    /* This buffer should be large enough to hold (-)iiiiii.ffffff and
     * (-)d.ffffffE(-)eeee*/
    char scratchpad[16];

    KNI_StartHandles(1);
    KNI_DeclareHandle(str_ref);

    a = KNI_GetParameterAsDouble(1);

    if (isnan(a)) {
        str = "NaN";
    } else if (isinf(a)) {
        if (a > 0.0f) {
            str = "Infinity";
        } else {
            str = "-Infinity";
        }
    } else {
        if ((a >= 10e-3) && (a < 10e7)) {
            snprintf(scratchpad, 16, "%.6f", a);
        } else {
            snprintf(scratchpad, 16, "%.6E", a);
        }

        scratchpad[15] = 0; /* Make sure that the string is terminated... */
        str = scratchpad;
    }

    KNI_NewStringUTF(str, str_ref);
    KNI_EndHandlesAndReturnObject(str_ref);
} // java_lang_Double_toString()

/** Implementation of java.lang.Double.parseDouble() */

static KNI_RETURNTYPE_DOUBLE java_lang_Double_parseDouble( void )
{
    // TODO
    dbg_error("java.lang.Double.parseDouble() is not implemented\n");
    KNI_ReturnDouble(0.0);
} // java_lang_Double_parseDouble()

/** Implementation of java.lang.Double.doubleToLongBits() */

static KNI_RETURNTYPE_LONG java_lang_Double_doubleToLongBits( void )
{
    jlong val = KNI_GetParameterAsLong(1);
    KNI_ReturnLong(val);
} // java_lang_Double_doubleToLongBits()

/** Implementation of java.lang.Double.longBitsToDouble() */

static KNI_RETURNTYPE_DOUBLE java_lang_Double_longBitsToDouble( void )
{
    jdouble val = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(val);
} // java_lang_Double_longBitsToDouble()

#endif /* JEL_FP_SUPPORT */

#if JEL_FP_SUPPORT

/** Implementation of java.lang.Float.toString() */

static KNI_RETURNTYPE_OBJECT java_lang_Float_toString( void )
{
    jfloat a;
    char *str;
    /* This buffer should be large enough to hold (-)iiiiii.ffffff and
     * (-)d.ffffffE(-)eeee*/
    char scratchpad[16];

    KNI_StartHandles(1);
    KNI_DeclareHandle(str_ref);

    a = KNI_GetParameterAsFloat(1);

    if (isnan(a)) {
        str = "NaN";
    } else if (isinf(a)) {
        if (a > 0.0f) {
            str = "Infinity";
        } else {
            str = "-Infinity";
        }
    } else {
        if ((a >= 10e-3) && (a < 10e7)) {
            snprintf(scratchpad, 16, "%.6f", a);
        } else {
            snprintf(scratchpad, 16, "%.6E", a);
        }

        scratchpad[15] = 0; /* Make sure that the string is terminated... */
        str = scratchpad;
    }

    KNI_NewStringUTF(str, str_ref);
    KNI_EndHandlesAndReturnObject(str_ref);
} // java_lang_Float_toString()

/** Implementation of java.lang.Float.floatToIntBits() */

static KNI_RETURNTYPE_INT java_lang_Float_floatToIntBits( void )
{
    jint val = KNI_GetParameterAsInt(1);
    KNI_ReturnInt(val);
} // java_lang_Float_floatToIntBits()

/** Implementation of java.lang.Float.intBitsToFloat() */

static KNI_RETURNTYPE_FLOAT java_lang_Float_intBitsToFloat( void )
{
    jfloat val = KNI_GetParameterAsFloat(1);
    KNI_ReturnFloat(val);
} // java_lang_Float_intBitsToFloat()

#endif // JEL_FP_SUPPORT

#if JEL_FP_SUPPORT

/** Implementation of java.lang.Math.sin() */

static KNI_RETURNTYPE_DOUBLE java_lang_Math_sin( void )
{
    jdouble a = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(sin(a));
} // java_lang_Math_sin()

/** Implementation of java.lang.Math.cos() */

static KNI_RETURNTYPE_DOUBLE java_lang_Math_cos( void )
{
    jdouble a = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(cos(a));
} // java_lang_Math_cos()

/** Implementation of java.lang.Math.tan() */

static KNI_RETURNTYPE_DOUBLE java_lang_Math_tan( void )
{
    jdouble a = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(tan(a));
} // java_lang_Math_tan()

/** Implementation of java.lang.Math.sqrt() */

static KNI_RETURNTYPE_DOUBLE java_lang_Math_sqrt( void )
{
    jdouble a = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(sqrt(a));
} // java_lang_Math_sqrt()

/** Implementation of java.lang.Math.ceil() */

static KNI_RETURNTYPE_DOUBLE java_lang_Math_ceil( void )
{
    jdouble a = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(ceil(a));
} // java_lang_Math_ceil()

/** Implementation of java.lang.Math.floor() */

static KNI_RETURNTYPE_DOUBLE java_lang_Math_floor( void )
{
    jdouble a = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(floor(a));
} // java_lang_Math_floor()

/** Implementation of java.lang.Math.log() */

static KNI_RETURNTYPE_DOUBLE java_lang_Math_log( void )
{
    jdouble a = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(log(a));
} // java_lang_Math_log()

/** Implementation of java.lang.Math.exp() */

static KNI_RETURNTYPE_DOUBLE java_lang_Math_exp( void )
{
    jdouble a = KNI_GetParameterAsDouble(1);
    KNI_ReturnDouble(exp(a));
} // java_lang_Math_exp()

#endif // JEL_FP_SUPPORT

/** Implementation of java.lang.Object.getClass() */

static KNI_RETURNTYPE_OBJECT java_lang_Object_getClass( void )
{
    KNI_StartHandles(2);
    KNI_DeclareHandle(this_ref);
    KNI_DeclareHandle(cl_ref);

    KNI_GetThisPointer(this_ref);
    *cl_ref = class_get_object(header_get_class((header_t *) *this_ref));
    KNI_EndHandlesAndReturnObject(cl_ref);
} // java_lang_Object_getClass()

/** Implementation of java.lang.Object.notify() */

static KNI_RETURNTYPE_VOID java_lang_Object_notify( void )
{
#if !JEL_THREAD_NONE
    KNI_StartHandles(1);
    KNI_DeclareHandle(this_ref);

    KNI_GetThisPointer(this_ref);

    if (!thread_notify(thread_self(), *this_ref, false)) {
        KNI_ThrowNew("java/lang/IllegalMonitorStateException", NULL);
    }

    KNI_EndHandles();
    KNI_ReturnVoid();
#else
    KNI_ReturnVoid();
#endif // !JEL_THREAD_NONE
} // java_lang_Object_notify()

/** Implementation of java.lang.Object.notifyAll() */

static KNI_RETURNTYPE_VOID java_lang_Object_notifyAll( void )
{
#if !JEL_THREAD_NONE
    KNI_StartHandles(1);
    KNI_DeclareHandle(this_ref);

    KNI_GetThisPointer(this_ref);

    if (!thread_notify(thread_self(), *this_ref, false)) {
        KNI_ThrowNew("java/lang/IllegalMonitorStateException", NULL);
    }

    KNI_EndHandles();
    KNI_ReturnVoid();
#else
    KNI_ReturnVoid();
#endif // !JEL_THREAD_NONE
} // java_lang_Object_notifyAll()

/** Implementation of java.lang.Object._wait() */

static KNI_RETURNTYPE_VOID java_lang_Object__wait( void )
{
#if !JEL_THREAD_NONE
    thread_t *self = thread_self();
    int64_t millis;
    int32_t nanos;

    KNI_StartHandles(1);
    KNI_DeclareHandle(this_ref);

    KNI_GetThisPointer(this_ref);
    millis = KNI_GetParameterAsLong(1);
    nanos = KNI_GetParameterAsInt(3);

    if (!thread_wait(self, *this_ref, millis, nanos)) {
        KNI_ThrowNew("java/lang/IllegalMonitorStateException", NULL);
    } else {
        // FIXME: Do we need to lock the object?
        if (JAVA_LANG_THREAD_REF2PTR(self->obj)->interrupted) {
            JAVA_LANG_THREAD_REF2PTR(self->obj)->interrupted = false;
            KNI_ThrowNew("java/lang/InterruptedException", NULL);
        }
    }

    KNI_EndHandles();
    KNI_ReturnVoid();
#else
    KNI_ReturnVoid();
#endif // !JEL_THREAD_NONE
} // java_lang_Object_wait_nocheck()

/** Implementation of java.lang.Runtime.exit() */

static KNI_RETURNTYPE_VOID java_lang_Runtime_exit( void )
{
    exit(KNI_GetParameterAsInt(1));
} // java_lang_Runtime_exit()

/** Implementation of java.lang.Runtime.freeMemory() */

static KNI_RETURNTYPE_LONG java_lang_Runtime_freeMemory( void )
{
    KNI_ReturnLong(gc_free_memory());
} // java_lang_Runtime_freeMemory()

/** Implementation of java_lang_Runtime.totalMemory() */

static KNI_RETURNTYPE_LONG java_lang_Runtime_totalMemory( void )
{
    KNI_ReturnLong(gc_total_memory());
} // java_lang_Runtime_totalMemory()

/** Implementation of java.lang.Runtime.gc() */

static KNI_RETURNTYPE_VOID java_lang_Runtime_gc( void )
{
    gc_collect();
    KNI_ReturnVoid();
} // java_lang_Runtime_gc()

/** Implementation of java.lang.String.intern() */

static KNI_RETURNTYPE_OBJECT java_lang_String_intern( void )
{
    java_lang_String_t *str;

    KNI_StartHandles(1);
    KNI_DeclareHandle(this_ref);

    KNI_GetThisPointer(this_ref);
    str = jstring_intern(JAVA_LANG_STRING_REF2PTR(*this_ref));
    *this_ref = JAVA_LANG_STRING_PTR2REF(str);
    KNI_EndHandlesAndReturnObject(this_ref);
} // java_lang_String_intern

/** Implementation of java.lang.System.currentTimeMillis() */

static KNI_RETURNTYPE_LONG java_lang_System_currentTimeMillis( void )
{
    struct timeval now;
    int64_t ret;

    gettimeofday(&now, NULL);

    ret = (int64_t) now.tv_sec * 1000;
    ret += (int64_t) now.tv_usec / 1000;
    KNI_ReturnLong(ret);
} // java_lang_System_currentTimeMillis()

/** Implementation of java.lang.System.arraycopy() */

static KNI_RETURNTYPE_VOID java_lang_System_arraycopy( void )
{
    class_t *src_type;
    class_t *dest_type;
    array_t *src_array;
    array_t *dest_array;
    uint32_t src_elem_type;
    uint32_t dest_elem_type;

    int32_t srcOffset;
    int32_t destOffset;
    int32_t len;

    KNI_StartHandles(2);
    KNI_DeclareHandle(src_ref);
    KNI_DeclareHandle(dest_ref);

    KNI_GetParameterAsObject(1, src_ref);
    srcOffset = KNI_GetParameterAsInt(2);
    KNI_GetParameterAsObject(3, dest_ref);
    destOffset = KNI_GetParameterAsInt(4);
    len = KNI_GetParameterAsInt(5);

    if (KNI_IsNullHandle(src_ref) || KNI_IsNullHandle(dest_ref)) {
        KNI_ThrowNew("java/lang/NullPointerException", NULL);
        goto end;
    }

    src_type = header_get_class((header_t *) *src_ref);
    dest_type = header_get_class((header_t *) *dest_ref);
    src_array = (array_t *) *src_ref;
    dest_array = (array_t *) *dest_ref;

    if (!(class_is_array(src_type) && class_is_array(dest_type))) {
        KNI_ThrowNew("java/lang/ArrayStoreException", NULL);
        goto end;
    }

    src_elem_type = src_type->elem_type;
    dest_elem_type = dest_type->elem_type;

    if ((src_elem_type != dest_elem_type)
        && ((src_elem_type != PT_REFERENCE)
            || (dest_elem_type != PT_REFERENCE)))
    {
        KNI_ThrowNew("java/lang/ArrayStoreException", NULL);
        goto end;
    }

    if ((srcOffset < 0)
        || (destOffset < 0)
        || (len < 0)
        || (srcOffset + len > src_array->length)
        || (destOffset + len > dest_array->length))
    {
        KNI_ThrowNew("java/lang/IndexOutOfBoundsException", NULL);
        goto end;
    }

    if (src_elem_type != PT_REFERENCE) {
        size_t scaled_src_offset, scaled_dest_offset, scaled_length;
        uint8_t *src_data = array_get_data(src_array);
        uint8_t *dest_data = array_get_data(dest_array);

        switch (src_elem_type) {
            case PT_BYTE:
                scaled_src_offset = srcOffset;
                scaled_dest_offset = destOffset;
                scaled_length = len;
                break;

            case PT_BOOL: /* Special case */
                if ((src_array == dest_array) && (srcOffset < destOffset)) {
                    srcOffset += len - 1;
                    destOffset += len - 1;

                    for (size_t i = 0; i < len; i++) {
                        uint8_t temp;
                        uint8_t ntemp;

                        temp = src_data[srcOffset >> 3] >> (srcOffset & 0x7);
                        ntemp = temp ^ 0x1;
                        dest_data[destOffset >> 3] |= temp
                                                      << (destOffset & 0x7);
                        dest_data[destOffset >> 3] &= ~(ntemp
                                                        << (destOffset & 0x7));
                        srcOffset--;
                        destOffset--;
                    }
                } else {
                    for (size_t i = 0; i < len; i++) {
                        uint8_t temp;
                        uint8_t ntemp;

                        temp = src_data[srcOffset >> 3] >> (srcOffset & 0x7);
                        ntemp = temp ^ 0x1;
                        dest_data[destOffset >> 3] |= temp
                                                      << (destOffset & 0x7);
                        dest_data[destOffset >> 3] &= ~(ntemp
                                                        << (destOffset & 0x7));
                        srcOffset++;
                        destOffset++;
                    }
                }

                goto end;

            case PT_CHAR:
            case PT_SHORT:
                scaled_src_offset = srcOffset * 2;
                scaled_dest_offset = destOffset * 2;
                scaled_length = len * 2;
                break;

            case PT_INT:
#if JEL_FP_SUPPORT
            case PT_FLOAT:
#endif // JEL_FP_SUPPORT
                scaled_src_offset = srcOffset * 4;
                scaled_dest_offset = destOffset * 4;
                scaled_length = len * 4;
                break;

            case PT_LONG:
#if JEL_FP_SUPPORT
            case PT_DOUBLE:
#endif // JEL_FP_SUPPORT
                scaled_src_offset = srcOffset * 8;
                scaled_dest_offset = destOffset * 8;
                scaled_length = len * 8;
                break;

            default:
                scaled_src_offset = 0;
                scaled_dest_offset = 0;
                scaled_length = 0;
                dbg_unreachable();
        }

        memmove(dest_data + scaled_dest_offset, src_data + scaled_src_offset,
                scaled_length);
    } else {
        arraycopy_ref(src_array, srcOffset, dest_array, destOffset, len);
    }

end:
    KNI_EndHandles();
} // java_lang_System_arraycopy()

/** Implementation of java.lang.System.identityHashCode() */

static KNI_RETURNTYPE_INT java_lang_System_identityHashCode( void )
{
    jint hash;

    KNI_StartHandles(1);
    KNI_DeclareHandle(o_ref);

    KNI_GetParameterAsObject(1, o_ref);
    hash = *o_ref / sizeof(uintptr_t);
    KNI_EndHandles();
    KNI_ReturnInt(hash);
} // java_lang_System_identityHashCode()

/** Implementation of java.lang.Throwable.printStackTrace() */

static KNI_RETURNTYPE_VOID java_lang_Throwable_printStackTrace( void )
{
    // TODO: does nothing at the moment
    KNI_ReturnVoid();
} // java_lang_Throwable_printStackTrace()

/** Implementation of java.lang.ref.WeakReference.addToWeakReferenceList() */

static KNI_RETURNTYPE_VOID java_lang_ref_WeakReference_addToWeakReferenceList( void )
{
    java_lang_ref_WeakReference_t *wref;

    KNI_StartHandles(1);
    KNI_DeclareHandle(wref_ref);

    KNI_GetParameterAsObject(1, wref_ref);
    wref = JAVA_LANG_REF_WEAKREFERENCE_REF2PTR(*wref_ref);
    gc_register_weak_ref(wref);
    KNI_EndHandles();
    KNI_ReturnVoid();
} // java_lang_ref_WeakReference_addToWeakReferenceList()

#if JEL_FINALIZER

/** Implementation of jelatine.VMFinalizer.getNextObject() */

static KNI_RETURNTYPE_OBJECT jelatine_VMFinalizer_getNextObject( void )
{
    KNI_StartHandles(1);
    KNI_DeclareHandle(obj_ref);

    *obj_ref = gc_get_finalizable();
    KNI_EndHandlesAndReturnObject(obj_ref);
} // jelatine_VMFinalizer_getNextObject()

#endif // JEL_FINALIZER

/** Implementation of jelatine.VMOutputStream.write_to_stderr() */

static KNI_RETURNTYPE_VOID jelatine_VMOutputStream_write_to_stderr( void )
{
    fputc(KNI_GetParameterAsByte(1), stderr);
} // jelatine_VMOutputStream_write_to_stderr()

/** Implementation of jelatine.VMOutputStream.write_to_stdout() */

static KNI_RETURNTYPE_VOID jelatine_VMOutputStream_write_to_stdout( void )
{
    fputc(KNI_GetParameterAsByte(1), stdout);
} // jelatine_VMOutputStream_write_to_stdout()

#if JEL_JARFILE_SUPPORT

/** Implementation of jelatine.VMResourceStream.open() */

static KNI_RETURNTYPE_BOOLEAN jelatine_VMResourceStream_open( void )
{
    jboolean res;
    java_lang_String_t *jstr;
    jelatine_VMResourceStream_t *rs;
    uint16_t *value_data;
    char *str;
    ZZIP_STAT zzip_stat;

    KNI_StartHandles(2);
    KNI_DeclareHandle(rs_ref);
    KNI_DeclareHandle(jstr_ref);

    KNI_GetThisPointer(rs_ref);
    rs = JELATINE_VMRESOURCESTREAM_REF2PTR(*rs_ref);

    // Get the resource name
    *jstr_ref = rs->resource;
    jstr = JAVA_LANG_STRING_REF2PTR(*jstr_ref);
    value_data = array_get_data(jstr->value);
    str = java_to_utf8(value_data + jstr->offset, jstr->count);

    // Open the stream
    rs->handle = bcl_get_resource(str);
    gc_free(str);

    if (rs->handle == NULL) {
        res = KNI_FALSE;
    } else {
        // Get the decompressed file length
        zzip_file_stat(rs->handle, &zzip_stat);
        rs->available = zzip_stat.st_size;
        res = KNI_TRUE;
    }

    KNI_EndHandles();
    KNI_ReturnBoolean(res);
} // jelatine_VMResourceStream_open()

/** Implementation of jelatine.VMResourceStream.read() */

static KNI_RETURNTYPE_INT jelatine_VMResourceStream_read( void )
{
    jelatine_VMResourceStream_t *rs;
    uint8_t data;
    jint res;

    KNI_StartHandles(1);
    KNI_DeclareHandle(rs_ref);

    KNI_GetThisPointer(rs_ref);
    rs = JELATINE_VMRESOURCESTREAM_REF2PTR(*rs_ref);

    if (zzip_file_read(rs->handle, &data, 1) == 0) {
        // EOF reached, return -1
        rs->available = 0;
        res = -1;
    } else {
        rs->available--;
        res = data;
    }

    KNI_EndHandles();
    KNI_ReturnInt(res);
} // jelatine_VMResourceStream_read()

/** Implementation of jelatine.VMResourceStream.finalize() */

static KNI_RETURNTYPE_VOID jelatine_VMResourceStream_finalize( void )
{
    jelatine_VMResourceStream_t *rs;

    KNI_StartHandles(1);
    KNI_DeclareHandle(rs_ref);

    KNI_GetThisPointer(rs_ref);
    rs = JELATINE_VMRESOURCESTREAM_REF2PTR(*rs_ref);

    // Close the zip file handle
    if (rs->handle != NULL) {
        zzip_file_close(rs->handle);
    }

    KNI_EndHandles();
    KNI_ReturnVoid();
} // jelatine_VMResourceStream_finalize()

#endif // JEL_JARFILE_SUPPORT

/** Implementation of jelatine.VMThread.currentThread() */

static KNI_RETURNTYPE_OBJECT jelatine_VMThread_currentThread( void )
{
    KNI_StartHandles(1);
    KNI_DeclareHandle(thread_ref);

    *thread_ref = thread_self()->obj;
    KNI_EndHandlesAndReturnObject(thread_ref);
} // jelatine_VMThread_currentThread()

/** Implementation of jelatine.VMThread.yield() */

static KNI_RETURNTYPE_VOID jelatine_VMThread_yield( void )
{
    thread_yield();
    KNI_ReturnVoid();
} // jelatine_VMThread_yield()

/** Implementation of jelatine.VMThread.sleep() */

static KNI_RETURNTYPE_VOID jelatine_VMThread_sleep( void )
{
    thread_t *self = thread_self();

    thread_sleep(KNI_GetParameterAsLong(1));

    // FIXME: Do we need to lock the object?
    if (JAVA_LANG_THREAD_REF2PTR(self->obj)->interrupted) {
        JAVA_LANG_THREAD_REF2PTR(self->obj)->interrupted = false;
        KNI_ThrowNew("java/lang/InterruptedException", NULL);
    }

    KNI_ReturnVoid();
} // jelatine_VMThread_sleep()

/** Implementation of jelatine.VMThread.start() */

static KNI_RETURNTYPE_OBJECT jelatine_VMThread_start( void )
{
#if !JEL_THREAD_NONE
    thread_t *new_thread;
    java_lang_Thread_t *jthread;
    class_t *cl;
    method_t *run;

    KNI_StartHandles(2);
    KNI_DeclareHandle(thread_ref);
    KNI_DeclareHandle(vmthread);

    KNI_GetParameterAsObject(1, thread_ref);
    jthread = JAVA_LANG_THREAD_REF2PTR(*thread_ref);
    cl = header_get_class(&(jthread->header));
    run = mm_get(cl->method_manager, "run", "()V");
    new_thread = thread_launch(*thread_ref, run);
    // FIXME: We should never stuff a C pointer inside an object reference
    *vmthread = (uintptr_t) new_thread;
    KNI_EndHandlesAndReturnObject(vmthread);
#else
    KNI_StartHandles(1);
    KNI_DeclareHandle(thread_ref);
    KNI_EndHandlesAndReturnObject(thread_ref);
#endif // !JEL_THREAD_NONE
} // jelatine_VMThread_start()

/** Implementation of jelatine.VMThread.activeCount() */

static KNI_RETURNTYPE_INT jelatine_VMThread_activeCount( void )
{
    // Decrease thread number by one as the finalizer thread is for internal purpose
    KNI_ReturnInt(tm_active() - 1);
} // jelatine_VMThread_activeCount()

/** Implementation of jelatine.VMThread.join() */

static KNI_RETURNTYPE_VOID jelatine_VMThread_join( void )
{
#if !JEL_THREAD_NONE
    thread_t *self = thread_self();

    KNI_StartHandles(1);
    KNI_DeclareHandle(thread_ref);

    KNI_GetParameterAsObject(1, thread_ref);
    thread_join((thread_t *) (JAVA_LANG_THREAD_REF2PTR(*thread_ref)->vmThread));

    // FIXME: Do we need to lock the object?
    if (JAVA_LANG_THREAD_REF2PTR(self->obj)->interrupted) {
        JAVA_LANG_THREAD_REF2PTR(self->obj)->interrupted = false;
        KNI_ThrowNew("java/lang/InterruptedException", NULL);
    }

    KNI_EndHandles();
    KNI_ReturnVoid();
#else
    KNI_ReturnVoid();
#endif // !JEL_THREAD_NONE
} // jelatine_VMThread_join()
