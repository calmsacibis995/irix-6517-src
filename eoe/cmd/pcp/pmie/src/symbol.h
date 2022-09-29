/***********************************************************************
 * symbol.h - a symbol is an object with a name, a value and a
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

/* $Id: symbol.h,v 1.1 1999/04/28 10:06:17 kenmcd Exp $ */

#ifndef SYMBOL_H
#define SYMBOL_H

/***********************************************************************
 * private
 ***********************************************************************/

union symunion;			/* for forward reference */

typedef struct {
    union symunion  *next;	/* forward pointer */
    union symunion  *prev;	/* backward pointer */
    union symunion  *free;	/* free list head */
} SymHdr;

typedef struct {
    union symunion  *ptr;	/* free list pointer */
    int		    count;	/* number of free entries */
} SymFree;

typedef struct {
    char	    *name;	/* name string */
    void	    *value;	/* arbitrary value */
} SymUsed;

typedef struct {
    union {
	SymFree	    free;	/* free symbol table entry */
	SymUsed	    used;	/* occupied symbol table entry */
    } stat;
    int		    refs;	/* refernce count */
} SymEntry;

typedef union symunion {
    SymHdr	    hdr;	/* symbol table or bucket header */
    SymEntry	    entry;	/* symbol or free slot */
} SymUnion;


/***********************************************************************
 * public
 ***********************************************************************/

#define SYM_NULL NULL
typedef SymUnion *Symbol;
typedef SymUnion SymbolTable;

/* access to name string, value and reference count */
#define symName(sym)  ((sym)->entry.stat.used.name)
#define symValue(sym) ((sym)->entry.stat.used.value)
#define symRefs(sym)  ((sym)->entry.refs)

/* initialize symbol table */
void symSetTable(SymbolTable *);

/* reset symbol table */
void symClearTable(SymbolTable *);

/* convert string to symbol */
Symbol symIntern(SymbolTable *, char *);

/* lookup symbol by name */
Symbol symLookup(SymbolTable *, char *);

/* copy symbol */
Symbol symCopy(Symbol);

/* remove reference to symbol */
void symFree(Symbol);

#endif /* SYMBOL_H */

