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

/** \file main.c
 * main() implementation of the command-line launcher and global data used by
 * the virtual machine */

#include "wrappers.h"

#include "vm.h"
#include "memory.h"

/******************************************************************************
 * Local function declarations                                                *
 ******************************************************************************/

static void parse_command_line_args(int, char **);
static char *adapt_classpath_string(const char *);

/** Self explanatory
 * \param argc Number of arguments
 * \param argv Argument list */

int main(int argc, char *argv[])
{
    int i, len;
    int jargs_n;
    char **jargs;
    char *cp = NULL, *bcp = NULL;
    char *main_class;

    parse_command_line_args(argc, argv);

    if (opts_get_version()) {
        printf("%s\n", PACKAGE_STRING);
    } else if (opts_get_help() || argc == 1 || opts_get_main_class() == NULL) {
        printf("Usage: jelatine [OPTIONS]... CLASSNAME [ARGUMENTS]...\n"
               "\n"
               "where options include:\n"
               "    -b, --bootclasspath <single directory or jar file>\n"
               "    -c, --classpath <single directory or jar file>\n"
               "    -s, --size <size of the heap in bytes>\n"
               "    --stack-size <size of a thread's stack in bytes>\n"
               "\n"
               "    -h, --help      display this help and exit\n"
               "    --version       output version information and exit\n"
 #if JEL_TRACE
               "\n"
               "    --trace-methods trace method invocations\n"
               "    --trace-opcodes trace opcode execution\n"
#endif // JEL_TRACE
#if JEL_PRINT
               "\n"
               "    --print-methods print method invocations\n"
               "    --print-opcodes print opcode execution\n"
               "    --print-memory print memory operations\n"
#endif // JEL_PRINT
            );
    } else {
        jargs = opts_get_jargs();

        if (jargs != NULL) {
            jargs_n = 0;

            while (jargs[jargs_n] != NULL) {
                jargs_n++;
            }

            opts_set_jargs_n(jargs_n);
        } else {
            opts_set_jargs_n(0);
        }

        /* Check that the class name is given in the internal format,
         * otherwise convert it */

        main_class = opts_get_main_class();
        len = strlen(main_class);

        for (i = 0; i < len; i++) {
            if (main_class[i] == '.') {
                main_class[i] = '/';
            }
        }

        cp = opts_get_classpath();
        bcp = opts_get_boot_classpath();

        if (cp != NULL) {
            len = strlen(cp);

            if (len == 0) {
                cp = NULL;
            } else if (len > 3) {
                if (strcmp(cp + len - 4, ".jar") == 0) {
                    ;
                } else {
                    cp = adapt_classpath_string(cp);
                }
            } else {
                cp = adapt_classpath_string(cp);
            }
        } else {
            cp = NULL;
        }

        if (bcp != NULL) {
            len = strlen(bcp);

            if (len == 0) {
                bcp = NULL;
            } else if (len > 3) {
                if (strcmp(bcp + len - 4, ".jar") == 0) {
                    ;
                } else {
                    bcp = adapt_classpath_string(bcp);
                }
            } else {
               bcp = adapt_classpath_string(bcp);
            }
        } else {
            bcp = NULL;
        }

        /* Create the vm and load the classes from the current directory
         * if the classpath is NULL or from the classpath directory */
        vm_run(cp, bcp, main_class, opts_get_heap_size(), opts_get_jargs_n(),
               opts_get_jargs());
    }

    exit(0);
} // main()

/** Parses the command-line args, the results are stored in the appropriate
 * global variables
 * \param argc Same as main()'s argc
 * \param argv Same as main()'s argv */

static void parse_command_line_args(int argc, char *argv[])
{
    size_t i = 1;

    while (i < argc) {
        if (((strcmp("-c", argv[i]) == 0)
             || (strcmp("--classpath", argv[i]) == 0))
            && (i + 1 < argc))

        {
            opts_set_classpath(argv[i + 1]);
            i += 2;
        } else if (((strcmp("-b", argv[i]) == 0)
                    || (strcmp("--bootclasspath", argv[i]) == 0))
                   && (i + 1 < argc))
        {
            opts_set_boot_classpath(argv[i + 1]);
            i += 2;
        } else if (((strcmp("-s", argv[i]) == 0)
                    || (strcmp("--size", argv[i]) == 0))
                   && (i + 1 < argc))
        {
            opts_set_heap_size(atoi(argv[i + 1]));
            i += 2;
        } else if ((strcmp("--stack-size", argv[i]) == 0) && (i + 1 < argc)) {
            size_t stack_size = atoi(argv[i + 1]);

            opts_set_stack_size(size_ceil(stack_size, sizeof(jword_t)));
            i += 2;
        } else if ((strcmp("-h", argv[i]) == 0)
                   || (strcmp("--help", argv[i]) == 0))
        {
            opts_set_help(true);
            i++;
        } else if (strcmp("--version", argv[i]) == 0) {
            opts_set_version(true);
            i++;
#if JEL_PRINT
        } else if (strcmp("--print-opcodes", argv[i]) == 0) {
            opts_set_print_opcodes(true);
            i++;
        } else if (strcmp("--print-methods", argv[i]) == 0) {
            opts_set_print_methods(true);
            i++;
        } else if (strcmp("--print-memory", argv[i]) == 0) {
            opts_set_print_memory(true);
            i++;
#endif // JEL_PRINT
#if JEL_TRACE
        } else if (strcmp("--trace-opcodes", argv[i]) == 0) {
            opts_set_trace_opcodes(true);
            i++;
        } else if (strcmp("--trace-methods", argv[i]) == 0) {
            opts_set_trace_methods(true);
            i++;
#endif // JEL_TRACE
        } else {
            break; // Move to the class-name
        }
    }

    if (i < argc) {
        opts_set_main_class(argv[i]);
        i++;

        if (i < argc) {
            opts_set_jargs_n(i - argc);
            opts_set_jargs(argv + i);
        }
    }
} // parse_command_line_args()

/** Adapths the classpath string so that it matches the convention used by the
 * virtual-machine
 * \param classpath A pointer to the classpath string
 * \returns The new, adapted classpath string */

static char *adapt_classpath_string(const char *classpath)
{
    size_t len;
    char *cp;

    len = strlen(classpath);
    cp = (char *) malloc(len + 2);

    if (cp == NULL) {
        exit(1);
    }

    strncpy(cp, classpath, len + 1);

    if (classpath[len - 1] != '/') {
        cp[len] = '/';
        cp[len + 1] = 0;
    }

    return cp;
} // adapt_classpath_string()
