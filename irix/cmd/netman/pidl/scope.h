#ifndef SCOPE_H
#define SCOPE_H
/*
 * PIDL symbol tables.  Symbol addresses must not change when a table
 * grows because the parse tree contains symbol pointers.  Therefore we
 * chain dynamically allocated symbol structures, and grow the table by
 * reallocating the hash bucket array when the load factor nears unity.
 */
#include "address.h"
#include "type.h"

enum symtype {
	stNil,			/* string or unknown symbol type */
	stProtocol,		/* protocol descriptor */
	stField,		/* protocol or struct field */
	stElement,		/* array element descriptor */
	stBaseType,		/* base type */
	stEnum,			/* enumerated type */
	stStruct,		/* structure type */
	stTypeDef,		/* derived type */
	stMacro,		/* string-valued name definition */
	stNickname,		/* protocol pathname shorthand */
	stNumber,		/* int-valued enumerator */
	stAddress		/* protocol address */
};

extern char *symtypes[];	/* symbol type names */

typedef unsigned long hash_t;

struct symlist {
	struct symbol	*sl_head;	/* first element on symbol list */
	struct symbol	*sl_tail;	/* and last (null-terminated) */
};

#define	APPENDSYM(sym, sl) \
	(((sl)->sl_head == 0) ? ((sl)->sl_head = (sl)->sl_tail = (sym)) \
	 : ((sl)->sl_tail = (sl)->sl_tail->s_next = (sym)))

#define	INSERTSYM(sym, sl) \
	((sym)->s_next = (sl)->sl_head, \
	 (sl)->sl_head = ((sl)->sl_head == 0) ? ((sl)->sl_tail = (sym)) : (sym))

typedef struct symbol {
	enum symtype		s_type;		/* union discriminant */
	char			*s_name;	/* name string in safe store */
	unsigned short		s_namlen;	/* name length */
	unsigned short		s_flags;	/* symbol flags, see below */
	hash_t			s_hash;		/* hash function value */
	struct symbol		*s_chain;	/* next on hash chain */
	struct symbol		*s_next;	/* next on symbol list */
	struct scope		*s_scope;	/* containing scope */
	union {					/* symbol value union */
	    struct {
		struct treenode	*tree;		/* protocol or field decl */
		char		*title;		/* protocol or field title */
		unsigned short	index:12;	/* index in *fields[] array */
		unsigned short	level:4;	/* field verbosity level */
		unsigned short	fid;		/* field identifier */
	    } decl;
	    struct {
		struct treenode	*tree;		/* array field declaration */
		typeword_t	etype;		/* array element type */
		short		ewidth;		/* width of this element */
		unsigned short	eid;		/* element field identifier */
		struct treenode	*dimlen;	/* dimension length if array */
	    } elem;
	    struct {
		struct treenode	*tree;		/* type definition tree */
		unsigned short	tsymndx:TSBITS;	/* type symbol index */
		unsigned short	trefcnt;	/* type reference count */
	    } type;
	    struct treenode	*tree;		/* abstract syntax tree */
	    struct typedesc	*desc;		/* base type descriptor */
	    struct pragma	*pragmas;	/* pragmatic directives */
	    long		val;		/* constant enumerator value */
	    Address		addr;		/* address byte-string */
	} s_data;
} Symbol;

#define	s_title		s_data.decl.title
#define	s_index		s_data.decl.index
#define	s_level		s_data.decl.level
#define	s_fid		s_data.decl.fid
#define	s_etype		s_data.elem.etype
#define	s_ewidth	s_data.elem.ewidth
#define	s_eid		s_data.elem.eid
#define	s_dimlen	s_data.elem.dimlen
#define	s_tsymndx	s_data.type.tsymndx
#define	s_trefcnt	s_data.type.trefcnt
#define	s_tree		s_data.tree
#define	s_desc		s_data.desc
#define	s_pragmas	s_data.pragmas
#define	s_val		s_data.val
#define	s_addr		s_data.addr

/* symbol flags */
#define	sfSaveName	0x0001		/* dynamically allocate name */
#define	sfTypeName	0x0002		/* lexical type-name */
#define	sfImported	0x0004		/* imported from a module */
#define	sfExported	0x0008		/* exported to other modules */
#define	sfFooter	0x0010		/* protocol footer symbol */
#define	sfOverloaded	0x0020		/* field name is multiply declared */
#define	sfVariantSize	0x0040		/* overloaded field has variant size */
#define	sfTemporary	0x0080		/* struct pointer temporary */
#define	sfDirect	0x0100		/* struct temporary (no indirection) */
#define	sfCantInline	0x0200		/* protocol or struct not inlineable */
#define	sfBitFields	0x0400		/* protocol or struct has bitfields */
#define	sfAnonymous	0x0800		/* generated, anonymous field name */
#define	sfDynamicNest	0x1000		/* protocol has nest statements */

typedef struct scope {
	unsigned short		s_length;	/* hash scope logical size */
	unsigned short		s_shift;	/* multiplicative hash shift */
	unsigned short		s_numfree;	/* number of free symbols */
	unsigned short		s_numfields;	/* number of fields in scope */
	Symbol			**s_table;	/* hash bucket vector */
	char			*s_name;	/* name of this scope */
	Symbol			*s_sym;		/* scope's symbol table entry */
	struct scope		*s_outer;	/* next enclosing scope */
	char			*s_module;	/* file containing this scope */
	char			*s_path;	/* pathname from outermost */
	struct symlists {
		struct symlist	consts;		/* stNumber&stAddress consts */
		struct symlist	elements;	/* array element descriptors */
		struct symlist	enums;		/* stEnum type definitions */
		struct symlist	fields;		/* protocol or struct fields */
		struct symlist	macros;		/* stMacro symbols */
		struct symlist	nicknames;	/* nested protocol macros */
		struct symlist	protocols;	/* stProtocol entries */
		struct symlist	structs;	/* stStruct type definitions */
		struct symlist	typedefs;	/* typedef'd names */
	} s_lists;
#ifdef METERING
	struct scopemeter {
		unsigned long	searches;	/* hash scope searches */
		unsigned long	probes;		/* probes on collision */
		unsigned long	hits;		/* lookups which found name */
		unsigned long	misses;		/* names not found */
		unsigned long	adds;		/* entries added */
		unsigned long	grows;		/* table expansions */
	} s_meter;
#endif
} Scope;

#define	s_consts	s_lists.consts
#define	s_elements	s_lists.elements
#define	s_enums		s_lists.enums
#define	s_fields	s_lists.fields
#define	s_macros	s_lists.macros
#define	s_nicknames	s_lists.nicknames
#define	s_protocols	s_lists.protocols
#define	s_structs	s_lists.structs
#define	s_typedefs	s_lists.typedefs

extern void	initscope(Scope *, char *, Symbol *, Scope *);
extern void	copyscope(Scope *, Scope *);
extern void	freescope(Scope *);
extern hash_t	hashname(char *, int);
typedef Symbol	*(*lookfun_t)(Scope *, char *, int, hash_t);
extern Symbol	*lookup(Scope *, char *, int, hash_t);
extern Symbol	*find(Scope *, char *, int, hash_t);
extern Symbol	*install(Scope *, Symbol *, enum symtype, int);
extern Symbol	*enter(Scope *, char *, int, hash_t, enum symtype, int);
extern int	isnickname(Scope *, Symbol *);

#endif
