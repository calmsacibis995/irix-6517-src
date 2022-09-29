#ifndef SCOPE_H
#define SCOPE_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Name spaces.  A name can be an arbitrary byte string.
 */
#include "address.h"
#include "strings.h"

enum symtype {
	SYM_FREE,		/* unallocated symbol */
	SYM_ADDRESS,		/* protocol address */
	SYM_NUMBER,		/* int-valued enumerator */
	SYM_MACRO,		/* string-valued name definition */
	SYM_FUNCTION,		/* protocol-specific function */
	SYM_PROTOCOL,		/* named protocol record */
	SYM_FIELD,		/* protocol field record */
	SYM_OPAQUE		/* void data pointer */
};

#define	MAXMACROARGS	8	/* maximum # of macro args */

struct macrodef {
	struct string		md_string;	/* macro definition string */
	int			md_nargs;	/* number of formal arguments */
	struct exprsource	*md_src;	/* ex_parse source record */
};

struct funcdef {
	int	(*fd_func)();	/* function address */
	int	fd_nargs;	/* number of arguments */
	char	*fd_desc;	/* brief description */
};

struct nestedproto {
	struct protocol	*np_proto;	/* protocol interface struct */
	long		np_prototype;	/* nested type code */
};

typedef struct symbol {
	enum symtype		sym_type;	/* union discriminant */
	char			*sym_name;	/* name string in safe store */
	unsigned short		sym_namlen;	/* name length */
	unsigned short		sym_hashcol:1;	/* hash collision */
	union {					/* value union */
	    union address	symu_addr;	/* address byte-string */
	    long		symu_val;	/* constant enumerator value */
	    struct macrodef	symu_def;	/* macro definition string */
	    struct funcdef	symu_func;	/* protocol function */
	    struct nestedproto	symu_nested;	/* nested protocol info */
	    struct protofield	*symu_field;	/* packet field descriptor */
	    void		*symu_data;	/* opaque data */
	} sym_u;
} Symbol;

#define	sym_addr	sym_u.symu_addr
#define	sym_val		sym_u.symu_val
#define	sym_def		sym_u.symu_def
#define	sym_func	sym_u.symu_func
#define	sym_proto	sym_u.symu_nested.np_proto
#define	sym_prototype	sym_u.symu_nested.np_prototype
#define	sym_field	sym_u.symu_field
#define	sym_data	sym_u.symu_data

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
Symbol	*sc_lookupsym(struct scope *, char *, int);
Symbol	*sc_addsym(struct scope *, char *, int, enum symtype);
void	sc_deletesym(struct scope *, char *, int);

#endif
