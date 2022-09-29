/*
 * fru_matcher.c-
 *
 *	This file contains code to compare the hardware error state
 * structure with various patterns to allow us to diagnose difficult
 * cases directly.  This works in conjuction with pattern matching code
 * in each of the fru_*.c files for the various kinds of boards.
 *
 */

#ifdef FRU_PATTERN_MATCHER

#include "evfru.h"
#include "fru_pattern.h"

fru_entry_t *token;
fru_case_t *case_ptr;

fru_case_t *find_case(int);

int
fru_check_patterns(everror_t *ee, evcfginfo_t *ec, whatswrong_t *diag)
{
    int case_id;
    int board_id;
    int match;

    token = fru_patterns;

    while ((token = next_token(token)) &&
	   (token->entry_type != ENDPATTERNS)) {

	if (token->entry_type != BEGINCASE) {
	    FRU_PRINTF("Bad token!  Should start with BEGINCASE\n");
	    return -1;
	}

	/* BEGINCASE tokens include the case id in the "value" field. */
	case_id = token->value;

	case_ptr = find_case(case_id);

	while (token = next_token(token)) {
   	 
	    if (token->entry_type == ENDCASE) {
		break;
	    }

	    if (token->entry_type != BEGINBOARD) {
		FRU_PRINTF("Bad token: %s!  Board should start with BEGINBOARD\n",
			decode_control_token(token));
		return -1;
	    }

/* XXX - Need to add code to make sure that the boards exist before declaring
 * a match. */

	    switch(token->value) {
		case EVTYPE_IP19:
#ifdef DEBUG
		    if (fru_debug)
			FRU_PRINTF("EVTYPE_IP19\n");
#endif
		    match = match_ip19board(&token, ee, ec, diag, case_ptr);
		    break;

		case EVTYPE_IP21:
#ifdef DEBUG
		    if (fru_debug)
			FRU_PRINTF("EVTYPE_IP21\n");
#endif
		    match = match_ip21board(&token, ee, ec, diag, case_ptr);
		    break;

		case EVTYPE_IO4:
#ifdef DEBUG
		    if (fru_debug)
			FRU_PRINTF("EVTYPE_IO4\n");
#endif
		    match = match_io4board(&token, ee, ec, diag, case_ptr);
		    break;

		case EVTYPE_MC3:
#ifdef DEBUG
		    if (fru_debug)
			FRU_PRINTF("EVTYPE_MC3\n");
#endif
		    match = match_mc3board(&token, ee, ec, diag, case_ptr);
		    break;

		default:
		    FRU_PRINTF("Bad board type (0x%x)\n", token->value);
		    break;	
	    }
	    if (!match) {
#ifdef DEBUG
		    if (fru_debug)
			FRU_PRINTF("No match!  (Case %d)\n", CASE_ID(case_ptr));
#endif

		    /* We didn't match, skip the rest of this case. */
		    token = find_case_end(token);
		    continue;
	    }

	}

	if (match == -1) {
	    FRU_PRINTF("Error in case %d description, aborting.\n", case_id);
	    return 0;
	}

	if (token->entry_type == ENDCASE) {
		/* We _did_ match.  Return the information */
#ifdef DEBUG
		if (fru_debug)
		    FRU_PRINTF("Case %d match!\n");
#endif
		return CONFIDENCE(case_ptr);
	}
    }
}


fru_case_t *find_case(int case_id)
{
    int i;

    for (i = 0; i < num_fru_cases; i++) { 
	if (fru_cases[i].case_id == case_id)
	    return &fru_cases[i];
    }

    return (fru_case_t *)0;
}

void update_pattern(whatswrong_t *ww, fru_case_t *cp, short utype, short unum)
{
    ww->confidence = CONFIDENCE(cp);
    ww->case_id = CASE_ID(cp);
    ww->src.unit_type = utype;
    ww->src.unit_num = unum;
    ww->dest.unit_type = FRU_NONE;
} 


fru_entry_t *next_token(fru_entry_t *cur_token)
{
    cur_token++;

    if ((__psint_t)cur_token < (__psint_t)&fru_patterns[0]) {
	FRU_PRINTF("Token pointer (0x%x) below minimum\n", cur_token);
	return (fru_entry_t *)0;
    }

    if ((__psint_t)cur_token > (__psint_t)&fru_patterns[num_fru_patterns]) {
	FRU_PRINTF("Token pointer (0x%x) above maximum\n", cur_token);
	return (fru_entry_t *)0;
    }

    return cur_token;
}


fru_entry_t *find_case_end(fru_entry_t *cur_token)
{
    while (cur_token && (cur_token->entry_type != ENDCASE))
	cur_token = next_token(cur_token);

    return cur_token;
}

fru_entry_t *find_board_end(fru_entry_t *cur_token)
{
    while (cur_token && (cur_token->entry_type != ENDBOARD))
	cur_token = next_token(cur_token);

    return cur_token;
}


fru_entry_t *prev_token(fru_entry_t *cur_token)
{
    cur_token--;

    if ((__psint_t)cur_token < (__psint_t)&fru_patterns[0]) {
	FRU_PRINTF("Token pointer (0x%x) below minimum\n", cur_token);
	return (fru_entry_t *)0;
    }

    if ((__psint_t)cur_token > (__psint_t)(&fru_patterns[num_fru_patterns])) {
	FRU_PRINTF("Token pointer (0x%x) above maximum\n", cur_token);
	return (fru_entry_t *)0;
    }

    return cur_token;
}


int is_control_token(fru_entry_t *token)
{
	if ((token->entry_type == BEGINCASE) ||
	    (token->entry_type == BEGINBOARD) ||
	    (token->entry_type == BEGINSUBUNIT) ||
	    (token->entry_type == ENDSUBUNIT) ||
	    (token->entry_type == ENDBOARD) ||
	    (token->entry_type == ENDCASE) ||
	    (token->entry_type == ENDPATTERNS))
		return 1;
	else
		return 0;

}

int is_fru_token(fru_entry_t *token)
{
	if (token->entry_type == FRU_POINTER)
		return 1;
	else
		return 0;
}


char *decode_control_token(fru_entry_t *token)
{
	switch(token->entry_type) {
		case BEGINPATTERNS:
			return "BEGINPATTERNS";
		case BEGINCASE:
			return "BEGINCASE";
		case BEGINBOARD:
			return "BEGINBOARD";
		case BEGINSUBUNIT:
			return "BEGINSUBUNIT";
		case ENDSUBUNIT:
			return "ENDSUBUNIT";
		case ENDBOARD:
			return "ENDBOARD";
		case ENDCASE:
			return "ENDCASE";
		case ENDPATTERNS:
			return "ENDPATTERNS";
		case FRU_POINTER:
			return "FRU_POINTER";
		default:
			return "Not a control token";
	}
}

#endif /* FRU_PATTERN_MATCHER */

