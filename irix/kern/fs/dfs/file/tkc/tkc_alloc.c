/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkc_alloc.c,v 65.4 1998/03/23 16:27:16 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1994, 1989 Transarc Corporation - All rights reserved */

#include <tkc.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc_alloc.c,v 65.4 1998/03/23 16:27:16 gwehrman Exp $")

/*
 * Poor man's fixed size storage allocation: Speedy by avoiding calls to osi_{Alloc/Free}
 * whenever possible is the key goal here.
 */
			/* Size of largest struct alloced (tkc_tokenList) */
#define	MIN_ALLOCSIZE	(sizeof(struct tkc_tokenList))


static struct tkcSpace {
    struct tkcSpace *next;
} *freeSpaceList = 0;
#ifdef SGIMIPS
/* For easy debugging... */
struct lock_data tcalloc_lock;
#else
static struct lock_data tcalloc_lock;
#endif /* SGIMIPS */


/* 
 * tkc_InitSpace -- Initialize space allocator. 
 */
void tkc_InitSpace()
{
    lock_Init(&tcalloc_lock);
}

/*
 * When no more available space in <freeSpaceList> allocate it via osi_Alloc
 */
char * tkc_AllocSpace() {
    register struct tkcSpace *tcp;

    lock_ObtainWrite(&tcalloc_lock);
    tkc_stats.spaceAlloced++;
    if (!freeSpaceList) {
	lock_ReleaseWrite(&tcalloc_lock);
	return (char *)osi_Alloc(MIN_ALLOCSIZE);
    }
    tcp = freeSpaceList;
    freeSpaceList = tcp->next;
    lock_ReleaseWrite(&tcalloc_lock);
    return (char *) tcp;
}


/*
 * Put the space back to <freeSpaceList> for later consumption
 */
void tkc_FreeSpace(datap)
    register char *datap;    
{
    lock_ObtainWrite(&tcalloc_lock);
    tkc_stats.spaceAlloced--;
    ((struct tkcSpace *)datap)->next = freeSpaceList;
    freeSpaceList = ((struct tkcSpace *)datap);
    lock_ReleaseWrite(&tcalloc_lock);
}
