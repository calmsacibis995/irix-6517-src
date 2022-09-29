#ifndef ANALYZE_H
#define ANALYZE_H
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Analysis pass declarations.
 */
#include "scope.h"

/*
 * A context holds current state computed during the syntax tree walk.
 */
typedef struct context {
	Symbol			*cx_modsym;	/* current module's symbol */
	unsigned short		cx_flags;	/* context flags, see below */
	unsigned short		cx_sflags;	/* current symbol flags */
	struct treenode		*cx_proto;	/* current protocol */
	Scope			*cx_scope;	/* current innermost scope */
	lookfun_t		cx_lookfun;	/* symbol lookup function */
	Symbol			*cx_looksym;	/* lookup result symbol */
	unsigned short		cx_bitoff;	/* running frame bit offset */
	unsigned short		cx_bbcount;	/* basic block count */
	struct basicblock	*cx_block;	/* basic block pointer */
	struct treenode		*cx_guard;	/* guarded member condition */
} Context;

#define	cxHeaderFile	0x0001		/* context exports names */
#define	cxCacheFields	0x0002		/* context declares caches */
#define	cxEnumFields	0x0004		/* context has enum fields */
#define	cxHeapFields	0x0008		/* dynamically allocated fields */

extern int	analyze(struct treenode *, Context *);

#endif
