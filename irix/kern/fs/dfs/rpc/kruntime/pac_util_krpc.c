/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: pac_util_krpc.c,v 65.5 1998/03/23 17:26:28 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: pac_util_krpc.c,v $
 * Revision 65.5  1998/03/23 17:26:28  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:20:49  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:43  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:27  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.62.2  1996/02/18  00:00:47  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:50  marty]
 *
 * Revision 1.1.62.1  1995/12/08  00:15:19  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:26  root]
 * 
 * Revision 1.1.60.1  1994/01/21  22:32:17  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:32  cbrooks]
 * 
 * Revision 1.1.4.2  1993/05/12  19:59:22  rsarbo
 * 	migrate changes to user space version of this routine
 * 	into kernel.
 * 	[1993/05/12  19:51:50  rsarbo]
 * 
 * Revision 1.1.2.3  1993/01/03  22:36:40  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:08  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:39:47  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:51  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:16:18  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**
**  NAME
**
**      pac_util_krpc.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**	Utility routine for dealing with pacs in kernel.
**
**
*/

#include <dce/dce.h>
#include <pac_util_krpc.h>

#ifndef NULL
#define NULL 0
#endif /* NULL */

extern void idl_free_krpc(pointer_t);

/* s e c _ id _ p a c _ u t i l _ f r e e
 * 
 * Release dynamic storage associated with a PAC.
 */
void sec_id_pac_util_free
#ifdef _DCE_PROTO_
  (
    sec_id_pac_t *p
  )
#else 
    ( p ) 
    sec_id_pac_t *p;
#endif 
{
    int i;

    if (p->realm.name) {
        idl_free_krpc(p->realm.name);
        p->realm.name = NULL;
    }
    if (p->principal.name) {
        idl_free_krpc(p->principal.name);
        p->principal.name = NULL;
    }
    if (p->group.name) {
        idl_free_krpc(p->group.name);
        p->group.name = NULL;
    }

    if (p->groups) {
        for (i = 0; i < p->num_groups; i++) {
            if (p->groups[i].name) {
                idl_free_krpc(p->groups[i].name);
                p->groups[i].name = NULL;
            }
        }
        p->num_groups = 0;
        idl_free_krpc(p->groups);
        p->groups = NULL;
    }

    if (p->foreign_groups) {
        for (i = 0; i < p->num_foreign_groups; i++) {
            if (p->foreign_groups[i].id.name) {
                idl_free_krpc(p->foreign_groups[i].id.name);
                p->foreign_groups[i].id.name = NULL;
            }
            if (p->foreign_groups[i].realm.name) {
                idl_free_krpc(p->foreign_groups[i].realm.name);
                p->foreign_groups[i].realm.name = NULL;
            }
        }
        p->num_foreign_groups = 0;
        idl_free_krpc(p->foreign_groups);
        p->foreign_groups = NULL;
    }
}

