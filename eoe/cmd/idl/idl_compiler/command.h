/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: command.h,v $
 * Revision 1.3  1993/09/13 16:11:00  jwag
 * split out typedef changes from ksgen changes
 *
 * Revision 1.2  1993/09/08  20:15:18  jwag
 * first ksgen hack
 *
 * Revision 1.1  1993/08/31  06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:38:36  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:33:31  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:44:40  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:00:42  zeliff]
 * 
 * Revision 1.1.2.2  1992/03/24  14:09:27  harrow
 * 	Add support for bug 4, ship holes for arrays of
 * 	ref pointers when contained within other types.
 * 	[1992/03/18  20:58:12  harrow]
 * 
 * Revision 1.1  1992/01/19  03:02:15  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      command.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Definitions for IDL command line parsing.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>               /* IDL common defs */
#include <nametbl.h>            /* Nametable defs */

/*
 *  IDL compiler command line options are kept in two parallel arrays,
 *  cmd_opt and cmd_val.  These arrays are passed as arguments to any
 *  routines that need to access command line information.  The partial
 *  signature of such routines is:
 *
 *  rtn
 *  (
 *      boolean     *cmd_opt,        * [in] Array of command option flags *
 *      void        **cmd_val,       * [in] Array of command option values *
 *      ...
 *  )
 *
 *  cmd_opt is an array of booleans.  Each entry is set to "true" if the
 *  corresponding option is selected and set to "false" if it is not.
 *  Some options, when set to "true", contain additional information in
 *  the cmd_val array.
 *
 *  cmd_val is an array of (void *) elements.  If an element in the cmd_opt
 *  array is set to "false", the corresponding element in the cmd_val array
 *  will be equal to NULL.  If an element in the cmd_opt array is set to
 *  "true", the corresponding element in the cmd_val array will in general
 *  contain additional information for that option.  The obvious exceptions
 *  to this rule are any "True/False" options, for which no additional
 *  information is necessary.
 *
 *  Valid elements of the cmd_val array point to additional data for
 *  the corresponding option.  The additional data can be of a simple type
 *  or a constructed type, depending on the information needed to describe
 *  the option.  For most is it simply (char *), a pointer to a string.
 *
 *  The list of #define's below define a set of indices into the two arrays.
 *  The meaning of each option should be obvious from its index name, which
 *  closely resembles the corresponding command option.
 *
 *  Comments appear next to those options for which there is additional
 *  information in the cmd_val array.  The comment indicates the actual
 *  data type of the cmd_val array element for that option, and what it
 *  represents.
 *
 *  *NOTE*: When adding new options to the list below, be sure to also modify
 *          the opt_info array for the dump_cmd_data function (command.c).
 */

#define opt_caux             0  /* (char *)     Client auxiliary file name */
#define opt_cc_cmd           1  /* (char *)     C command line */
#define opt_cc_opt           2  /* (char *)     Addtl C command line options */
#define opt_cepv             3
#define opt_confirm          4
#define opt_cpp              5  /* (char *)     Filespec of CPP to invoke */
#define opt_cpp_def          6  /* (char **)    Array of define strs for CPP */
#define opt_cpp_opt          7  /* (char *)     Addtl CPP cmd line options */
#define opt_cpp_undef        8  /* (char **)    Array of undef strs for CPP */
#define opt_cstub            9  /* (char *)     Client stub file name */
#define opt_def_idir        10
#define opt_do_bug          11  /* (boolean *)  Array of "bug" flags */
#define opt_emit_cstub      12
#define opt_emit_sstub      13
#define opt_header          14  /* (char *)     Header file name */
#define opt_idir            15  /* (char **)    Array of include dirs */
#define opt_keep_c          16
#define opt_keep_obj        17
#define opt_mepv            18
#define opt_out             19  /* (char *)     Output directory */
#define opt_saux            20  /* (char *)     Server auxiliary file name */
#define opt_source          21  /* (char *)     Source IDL file name */
#define opt_space_opt       22
#define opt_sstub           23  /* (char *)     Server stub file name */
#define opt_stdin           24
#define opt_syntax_check    25
#define opt_verbose         26
#define opt_version         27
#define opt_warn            28
#ifdef __sgi
#define opt_ksgen	    29
#define opt_kstypes	    30
#endif
/*
 * Remaining options are valid only when code built with DUMPERS.
 */
#ifndef DUMPERS
#ifdef __sgi
#define NUM_OPTS            31
#else
#define NUM_OPTS            29
#endif
#else
#define opt_dump_acf        29
#define opt_dump_ast        30
#define opt_dump_ast_after  31
#define opt_dump_cmd        32
#define opt_dump_debug      33
#define opt_dump_flat       34
#define opt_dump_mnode      35
#define opt_dump_mool       36
#define opt_dump_nametbl    37
#define opt_dump_recs       38
#define opt_dump_sends      39
#define opt_dump_unode      40
#define opt_dump_uool       41
#define opt_dump_yy         42
#define opt_ool             43
#ifdef __sgi
#define opt_ksgen	    44
#define opt_kstypes	    45
#define NUM_OPTS            46
#else
#define NUM_OPTS            44
#endif /* __sgi */
#endif

/*
 * Indices into the array of booleans pointed to by cmd_val[opt_do_bug].
 * Note that valid indices start at 1, not 0!
 */
#define bug_array_align  1
#define bug_array_align2 2
#define bug_boolean_def  3
#define bug_array_no_ref_hole 4 /* Leave no hole for array of ref pointers */
#define NUM_BUGS         4


/* Data exported by command.c */

extern char *CMD_def_cpp_cmd;   /* Default cpp command */

/* Functions exported by command.c */

extern boolean CMD_parse_args(
#ifdef PROTO
    int             argc,
    char            **argv,
    boolean         **p_cmd_opt,
    void            ***p_cmd_val,
    STRTAB_str_t    *idl_sid
#endif
);

extern void CMD_explain_args(
#ifdef PROTO
    void
#endif
);
