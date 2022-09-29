/***********************************************************************
 * symbol.c - a symbol is an object with a name, a value and a
 *            reference count
 ***********************************************************************
 *
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

#ident "$Id: symbol.c,v 1.1 1999/04/28 10:06:17 kenmcd Exp $"

#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "dstruct.h"
#include "pmapi.h"


/***********************************************************************
 * constants
 ***********************************************************************/

/* bucket memory alignment, address mask and size */
#define ALIGN	1024
#define MASK	(ALIGN - 1)
#define BSIZE	(ALIGN / sizeof(SymUnion))



/***********************************************************************
 * types
 ***********************************************************************/

typedef SymUnion Bucket[BSIZE]; 



/***********************************************************************
 * local functions
 ***********************************************************************/

static void
addBucket(SymUnion *st)
{
    SymUnion *bckt = (SymUnion *) aalloc(ALIGN, sizeof(Bucket));
    SymUnion *scoop = bckt + 1;

    bckt->hdr.prev = st;
    bckt->hdr.next = st->hdr.next;
    bckt->hdr.free = scoop;
    st->hdr.next->hdr.prev = bckt;
    st->hdr.next = bckt;
    scoop->entry.refs = 0;
    scoop->entry.stat.free.ptr = NULL;
    scoop->entry.stat.free.count = BSIZE - 1;
}

static void
remBucket(SymUnion *bckt)
{
    SymUnion *prev = bckt->hdr.prev;
    SymUnion *next = bckt->hdr.next;
    SymUnion *scan;
    int      i = 1;

    prev->hdr.next = next;
    next->hdr.prev = prev;
    while (i < BSIZE) {
	scan = bckt + i;
	if (scan->entry.refs == 0)
	    i += scan->entry.stat.free.count;
	else {
	    free(scan->entry.stat.used.name);
	    i++;
	}
    }
    free(bckt);
}



/***********************************************************************
 * exported functions
 ***********************************************************************/

/* initialize symbol table */
void
symSetTable(SymbolTable *st)
{
    /* start with one clean bucket */
    st->hdr.next = st;
    st->hdr.prev = st;
    addBucket(st);
}


/* reset symbol table */
void
symClearTable(SymbolTable *st)
{
    /* unchain all buckets and free storage */
    while (st->hdr.next != st)
	remBucket(st->hdr.next);
    /* start with one clean bucket */
    addBucket(st);
}


/* Convert string to symbol.  A copy of the name string is made
   on the heap for use by the symbol. */
Symbol
symIntern(SymbolTable *st, char *name)
{
    SymUnion *bckt;
    SymUnion *scoop;
    char     *copy;
    int      i;

    /* pick up existing symbol */
    bckt = st->hdr.next;
    while (bckt != st) {
        i = 1;
	while (i < BSIZE) {
	    scoop = bckt + i;
	    if (scoop->entry.refs) {
		if (strcmp(name, scoop->entry.stat.used.name) == 0) {
		    scoop->entry.refs++;
		    return scoop;
		}
		i++;
	    }
	    else
		i += scoop->entry.stat.free.count;
	}
        bckt = bckt->hdr.next;
    }

    /* pick up free entry */
    bckt = st->hdr.next;
    while (bckt != st) {
	if (scoop = bckt->hdr.free) {
	    if (scoop->entry.stat.free.count > 1)
		scoop += --scoop->entry.stat.free.count;
	    else
		bckt->hdr.free = scoop->entry.stat.free.ptr;
	    break;
	}
	bckt = bckt->hdr.next;
    }

    /* no free entry - allocate new bucket */
    if (scoop == NULL) {
	addBucket(st);
	scoop = st->hdr.next + 1;
	scoop += --scoop->entry.stat.free.count;
    }

    /* initialize symbol */
    scoop->entry.refs = 1;
    copy = (char *) alloc(strlen(name) + 1);
    strcpy(copy, name);
    scoop->entry.stat.used.name = copy;
    scoop->entry.stat.used.value = NULL;
    return scoop;
}


/* lookup symbol by name */
Symbol
symLookup(SymbolTable *st, char *name)
{
    SymUnion *bckt;
    SymUnion *scoop;
    int      i;

    bckt = st->hdr.next;
    while (bckt != st) {
        i = 1;
	while (i < BSIZE) {
	    scoop = bckt + i;
	    if (scoop->entry.refs) {
		if (strcmp(name, scoop->entry.stat.used.name) == 0) {
		    scoop->entry.refs++;
		    return scoop;
		}
		i++;
	    }
	    else
		i += scoop->entry.stat.free.count;
	}
        bckt = bckt->hdr.next;
    }
    return NULL;
}


/* copy symbol */
Symbol
symCopy(Symbol sym)
{
    sym->entry.refs++;
    return sym;
}


/* remove reference to symbol */
void
symFree(Symbol sym)
{
    SymUnion *bckt;
    SymUnion *lead, *lag;

    if ((sym != SYM_NULL) && (--sym->entry.refs <= 0)) {

	/* free up name string BUT NOT value */
	free(sym->entry.stat.used.name);

	/* find correct place in ordered free list */
	bckt = (SymUnion *) ((char *) sym - ((long) sym & MASK));
	lead = bckt->hdr.free;
	lag = NULL;
	while ((lead != NULL) && (lead < sym)) {
	    lag = lead;
	    lead = lead->entry.stat.free.ptr;
	}

	/* coalesce with preceding free block */
	if (lag + lag->entry.stat.free.count == sym) {
	    lag->entry.stat.free.count++;
	    sym = lag;
	}

	/* link up as single free entry */
	else {
	    if (lag)
		lag->entry.stat.free.ptr = sym;
	    else
		bckt->hdr.free = sym;
	    sym->entry.stat.free.count = 1;
	    sym->entry.stat.free.ptr = lead;
	}

	/* coalesce with following free block */
	if (sym + sym->entry.stat.free.count == lead) {
	    sym->entry.stat.free.count += lead->entry.stat.free.count;
	    sym->entry.stat.free.ptr = lead->entry.stat.free.ptr;
	}
    }
}


