/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL symbol tables.  Machine Dependent: we assume that hash_t is 32 bits.
 */
#include <bstring.h>
#include <values.h>
#include "debug.h"
#include "heap.h"
#include "macros.h"
#include "pidl.h"
#include "scope.h"
#include "strings.h"

char *symtypes[] = {
	"unknown",
	"protocol",
	"field",
	"element",
	"base type",
	"enum",
	"struct",
	"typedef",
	"macro",
	"nickname",
	"constant integer",
	"constant address"
};

/*
 * Multiplicative hash with open chaining.  PHI is (golden ratio * 2**32).
 * The hash function accumulates a 32-bit key by adding each character in
 * the string key into a rotating accumulator.  The rotor constant RHO is
 * chosen based on the observed mean string length of ~5.4 and the 32 bit
 * hash_t limit.
 */
#define	PHI	((hash_t) 2654435769)
#define	RHO	6

hash_t
hashname(char *name, int namlen)
{
	hash_t hash;

	for (hash = 0; --namlen >= 0; name++)
		hash = (hash << RHO) + (hash >> (BITS(hash_t) - RHO)) + *name;
	return PHI * hash;
}

/*
 * Initialize a multiplicative hash table.  Allocate at least a 16 entry
 * table, to avoid collisions that are likely when taking only the first
 * few bits of the hash function's value for poorly distributed strings.
 * Waste 1/8 of the table for the free sentinel and a good alpha.
 */
#define	MINLOG2		4
#define	ALPHA(len)	((len) - (len) / 8)

void
initscope(Scope *s, char *name, Symbol *sym, Scope *outer)
{
	unsigned int count;

	count = 1 << MINLOG2;
	s->s_length = count;
	s->s_shift = BITS(hash_t) - MINLOG2;
	s->s_numfree = ALPHA(count);
	s->s_numfields = 0;
	s->s_table = vnew(count, Symbol *);
	bzero(s->s_table, count * sizeof s->s_table[0]);
	s->s_name = name;
	s->s_sym = sym;
	s->s_outer = outer;
	s->s_module = yyfilename;
	s->s_path = 0;
	bzero(&s->s_lists, sizeof s->s_lists);
#ifdef METERING
	bzero(&s->s_meter, sizeof s->s_meter);
#endif
}

void
copyscope(Scope *from, Scope *to)
{
	int count;
	Symbol **sp, *sym, *nsym;
	struct symlist *sl;

	count = from->s_length;
	to->s_length = count;
	to->s_shift = from->s_shift;
	to->s_numfree = from->s_numfree;
	to->s_numfields = from->s_numfields;
	to->s_table = vnew(count, Symbol *);
	to->s_name = from->s_name;
	to->s_sym = from->s_sym;
	to->s_outer = from->s_outer;
	to->s_module = from->s_module;
	if (from->s_path)
		to->s_path = strdup(from->s_path);
	else
		to->s_path = 0;
	for (sp = from->s_table; --count >= 0; sp++) {
		for (sym = *sp; sym; sym = sym->s_chain) {
			nsym = install(to, sym, sym->s_type, sym->s_flags);
			nsym->s_data = sym->s_data;
			switch (nsym->s_type) {
			  case stNumber:
				sl = &to->s_consts;
				break;
			  case stElement:
				sl = &to->s_elements;
				break;
			  case stEnum:
				sl = &to->s_enums;
				break;
			  case stField:
				sl = &to->s_fields;
				break;
			  case stMacro:
				sl = &to->s_macros;
				break;
			  case stNickname:
				sl = &to->s_nicknames;
				break;
			  case stProtocol:
				sl = &to->s_protocols;
				break;
			  case stStruct:
				sl = &to->s_structs;
				break;
			  case stTypeDef:
				sl = &to->s_typedefs;
			}
			APPENDSYM(nsym, sl);
		}
	}
#ifdef METERING
	bcopy(&from->s_meter, &to->s_meter, sizeof to->s_meter);
#endif
}

void
freescope(Scope *s)
{
	int count;
	Symbol **sp, *sym;

	count = s->s_length;
	for (sp = s->s_table; --count >= 0; sp++) {
		while ((sym = *sp) != 0) {
			*sp = sym->s_chain;
			if (sym->s_flags & sfSaveName)
				delete(sym->s_name);
			delete(sym);
		}
	}
	delete(s->s_table);
	delete(s->s_path);
	bzero(s, sizeof *s);
}

/*
 * Return the address of the pointer to name's entry if it exists, otherwise
 * of the null pointer to set.
 */
static Symbol **
search(Scope *s, char *name, int namlen, hash_t hash)
{
	Symbol **sp, *sym;

	METER(s->s_meter.searches++);
	for (sp = &s->s_table[hash >> s->s_shift]; (sym = *sp) != 0;
	     sp = &sym->s_chain) {
		if (namlen == sym->s_namlen
		    && !strncmp(name, sym->s_name, namlen)) {
			break;
		}
		METER(s->s_meter.probes++);
	}
	return sp;
}

Symbol *
lookup(Scope *s, char *name, int namlen, hash_t hash)
{
	Symbol *sym;
	
	sym = *search(s, name, namlen, hash);
	METER(sym ? s->s_meter.hits++ : s->s_meter.misses++);
	return sym;
}

Symbol *
find(Scope *s, char *name, int namlen, hash_t hash)
{
	Symbol *sym;

	do {
		sym = lookup(s, name, namlen, hash);
		if (sym)
			return sym;
	} while ((s = s->s_outer) != 0);
	return 0;
}

Symbol *
add(Scope *s, char *name, int namlen, hash_t hash, enum symtype type,
    int flags, Symbol **chainp)
{
	Symbol *sym;

	if (s->s_numfree == 0) {
		Symbol **sp, **oldtable, **bucket;
		int count, length;

		/*
		 * Hash table is full, so double its size and re-insert all
		 * active entries and the new one.
		 */
		METER(s->s_meter.grows++);
		count = s->s_length;
		length = 2 * count;
		s->s_length = length;
		s->s_numfree = count;
		--s->s_shift;
		oldtable = s->s_table;
		s->s_table = vnew(length, Symbol *);
		bzero(s->s_table, length * sizeof s->s_table[0]);
		for (sp = oldtable; --count >= 0; sp++) {
			while ((sym = *sp) != 0) {
				*sp = sym->s_chain;
				bucket = &s->s_table[sym->s_hash >> s->s_shift];
				sym->s_chain = *bucket;
				*bucket = sym;
			}
		}
		delete(oldtable);
		chainp = search(s, name, namlen, hash);
	}

	METER(s->s_meter.adds++);
	assert(s->s_numfree > 0);
	--s->s_numfree;
	sym = new(Symbol);
	sym->s_type = type;
	sym->s_name = (flags & sfSaveName) ? strndup(name, namlen) : name;
	sym->s_namlen = namlen;
	sym->s_flags = flags;
	sym->s_hash = hash;
	sym->s_next = 0;
	sym->s_scope = s;
	sym->s_chain = *chainp;
	*chainp = sym;
	return sym;
}

Symbol *
install(Scope *s, Symbol *sym, enum symtype type, int flags)
{
	hash_t hash;

	hash = sym->s_hash;
	return add(s, sym->s_name, sym->s_namlen, hash, type, flags,
		   &s->s_table[hash >> s->s_shift]);
}

Symbol *
enter(Scope *s, char *name, int namlen, hash_t hash, enum symtype type,
      int flags)
{
	Symbol **sp, *sym;

	sp = search(s, name, namlen, hash);
	sym = *sp;
	if (sym)
		return sym;
	return add(s, name, namlen, hash, type, flags, sp);
}

/*
 * Return 1 if def is a protocol pathname beginning with s, 0 otherwise.
 */
int
isnickname(Scope *s, Symbol *def)
{
	Symbol *sym;
	char *dname, dnext;
	int namlen;

	sym = s->s_sym;
	if (sym->s_type != stProtocol)
		return 0;
	dname = def->s_name;
	namlen = sym->s_namlen;
	if (strncasecmp(sym->s_name, dname, namlen))
		return 0;
	dnext = dname[namlen];
	return dnext == '.' || dnext == '\0';
}
