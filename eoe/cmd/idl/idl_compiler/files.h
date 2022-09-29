/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: files.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:39:25  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:52  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:46:36  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:07  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/05  15:56:11  harrow
 * 	*** empty log message ***
 * 
 * Revision 1.1.1.2  1992/05/05  15:49:19  harrow
 * 	Update prototype of FILE_execute to support display of full
 * 	cc compile command generated if -v is specified.
 * 
 * Revision 1.1  1992/01/19  03:02:34  devrcs
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
**      files.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Header file for file manipulation routines.
**
**  VERSION: DCE 1.0
**
*/

#ifndef files_incl
#define files_incl

#ifndef S_IFREG
#ifdef vms
#  include <types.h>
#  include <stat.h>
#else
#  include <sys/types.h>
#  include <sys/stat.h>
#endif
#endif

#include <nidl.h>               /* IDL common defs */
#include <nametbl.h>

typedef enum                    /* Filespec kinds: */
{
    file_dir,                   /* Directory */
    file_file,                  /* Regular ol' file */
    file_special                /* Something else */
} FILE_k_t;

extern boolean FILE_open(
#ifdef PROTO
    char *filespec,
    FILE **fid
#endif
);

extern boolean FILE_create(
#ifdef PROTO
    char *filespec,
    FILE **fid
#endif
);

extern boolean FILE_lookup(
#ifdef PROTO
    char        *filespec,
    char        **idir_list,
    struct stat *stat_buf,
    char        *lookup_spec
#endif
);

extern boolean FILE_form_filespec(
#ifdef PROTO
    char *in_filespec,
    char *dir,
    char *type,
    char *rel_filespec,
    char *out_filespec
#endif
);

#ifdef VMS
/*
**  Default filespec; only good for one call to FILE_parse.
*/
extern char *FILE_def_filespec;
#endif

extern boolean FILE_parse(
#ifdef PROTO
    char *filespec,
    char *dir,
    char *name,
    char *type
#endif
);

extern boolean FILE_has_dir_info(
#ifdef PROTO
    char *filespec
#endif
);

extern boolean FILE_is_cwd(
#ifdef PROTO
    char *filespec
#endif
);

extern boolean FILE_kind(
#ifdef PROTO
    char        *filespec,
    FILE_k_t    *filekind
#endif
);

extern int FILE_execute_cmd(
#ifdef PROTO
    char        *cmd_string,
    char        *p1,
    char        *p2,
    long        msg_id
#endif
);

extern void FILE_delete(
#ifdef PROTO
    char        *filename
#endif
);

#endif /* files_incl */
