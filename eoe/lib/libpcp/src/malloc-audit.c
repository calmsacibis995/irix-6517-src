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

#ident "$Id: malloc-audit.c,v 2.3 1997/12/18 06:35:05 markgw Exp $"

/*
 * Audit malloc/realloc/free etc.
 */

#ifdef MALLOC_AUDIT
#include "no-malloc-audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef struct x {
    struct x	*next;	/* linked list of these from root_at and free_at */
    char	*file;	/* caller's source file */
    int		line;	/* caller's source line number */
    void	*addr;	/* addr from alloc or realloc */
    int		size;	/* size from alloc or realloc */
    int		state;	/* S_PERSIST if marked persistent */
} audit_t;

#define S_DYNAMIC	0
#define S_PERSIST	1

static audit_t	*root_at;
static audit_t	*free_at;
static int	err_cnt;

/*
 * useful for getting dbx to stop here ...
 */
static void
_malloc_error_(void)
{
    err_cnt++;
}

static audit_t *
new(void)
{
    audit_t	*ap;

    if (free_at == NULL) {
	ap = (audit_t *)sbrk(sizeof(*ap));
	if (ap == NULL) {
	    perror("MALLOC AUDIT: sbrk failed");
	    abort();
	}
	ap->next = NULL;
    }
    else {
	ap = free_at;
	free_at = free_at->next;
    }
    ap->next = root_at;
    root_at = ap;
    ap->state = S_DYNAMIC;
    return ap;
}

/*
 * call (typically at exit) to dump trace of any unfreed blocks
 */
void _malloc_audit_(void)
{
    audit_t	*ap;
    int		first;

    if (err_cnt)
	fprintf(stderr, "MALLOC AUDIT: %d errors detected\n", err_cnt);
    if (root_at != NULL) {
	for (ap = root_at; ap != NULL; ap = ap->next) {
	    if (ap->state == S_PERSIST)
		continue;
	    if (first) {
		fprintf(stderr, "MALLOC AUDIT: allocated, but not freed ...\n");
		first = 0;
	    }
	    fprintf(stderr, "MALLOC AUDIT: %5d bytes @ 0x%x from %s:%d\n",
		ap->size, ap->addr, ap->file, ap->line);
	}
    }
}

/*
 * forget about all allocations up to this point in time
 */
void _malloc_reset_(void)
{
    audit_t	*ap;
    audit_t	*priorp;

    for (ap = root_at, priorp = NULL; ap != NULL; ap = ap->next)
	priorp = ap;

    if (priorp != NULL) {
	priorp->next = free_at;
	free_at = root_at;
	root_at = NULL;
    }
}

/*
 * mark this addr as persistent, so _malloc_free_ checks, but _malloc_audit_
 * does not report it
 */
void _malloc_persistent_(void *ptr, char *file, int line)
{
    audit_t	*ap;

    for (ap = root_at; ap != NULL; ap = ap->next) {
	if (ap->addr == ptr) {
	    ap->state = S_PERSIST;
	    return;
	}
    }
    fprintf(stderr, "MALLOC AUDIT: %s:%d tried to mark non-malloc'd area at 0x%08x persistent\n", file, line, ptr);
    _malloc_error_();
}

void *_malloc_(size_t size, char *file, int line)
{
    audit_t	*ap = new();

    ap->file = file;
    ap->line = line;
    ap->size = size;
    ap->addr = malloc(size);
    return ap->addr;
}

void _free_(void *ptr, char *file, int line)
{
    audit_t	*ap;
    audit_t	*priorp;

    for (ap = root_at, priorp = NULL; ap != NULL; ap = ap->next) {
	if (ap->addr == ptr) {
	    free(ptr);
	    if (priorp == NULL)
		root_at = ap->next;
	    else
		priorp->next = ap->next;
	    ap->next = free_at;
	    free_at = ap;
	    return;
	}
	priorp = ap;
    }
    fprintf(stderr, "MALLOC AUDIT: %s:%d tried to free non-malloc'd area at 0x%08x\n", file, line, ptr);
    _malloc_error_();
    free(ptr);
}

void *_realloc_(void *ptr, size_t size, char *file, int line)
{
    audit_t	*ap;

    if (ptr == NULL) {
	/* special case */
	return _malloc_(size, file, line);
    }

    for (ap = root_at; ap != NULL; ap = ap->next) {
	if (ap->addr == ptr) {
	    ap->file = file;
	    ap->line = line;
	    ap->addr = realloc(ptr, size);
	    ap->size = size;
	    return ap->addr;
	}
    }
    fprintf(stderr, "MALLOC AUDIT: %s:%d tried to realloc non-malloc'd area at 0x%08x\n", file, line, ptr);
    _malloc_error_();
    return realloc(ptr, size);
}

void *_calloc_(size_t nelem, size_t elsize, char *file, int line)
{
    audit_t	*ap = new();

    ap->file = file;
    ap->line = line;
    ap->size = nelem * elsize;
    ap->addr = calloc(nelem, elsize);
    return ap->addr;
}

void *_memalign_(size_t alignment, size_t size, char *file, int line)
{
    audit_t	*ap = new();

    ap->file = file;
    ap->line = line;
    ap->size = size;
    ap->addr = memalign(alignment, size);
    return ap->addr;
}

void *_valloc_(size_t size, char *file, int line)
{
    audit_t	*ap = new();

    ap->file = file;
    ap->line = line;
    ap->size = size;
    ap->addr = valloc(size);
    return ap->addr;
}

char *_strdup_(char *s1, char *file, int line)
{
    audit_t	*ap = new();

    ap->file = file;
    ap->line = line;
    ap->size = strlen(s1);
    ap->addr = strdup(s1);
    return ap->addr;
}

#endif
