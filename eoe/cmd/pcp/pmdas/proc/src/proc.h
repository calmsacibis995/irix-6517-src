/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: proc.h,v 1.2 1998/09/09 19:04:35 kenmcd Exp $"

#ifndef PROC_H
#define PROC_H

/* 
 * proc name related stuff
 */
#define PROCFS			"/proc"
#define PROCFS_INFO		"/proc/pinfo"

/* 
 * SIZE of string macros
 * all macros include at least +1 for string terminator
 */
/* max size for proc_entry_len + 1; entry_len = 10 on 6.5 */
#define PROC_FNAME_SIZE		20  

/* sizeof() includes null terminator which takes into account the 
 * extra '/' needed in the path.
 */
#define PROC_PATH_SIZE		(sizeof(PROCFS_INFO)+PROC_FNAME_SIZE) 

/* expand by 4 for non-printables; +1 for space b/w pid and args */
#define INSTMAP_NAME_SIZE	(PROC_FNAME_SIZE+1+PRARGSZ*4)	

extern int proc_entry_len;      /* length of a proc entry in /proc */
extern char proc_fmt[8];

/* encode the domain(x), cluster (y) and item (z) parts of the PMID */
#define PMID(x,y) ((x<<10)|y)

#define NSEC_SEC(x) (x >= 500000000 ? 1 : 0)
#define NSEC_MSEC(x) (x/1000000)

/* Macros for initialising a pmAtomValue from long and unsigned long values */

#if _MIPS_SZLONG == 32

#define set_atom_long(atom, val) (atom)->l = (long)(val)
#define set_atom_ulong(atom, val) (atom)->ul = (unsigned long)(val)

#elif _MIPS_SZLONG == 64

#define set_atom_long(atom, val) (atom)->ll = (long)(val)
#define set_atom_ulong(atom, val) (atom)->ull = (unsigned long)(val)

#else
    bozo
#endif


#endif
