/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: getflags.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:45  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:26  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:26  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:41  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:40  devrcs
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
**  NAME
**
**      GETFLAGS.H
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Constants for command line parsing
**
**  VERSION: DCE 1.0
**
*/

#ifndef GETFLAGS_H
#define GETFLAGS_H

#include <nidl.h>


typedef char *FLAGDEST;
typedef struct options
        {
        char *option;
        int ftype;
        FLAGDEST dest;
        } OPTIONS;

/*
 * Rico, 23-Mar-90: The format of the ftype field appears to be:
 *
 *      Bit 15:   (1 bit)  HIDARG flag - flags hidden args to printflags fn
 *      Bit 14:   (1 bit)  VARARG flag - flags a variable option (can repeat)
 *      Bit 13-8: (6 bits) maximum number of occurences of a variable option
 *      Bit  7-0: (8 bits) argument type
 *
 *       15  14  13              8   7                    0
 *      -----------------------------------------------------
 *      | H | V |  max_occurences  |     argument_type      |
 *      -----------------------------------------------------
 */

/* Argument types */
#define INTARG      0
#define STRARG      1
#define TOGGLEARG   2
#define CHRARG      3
#define FLTARG      4
#define LONGARG     5
#define ASSERTARG   6
#define DENYARG     7
#define OSTRARG     8           /* Optional string arg, added 23-Mar-90 */

#define HIDARG (128 << 8)       /* H bit */
#define VARARGFLAG 64           /* V bit - gets shifted 8 bits by macros */
#define MULTARGMASK 63          /* Mask to get max_occurences */

/* Macros for specifying ftype */
#define MULTARG(n, a) (((n) << 8) + a)
#define AINTARG(n) MULTARG(n,INTARG)
#define VINTARG(n) AINTARG(n|VARARGFLAG)
#define ASTRARG(n) MULTARG(n,STRARG)
#define VSTRARG(n) ASTRARG(n|VARARGFLAG)
#define ATOGGLEARG(n) MULTARG(n,TOGGLEARG)
#define AASSERTARG(n) MULTARG(n,ASSERTARG)
#define ADENYARG(n) MULTARG(n,DENYARG)
#define ACHRARG(n) MULTARG(n,CHRARG)
#define VCHRARG(n) ACHRARG(n|VARARGFLAG)
#define AFLTARG(n) MULTARG(n,FLTARG)
#define VFLTARG(n) AFLTARG(n|VARARGFLAG)
#define ALONGARG(n) MULTARG(n,LONGARG)
#define VLONGARG(n) AFLTARG(n|VARARGFLAG)

/* Macros for converting command line arguments */
#define GETINT(s) {s = atoi(*++av); ac--;}
#define GETSTR(s) {s = *++av;ac--;}
#define GETCH(s) {av++; s = av[0][0]; ac--;}
#define GETFLT(s) {s = atof(*++av); ac--;}
#define GETLONG(s) {s = atol(*++av); ac--;}


void printflags (
#ifdef PROTO
    OPTIONS table[]
#endif
);

void getflags (
#ifdef PROTO
    int argc,
    char **argv,
    OPTIONS table[]
#endif
);

void flags_incr_count (
#ifdef PROTO
    OPTIONS table[],
    char *option,
    int delta
#endif
);

int flags_option_count (
#ifdef PROTO
    OPTIONS table[],
    char *option
#endif
);

int flags_other_count (
#ifdef PROTO
    void
#endif
);

char *flags_other (
#ifdef PROTO
    int index
#endif
);


#endif
