/*
 * fru_pattern.c-
 *
 *	This file contains the information necessary to perform pattern
 * matching analysis.  The patterns are here, and the code is in
 * fru_matcher.c.  To add a pattern, one must define the pattern in
 * the fru_patterns array (using symbols from fru_pattern.h) and add a
 * case to the fru_cases array.
 *
 */

#ifdef FRU_PATTERN_MATCHER

#include "evfru.h"

#include "fru_pattern.h"

fru_entry_t fru_patterns[] = {
	{BEGINPATTERNS},
	{BEGINCASE, 1},
	{BEGINBOARD, EVTYPE_MC3},
	{BEGINSUBUNIT, SUBUNIT_LEAF},
	{FRU_POINTER},
	{ENDSUBUNIT, SUBUNIT_LEAF},
	{ENDBOARD, EVTYPE_MC3},
	{ENDCASE, 1},
	{ENDPATTERNS}
};

int num_fru_patterns = sizeof(fru_patterns) / sizeof(fru_entry_t);

/* Put case descriptions here. */

fru_case_t fru_cases[] = {
	{1, 0, {0, 0}, "Test pattern"}
};

int num_fru_cases = sizeof(fru_cases) / sizeof(fru_case_t);

/* Maybe these should go into separate files to make this easier to
 * generate and parse automatically.  Well, it doesn't matter now.
 */

#endif /* FRU_PATTERN_MATCHER */

