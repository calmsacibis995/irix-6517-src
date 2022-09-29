/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: sec_kmalloc.c,v 65.5 1998/04/01 14:17:08 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_kmalloc.c,v $
 * Revision 65.5  1998/04/01 14:17:08  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for sec_cred_internal.h.  Changed the return value
 * 	for mkmalloc() from char pointer to void pointer.  Changed
 * 	argument for mkfree() from char pointer to void pointer.  Removed
 * 	non-kernel code.  This code is only used in the kernel.
 *
 * Revision 65.4  1998/03/23  17:32:22  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 18:14:16  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  20:19:00  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:30:04  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  16:00:19  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  20:09:44  dce
 * *** empty log message ***
 *
 * Revision 1.1  1995/08/04  19:32:00  dcebuild
 * Initial revision
 *
 * Revision 1.1.2.2  1994/10/06  20:29:20  agd
 * 	expand copyright
 * 	[1994/10/06  14:27:49  agd]
 *
 * Revision 1.1.2.1  1994/08/09  17:32:39  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/09  17:01:52  burati]
 * 
 * $EndLog$
 */

/* sec_kmalloc.c
 * Internal kernel malloc support routines for kernel security modules.
 *
 * Copyright (c) Hewlett-Packard Company 1994
 * Unpublished work. All Rights Reserved.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sec_cred_internal.h>

/* Wrapper for kmalloc and kfree */
void *mkmalloc(unsigned int size)
{
    return (kern_malloc(size));
}
	
void mkfree(void *a)
{
    kern_free(a);
}
