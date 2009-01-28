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
 * Class file implementation                                                  *
 ******************************************************************************/

/** Opens a class file with the specified class name (in the internal class
 * format) looking in the classpath directory or JAR file, throws an exception
 * if the class file cannot be found or opened.
 * \param name The class name
 * \param classpath The current classpath, either a directory path or a JAR file
 * \returns A pointer to the newly create class_file_t structure */

class_file_t *cf_open(const char *name, const char *classpath)
{
    FILE *file = NULL; // Class-file pointer
    int exception;
    char *path;
    size_t length_cp, size;
    class_file_t *cf;

    /* Prepend the classpath to the class name and append ".class" to the
     * resulting string */

    if (classpath == NULL) {
        length_cp = 0;
    } else {
        length_cp = strlen(classpath);
    }

    path = gc_malloc(strlen(".class") + strlen(name) + length_cp + 1);

    if (classpath == NULL) {
        path[0] = 0;
    } else {
        strcpy(path, classpath);
    }

    strcat(path, name);
    strcat(path, ".class");

    c_try {
        file = efopen(path, "r");
    } c_catch (exception) {
        gc_free(path);
        c_rethrow(exception);
    }

    gc_free(path);
    efseek(file, 0, SEEK_END);
    size = ftell(file);
    efseek(file, 0, SEEK_SET);

    cf = gc_malloc(sizeof(class_file_t));
    cf->file.plain = file;
    cf->pos = 0;
    cf->size = size;
    cf->jar = false;

    return cf;
} // cf_open()

#if JEL_JARFILE_SUPPORT

class_file_t *cf_open_jar(const char *name, ZZIP_DIR *jar)
{
    ZZIP_FILE *file = NULL; // Class-file pointer
    ZZIP_STAT stat;
    char *path;
    class_file_t *cf;
    size_t size;

    //* Append ".class" to the class name

    path = gc_malloc(strlen(".class") + strlen(name) + 1);

    strcpy(path, name);
    strcat(path, ".class");

    file = zzip_file_open(jar, path, 0);
    gc_free(path);

    if (file == NULL) {
        c_throw(JAVA_LANG_CLASSNOTFOUNDEXCEPTION, "Unable to open JAR file");
    }

    zzip_file_stat(file, &stat);
    size = stat.st_size;

    cf = gc_malloc(sizeof(class_file_t));
    cf->file.compressed = file;
    cf->pos = 0;
    cf->size = size;
    cf->jar = true;

    return cf;
} // cf_open_jar()

#endif // JEL_JARFILE_SUPPORT

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
