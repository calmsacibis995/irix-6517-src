/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Name space implementation.  Machine Dependent: we assume that
 * long ints are 32 bits.
 */
#include <bstring.h>
#include <values.h>
#include "debug.h"
#include "heap.h"
#include "macros.h"
#include "scope.h"

Scope *
scope(int count, char *name)
{
	Scope *sc;

	sc = new(Scope);
	sc_init(sc, count, name);
	return sc;
}

void
sc_destroy(Scope *sc)
{
	sc_finish(sc);
	delete(sc);
}

/*
 * Multiplicative hash with linear probe.  PHI is (golden ratio * 2**32).
 * The hash function accumulates a 32-bit key by adding each character in
 * the string key into a rotating accumulator.  The rotor constant RHO is
 * chosen based on the observed mean string length of ~5.4 and the 32 bit
 * hash_t limit.
 */
typedef unsigned long hash_t;

#define	PHI	((hash_t) 2654435769)
#define	RHO	6

unsigned int
sc_hash(Scope *sc, char *name, int namlen)
{
	hash_t hash;

	for (hash = 0; --namlen >= 0; name++)
		hash = (hash << RHO) + (hash >> BITS(hash_t) - RHO) + *name;
	return (PHI * hash) >> sc->sc_shift;
}

/*
 * Initialize a multiplicative hash table.  Allocate at least a 64 entry
 * table, to avoid collisions that are likely when taking only the first
 * few bits of the hash function's value for poorly distributed strings.
 * Waste 1/8 of the table for the free sentinel and a good alpha.
 */
#define	MINLOG2		6
#define	ALPHA(len)	((len) - (len) / 8)

void
sc_init(Scope *sc, int count, char *name)
{
	unsigned int log2;

	count += count / 2;	/* pro-rate for sentinel and good alpha */
	LOG2CEIL(count, log2);
	if (log2 < MINLOG2)
		log2 = MINLOG2;
	count = 1 << log2;
	sc->sc_length = count;
	sc->sc_numfree = ALPHA(count);
	sc->sc_shift = BITS(hash_t) - log2;
	sc->sc_table = vnew(count, Symbol);
	bzero(sc->sc_table, count * sizeof sc->sc_table[0]);
	sc->sc_name = name;
#ifdef METERING
	bzero(&sc->sc_meter, sizeof sc->sc_meter);
#endif
}

void
sc_finish(Scope *sc)
{
	delete(sc->sc_table);
}

/*
 * Return a pointer to the entry matching name if it exists, otherwise
 * to the empty entry in which name should be installed.
 */
static Symbol *
sc_search(Scope *sc, char *name, int *namlenp, int adding)
{
	int namlen;
	Symbol *sym;

	METER(sc->sc_meter.sm_searches++);
	namlen = *namlenp;
	if (namlen < 0)
		*namlenp = namlen = strlen(name);
	sym = &sc->sc_table[sc_hash(sc, name, namlen)];
	while (sym->sym_type != SYM_FREE
	    && (namlen != sym->sym_namlen
	        || bcmp(name, sym->sym_name, namlen))) {
		/*
		 * Because sc_numfree was initialized to 7/8 of the table
		 * size, we need not test for the table filling up.  We're
		 * guaranteed to hit the free sentinel before revisiting
		 * the primary hash.
		 */
		METER(sc->sc_meter.sm_probes++);
		if (adding)
			sym->sym_hashcol = 1;
		if (--sym < sc->sc_table)
			sym += sc->sc_length;
	}
	return sym;
}

Symbol *
sc_lookupsym(Scope *sc, char *name, int namlen)
{
	Symbol *sym;
	
	sym = sc_search(sc, name, &namlen, 0);
	if (sym->sym_type != SYM_FREE) {
		METER(sc->sc_meter.sm_hits++);
		return sym;
	}
	METER(sc->sc_meter.sm_misses++);
	return 0;
}

/*
 * NB: name must point to safe store.
 */
Symbol *
sc_addsym(Scope *sc, char *name, int namlen, enum symtype type)
{
	Symbol *sym;

	sym = sc_search(sc, name, &namlen, 1);
	if (sym->sym_type != SYM_FREE) {
		sym->sym_type = type;
		return sym;
	}
	if (sc->sc_numfree == 0) {
		int count, length;
		Symbol *oldtable;

		/*
		 * Hash table is full, so double its size and re-insert all
		 * active entries plus the new one for name.
		 */
		METER(sc->sc_meter.sm_grows++);
		count = sc->sc_length;
		length = 2 * count;
		sc->sc_length = length;
		sc->sc_numfree = count;
		--sc->sc_shift;
		oldtable = sc->sc_table;
		sc->sc_table = vnew(length, Symbol);
		bzero(sc->sc_table, length * sizeof sc->sc_table[0]);
		for (sym = oldtable; --count >= 0; sym++) {
			if (sym->sym_type == SYM_FREE)
				continue;
			length = sym->sym_namlen;
			*sc_search(sc, sym->sym_name, &length, 1) = *sym;
		}
		delete(oldtable);
		sym = sc_search(sc, name, &namlen, 1);
	}
	METER(sc->sc_meter.sm_adds++);
	assert(sc->sc_numfree > 0);
	--sc->sc_numfree;
	sym->sym_name = name;
	sym->sym_namlen = namlen;
	sym->sym_type = type;
	return sym;
}

/*
 * Perhaps shrink table when appropriate.
 */
void
sc_deletesym(Scope *sc, char *name, int namlen)
{
	Symbol *sym;

	sym = sc_search(sc, name, &namlen, 0);
	if (sym->sym_type == SYM_FREE)
		return;
	METER(sc->sc_meter.sm_deletes++);
	assert(sc->sc_numfree < ALPHA(sc->sc_length));
	sc->sc_numfree++;

	if (sc->sc_length > 1 << MINLOG2
	    && sc->sc_numfree == sc->sc_length - sc->sc_length / 4) {
		int count, length;
		Symbol *oldtable;

		/*
		 * If 3/4 of a non-minimal scope is free, shrink it.
		 */
		METER(sc->sc_meter.sm_shrinks++);
		count = sc->sc_length;
		length = count / 2;
		sc->sc_length = length;
		sc->sc_numfree -= length;
		sc->sc_shift++;
		oldtable = sc->sc_table;
		sc->sc_table = vnew(length, Symbol);
		bzero(sc->sc_table, length * sizeof sc->sc_table[0]);

		sym->sym_type = SYM_FREE;
		for (sym = oldtable; --count >= 0; sym++) {
			if (sym->sym_type == SYM_FREE)
				continue;
			length = sym->sym_namlen;
			*sc_search(sc, sym->sym_name, &length, 1) = *sym;
		}
		delete(oldtable);
	} else if (sym->sym_hashcol) {
		int slot, slot2, hash;
		Symbol *sym2;

		/*
		 * If not shrinking, reorganize any colliding entries.
		 */
		slot = slot2 = sym - sc->sc_table;
		for (;;) {
			sym->sym_type = SYM_FREE;
			sym->sym_hashcol = 0;
			do {
				if (--slot2 < 0)
					slot2 += sc->sc_length;
				sym2 = &sc->sc_table[slot2];
				if (sym2->sym_type == SYM_FREE)
					return;
				METER(sc->sc_meter.sm_dprobes++);
				hash = sc_hash(sc, sym2->sym_name,
					       sym2->sym_namlen);
			} while (slot2 <= hash && hash < slot
				 || slot < slot2 && (hash < slot
						     || slot2 <= hash));
			METER(sc->sc_meter.sm_dmoves++);
			*sym = *sym2;
			sym = sym2;
			slot = slot2;
		}
	} else
		sym->sym_type = SYM_FREE;
}
