/* global.h:  The global variables for bc.  */

/*  This file is part of bc written for MINIX.
    Copyright (C) 1991, 1992, 1993, 1994 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/


/* The current break level's lable. */
EXTERN int break_label;

/* The current if statement's else label or label after else. */
EXTERN int if_label;

/* The current for statement label for continuing the loop. */
EXTERN int continue_label;

/* Next available label number. */
EXTERN int next_label;

/* Byte code character storage.  Used in many places for generation of code. */
EXTERN char genstr[80];

/* Count of characters printed to the output in compile_only mode. */
EXTERN int out_count;

/* Have we generated any code since the last initialization of the code
   generator.  */
EXTERN char did_gen;

/* Is this run an interactive execution.  (Is stdin a terminal?) */
EXTERN char interactive;

/* Just generate the byte code.  -c flag. */
EXTERN int compile_only;

/* Load the standard math functions.  -l flag. */
EXTERN int use_math;

/* Give a warning on use of any non-standard feature (non-POSIX).  -w flag. */
EXTERN int warn_not_std;

/* Accept POSIX bc only!  -s flag. */
EXTERN int std_only;

/* Don't print the banner at start up.  -q flag. */
EXTERN int quiet;

/* The list of file names to process. */
EXTERN file_node *file_names;

/* The name of the current file being processed. */
EXTERN char *file_name;

/* Is the current file a named file or standard input? */
EXTERN char   is_std_in;

/* global variables for the bc machine. All will be dynamic in size.*/
/* Function storage. main is (0) and functions (1-f_count) */

EXTERN bc_function *functions;
EXTERN char **f_names;
EXTERN int  f_count;

/* Variable stoarge and reverse names. */

EXTERN bc_var **variables;
EXTERN char **v_names;
EXTERN int  v_count;

/* Array Variable storage and reverse names. */

EXTERN bc_var_array **arrays;
EXTERN char **a_names;
EXTERN int  a_count;

/* Execution stack. */
EXTERN estack_rec *ex_stack;

/* Function return stack. */
EXTERN fstack_rec *fn_stack;

/* Current ibase, obase, scale, and n_history (if needed). */
EXTERN int i_base;
EXTERN int o_base;
EXTERN int scale;
#ifdef READLINE
EXTERN int n_history;
#endif

/* "Condition code" -- false (0) or true (1) */
EXTERN char c_code;

/* Records the number of the runtime error. */
EXTERN char runtime_error;

/* Holds the current location of execution. */
EXTERN program_counter pc;

/* For POSIX bc, this is just for number output, not strings. */
EXTERN int out_col;

/* Keeps track of the current number of characters per output line.
   This includes the \n at the end of the line. */
EXTERN int line_size;

/* Input Line numbers and other error information. */
EXTERN int line_no;
EXTERN int had_error;

/* For larger identifiers, a tree, and how many "storage" locations
   have been allocated. */

EXTERN int next_array;
EXTERN int next_func;
EXTERN int next_var;

EXTERN id_rec *name_tree;

/* defined in number.c */
extern bc_num _zero_;
extern bc_num _one_;

/* For use with getopt.  Do not declare them here.*/
extern int optind;

/* Allow multichar names */
EXTERN int multi_name;

