#ifndef ENUM_H
#define ENUM_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Enumerated type tables.
 *
 * NB:	1. EI uses the string-ization feature of Reiser's cpp.
 *	2. en_name may return a pointer to static store.
 */

#include "index.h"		/* to get typedef Index */

struct scope;
struct __xdr_s;

typedef struct enumerator {
	char	*e_name;	/* constant string name */
	int	e_namlen;	/* and name length */
	int	e_val;		/* integer value */
} Enumerator;

#ifdef __STDC__
#define	EI(name) \
	{ # name, constrlen(# name), (int) name }
#else
#define	EI(name) \
	{ "name", constrlen("name"), (int) name }
#endif
#define	EI_VAL(name, val) \
	{ name, constrlen(name), (int) val }

typedef struct enumeration {
	int		en_min;		/* least enumerator value */
	int		en_max;		/* greatest enumerator value */
	unsigned int	en_last;	/* last index in dense vector */
	Index		*en_index;	/* hash table if sparse */
	Enumerator	*en_vec;	/* name-value pair vector */
} Enumeration;

/*
 * Enumeration init, finish, and value-to-name operations.
 */
void	en_initscope(Enumeration *, Enumerator *, int, struct scope *);
void	en_finish(Enumeration *);
char	*en_lookup(Enumeration *, int);
char	*en_name(Enumeration *, int);

/*
 * Given a vector of enumerators, add each element to a scope.
 * XXXbe mislocated?
 */
void	sc_addnumbers(struct scope *, Enumerator *, int);

/*
 * Compatibility defines for original, only-for-protocol-scope ops.
 */
#define	en_init(en, vec, len, pr) \
	en_initscope(en, vec, len, &(pr)->pr_scope)
#define	pr_addnumbers(pr, vec, len) \
	sc_addnumbers(&(pr)->pr_scope, vec, len)

/*
 * Given an enumerator vector and a bitset, return a pointer to static store
 * containing the space-separated bit names.
 */
char	*en_bitset(Enumerator *, int, int);

/*
 * PIDL compiler runtime support.
 */
extern Enumeration _bool;

#endif
