/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Enumerated type names, mapped from integer values using a vector if
 * the values form a continuous integer sequence, otherwise using an
 * instance of class index.
 */
#include <stdio.h>
#include "enum.h"
#include "index.h"
#include "macros.h"
#include "scope.h"
#include "strings.h"

static Enumerator boolvec[] = { EI_VAL("FALSE", 0), EI_VAL("TRUE", 1) };

Enumeration _bool = { 0, 1, 1, 0, boolvec };

void
en_initscope(Enumeration *en, Enumerator *vec, int len, Scope *sc)
{
	int dense, count, value;
	Enumerator *e;

	en->en_min = vec[0].e_val;
	en->en_max = vec[len-1].e_val;
	en->en_last = en->en_max - en->en_min;
	dense = 1;
	value = en->en_min - 1;
	for (count = len, e = vec; --count >= 0; e++) {
		if (e->e_val != value + 1)
			dense = 0;
		value = e->e_val;
		sc_addnumber(sc, e->e_name, e->e_namlen, value);
	}
	if (en->en_vec)
		return;
	if (dense)
		en->en_index = 0;
	else {
		en->en_index = index(len, sizeof e->e_val, 0);
		for (count = len, e = vec; --count >= 0; e++)
			in_enter(en->en_index, &e->e_val, e->e_name);
	}
	en->en_vec = vec;
}

void
en_finish(Enumeration *en)
{
	if (en->en_index)
		in_destroy(en->en_index);
	en->en_vec = 0;
}

#define	EN_LOOKUP(en, value)					\
	if (en->en_index) {					\
		char *name = in_match(en->en_index, &value);	\
		if (name)					\
			return name;				\
	} else {						\
		unsigned int i = value - en->en_min;		\
		if (i <= en->en_last)				\
			return en->en_vec[i].e_name;		\
	}

char *
en_lookup(Enumeration *en, int value)
{
	EN_LOOKUP(en, value);
	return 0;
}

char *
en_name(Enumeration *en, int value)
{
	static char buf[11+1];

	EN_LOOKUP(en, value);
	(void) sprintf(buf, "%d", value);
	return buf;
}

void
sc_addnumbers(Scope *sc, Enumerator *e, int len)
{
	for (; --len >= 0; e++)
		sc_addnumber(sc, e->e_name, e->e_namlen, e->e_val);
}

char *
en_bitset(Enumerator *e, int len, int set)
{
	int cc;
	static char buf[128];

	for (cc = 0; --len >= 0; e++) {
		if (!(e->e_val & set))
			continue;
		if (cc + 1 + e->e_namlen >= sizeof buf)
			continue;
		cc += nsprintf(&buf[cc], sizeof buf - cc, "%s%s",
			       (cc == 0) ? "" : " ", e->e_name);
	}
	if (cc == 0)
		return strcpy(buf, "0");
	return buf;
}
