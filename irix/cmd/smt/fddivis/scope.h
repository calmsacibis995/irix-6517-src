#ifndef SCOPE_H
#define SCOPE_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Name spaces.  A name can be an arbitrary byte string.
 */

enum symtype {
	SYM_FREE,		/* unallocated symbol */
	SYM_INUSE,		/* allocated */
};


typedef struct symbol {
	enum symtype		sym_type;	/* union discriminant */
	char			*sym_name;	/* name string in safe store */
	unsigned short		sym_namlen;	/* name length */
	unsigned short		sym_hashcol:1;	/* hash collision */
	void *			sym_data;	/* data */
} Symbol;


typedef struct scope {
	unsigned int		sc_length;	/* hash table logical size */
	unsigned int		sc_numfree;	/* number of free symbols */
	unsigned int		sc_shift;	/* multiplicative hash shift */
	struct symbol		*sc_table;	/* base of dynamic hash table */
	char			*sc_name;	/* name for debugging */
#ifdef METERING
	struct scopemeter {
		unsigned long	sm_searches;	/* hash table searches */
		unsigned long	sm_probes;	/* linear probes on collision */
		unsigned long	sm_hits;	/* lookups which found name */
		unsigned long	sm_misses;	/* names not found */
		unsigned long	sm_adds;	/* entries added */
		unsigned long	sm_grows;	/* table expansions */
		unsigned long	sm_shrinks;	/* table contractions */
		unsigned long	sm_deletes;	/* entries removed */
		unsigned long	sm_dprobes;	/* entries hashed by delete */
		unsigned long	sm_dmoves;	/* entries moved by delete */
	} sc_meter;
#endif
} Scope;

Scope	*scope(int, char *);
void	sc_destroy(struct scope *);
void	sc_init(struct scope *, int, char *);
void	sc_finish(struct scope *);
Symbol	*sc_lookupsym(struct scope *, void *, int);
Symbol	*sc_addsym(struct scope *, void *, int, enum symtype);
void	sc_deletesym(struct scope *, void *, int);

#endif
