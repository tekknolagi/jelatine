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

/** \file classfile.c
 * Class-file wrapper implementation */

#include "wrappers.h"

#include "class.h"
#include "classfile.h"
#include "memory.h"
#include "util.h"

/******************************************************************************
 * Classpath type definitions                                                 *
 ******************************************************************************/

/** Represents a generic path, it might be a directory path or JAR file. The
 * \a jar field is updated on demand */

struct path_t {
    const char *str; ///< The path to the directory or JAR file
    ZZIP_DIR *jar; ///< The JAR file handle
};

/** Typedef for the ::struct path_t */
typedef struct path_t path_t;

/** Represents the virtual machine classpath */

struct classpath_t {
    size_t entries; ///< Entries in the application classpath
    path_t boot; ///< Boot classpath for system classes
    path_t user[]; ///< Classpath for the application classes
};

/** Typedef for the ::struct classpath_t */
typedef struct classpath_t classpath_t;

/******************************************************************************
 * Globals                                                                    *
 ******************************************************************************/

/** Global classpath information */
static classpath_t *classpath;

/******************************************************************************
 * Classpath local functions prototypes                                       *
 ******************************************************************************/

static void adapt_classpath_string(path_t *, const char *, size_t);

/******************************************************************************
 * Classpath implementation                                                   *
 ******************************************************************************/

static void adapt_classpath_string(path_t *path, const char *str, size_t len)
{
    char *tmp;

    if (len == 0) {
        c_throw(JAVA_LANG_VIRTUALMACHINEERROR, "Invalid classpath string");
    }

#if JEL_JARFILE_SUPPORT
    if ((len > 4) && (strcmp(str + len - 4, ".jar") == 0)) {
        // This is a JAR file
        tmp = gc_palloc(len + 1);
        strncpy(tmp, str, len);
        path->str = tmp;
        path->jar = zzip_dir_open(tmp, 0);

        if (path->jar == NULL) {
            c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                    "Unable to open JAR file: %s", tmp);
        }

        return;
    }
#endif // JEL_JARFILE_SUPPORT

    if (str[len - 1] != '/') {
        len++;
    }

    tmp = gc_palloc(len + 1);
    strncpy(tmp, str, len);
    tmp[len - 1] = '/';
    path->str = tmp;
} // adapt_classpath_string()

/** Initializes the classpath by processing the \a cp and \a bcp strings
 * \param cp A string holding the application classpath directories and
 * JAR files
 * \param bcp A string holding the system classpath directory or JAR
 * file */

void classpath_init(const char *cp, const char *bcp)
{
    size_t count = 0;
    const char *idx = cp;

    // Count the number of directories/JAR files in the classpath
    while ((idx = strchr(idx, ':')) != NULL) {
        count++;
        idx++;
    }

    classpath = gc_palloc(sizeof(classpath_t) + sizeof(path_t) * (count + 1));
    classpath->entries = count + 1;

    adapt_classpath_string(&classpath->boot, bcp, strlen(bcp));
    idx = cp;

    for (size_t i = 0; i < count; i++) {
        idx = strchr(idx, ':');
        adapt_classpath_string(&classpath->user[i], cp, idx - cp);
        idx++;
        cp = idx;
    }

    adapt_classpath_string(&classpath->user[count], cp, strlen(cp));
} // classpath_init()

/** Tear down the classpath */

void classpath_teardown( void )
{
#if JEL_JARFILE_SUPPORT
    for (size_t i = 0; i < classpath->entries; i++) {
        if (classpath->user[i].jar != NULL) {
            zzip_dir_close(classpath->user[i].jar);
        }
    }
#endif // JEL_JARFILE_SUPPORT
} // classpath_teardown()

/******************************************************************************
 * Class file implementation                                                  *
 ******************************************************************************/

/** Opens a class file by looking it up in the specified classpath
 * \param name The class name
 * \param cp The classpath used for the class lookup
 * \returns A new classfile object, throws an exception upon failure */

static class_file_t *cf_open_with_classpath(const char *name, const path_t *cp)
{
#if JEL_JARFILE_SUPPORT
    ZZIP_FILE *zzip_file;
    ZZIP_STAT stat;
#endif // JEL_JARFILE_SUPPORT
    FILE *file = NULL;
    class_file_t *cf;
    int exception;
    char *path;
    size_t cp_len, size;
    size_t cl_len = strlen(name);

    if (cp->jar != NULL) {
        // Append the .class suffic to the class name
        path = gc_malloc(strlen(".class") + cl_len + 1);

        strncpy(path, name, cl_len + 1);
        strncat(path, ".class", cl_len + strlen(".class") + 1);
        zzip_file = zzip_file_open(cp->jar, path, 0);
        gc_free(path);

        if (zzip_file == NULL) {
            return NULL;
        }

        zzip_file_stat(zzip_file, &stat);
        size = stat.st_size;

        cf = gc_malloc(sizeof(class_file_t));
        cf->file.compressed = zzip_file;
        cf->jar = true;
    } else {
        /* Append the class name to the classpath and add the .class
         * suffix to the resulting string */
        cp_len = strlen(cp->str);
        path = gc_malloc(strlen(".class") + strlen(name) + cp_len + 1);
        strncpy(path, cp->str, cp_len + 1);
        strncat(path, name, strlen(name) + cp_len + 1);
        strncat(path, ".class", strlen(".class") + strlen(name) + cp_len + 1);

        c_try {
            file = efopen(path, "r");
        } c_catch (exception) {
            gc_free(path);
            c_clear_exception();
            return NULL;
        }

        gc_free(path);
        efseek(file, 0, SEEK_END);
        size = ftell(file);
        efseek(file, 0, SEEK_SET);

        cf = gc_malloc(sizeof(class_file_t));
        cf->file.plain = file;
    }

    cf->size = size; // Store the file size

    return cf;
} // cf_open_with_classpath()

/** Opens a class file with the specified class name (in the internal class
 * format) looking in the classpath directory or JAR file, throws an exception
 * if the class file cannot be found or opened.
 * \param name The class name */

class_file_t *cf_open(const char *name)
{
    class_file_t *cf = NULL;

    if ((strncmp(name, "java/", 5) == 0)
        || (strncmp(name, "javac/", 6) == 0)
        || (strncmp(name, "javax/", 6) == 0)
        || (strncmp(name, "jelatine/", 9) == 0))
    {
        cf = cf_open_with_classpath(name, &classpath->boot);
    } else {
        for (size_t i = 0; i < classpath->entries; i++) {
            cf = cf_open_with_classpath(name, &classpath->user[i]);

            if (cf != NULL) {
                break;
            }
        }
    }

    if (cf == NULL) {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR, "Cannot find class %s\n", name);
    }

    return cf;
} // cf_open()

/** Closes a class file and disposes the relevant structure
 * \param cf A pointer to the class_file_y structure to be closed and disposed
 */

void cf_close(class_file_t *cf)
{
    assert(cf);

#if JEL_JARFILE_SUPPORT
    if (cf->jar) {
        zzip_file_close(cf->file.compressed);
    } else {
        efclose(cf->file.plain);
    }
#else
    efclose(cf->file.plain);
#endif // JEL_JARFILE_SUPPORT

    gc_free(cf);
} // class_file_close()

/** Reads a single-byte value from the provided class-file, throws an exception
 * if the end of the file has been already reached
 * \param cf A pointer to an open class-file
 * \returns A single-byte constant read from the class-file */

uint8_t cf_load_u1(class_file_t *cf)
{
    uint8_t data;

    if ((cf->pos + 1) <= cf->size) {
#if JEL_JARFILE_SUPPORT
        if (cf->jar) {
            zzip_file_read(cf->file.compressed, &data, 1);
        } else {
            data = fgetc(cf->file.plain);
        }
#else
        data = fgetc(cf->file.plain);
#endif // JEL_JARFILE_SUPPORT

        cf->pos++;
        return data;
    } else {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Reading past the end of the class file");
    }
} // cf_load_u1()

/** Reads a two-bytes value from the provided class-file, throws an exception if
 * the end of the file has been already reached
 * \param cf A pointer to an open class-file
 * \returns A two-bytes constant read from the class-file */

uint16_t cf_load_u2(class_file_t *cf)
{
    uint16_t result;
    uint8_t data[2];

    if ((cf->pos + 2) <= cf->size) {
#if JEL_JARFILE_SUPPORT
        if (cf->jar) {
            zzip_file_read(cf->file.compressed, data, 2);
        } else {
            fread(data, 2, 1, cf->file.plain);
        }
#else
        fread(data, 2, 1, cf->file.plain);
#endif // JEL_JARFILE_SUPPORT

        result = (data[0] << 8) | data[1];
        cf->pos += 2;
        return result;
    } else {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Reading past the end of the class file");
    }
} // cf_load_u2()

/** Reads a four-bytes value from the provided class-file, throws an exception
 * if the end of the file has been already reached
 * \param cf A pointer to an open class-file
 * \returns A four-bytes constant read from the class-file */

uint32_t cf_load_u4(class_file_t *cf)
{
    uint32_t result;
    uint8_t data[4];

    if ((cf->pos + 4) <= cf->size) {
#if JEL_JARFILE_SUPPORT
        if (cf->jar) {
            zzip_file_read(cf->file.compressed, data, 4);
        } else {
            fread(data, 4, 1, cf->file.plain);
        }
#else
        fread(data, 4, 1, cf->file.plain);
#endif // JEL_JARFILE_SUPPORT

        result = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        cf->pos += 4;
        return result;
    } else {
        c_throw(JAVA_LANG_NOCLASSDEFFOUNDERROR,
                "Reading past the end of the class file");
    }
} // cf_load_u4()

/** Seeks to the position specified by the \a offset parameter, throws an
 * exception if the position is out of the file's range. This function's
 * semantic is the same as fseek() but only the SEEK_SET and SEEK_CUR whence
 * modes are supported
 * \param cf A pointer to an open class-file
 * \param offset The new position in the class-file
 * \param whence From where to start seeking */

void cf_seek(class_file_t *cf, long offset, int whence)
{
    assert(whence == SEEK_CUR || whence == SEEK_SET);

    if (whence == SEEK_CUR) {
        cf->pos += offset;
    } else {
        cf->pos = offset;
    }

    if ((cf->pos > cf->size) || (cf->pos < 0)) {
        c_throw(JAVA_LANG_VIRTUALMACHINEERROR,
                "Seeking past the end of the class file");
    }

#if JEL_JARFILE_SUPPORT
    if (cf->jar) {
        zzip_seek(cf->file.compressed, cf->pos, SEEK_SET);
    } else {
        efseek(cf->file.plain, cf->pos, SEEK_SET);
    }
#else
    efseek(cf->file.plain, cf->pos, SEEK_SET);
#endif // JEL_JARFILE_SUPPORT
} // cf_seek()

#if JEL_JARFILE_SUPPORT

/** Return a ZZIP_FILE handle for the requested resource or NULL if none was
 * found matching the passed name
 * \param resource A string representing the resource name
 * \returns A pointer to a ZZIP_FILE handle or NULL upon failure */

ZZIP_FILE *jar_get_resource(const char *resource)
{
    ZZIP_FILE *zzip_file = NULL;

    for (size_t i = 0; i < classpath->entries; i++) {
        if (classpath->user[i].jar != NULL) {
            zzip_file = zzip_file_open(classpath->user[i].jar, resource, 0);

            if (zzip_file != NULL) {
                break;
            }
        }
    }

    return zzip_file;
} // jar_get_resource()

#endif // JEL_JARFILE_SUPPORT
