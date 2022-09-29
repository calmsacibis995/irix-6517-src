/*
 * fru_main.c-
 *
 *	Here we have the main entrance to the FRU analyzer.  This file
 * contains code to sequence through the boards present in a system,
 * analyze the error state or each, and print the results.
 *
 */

#include "evfru.h"			/* FRU analyzer definitions */

static whatswrong_t judgment[EV_MAX_SLOTS];
static whatswrong_t matcher[EV_MAX_SLOTS];

extern void display_whatswrong(whatswrong_t *, evcfginfo_t *);

/*
 * Normally, when we think the error's hardware, we hide the suspect
 * hardware.  Since it could be useful to know it, we can set the
 * fru_ignore_sw flag to re-expose it.
 */
int fru_ignore_sw = 0;

#ifdef DEBUG
int fru_debug = 0;
#endif

/* This is the main entrypoint into the FRU analyzer. */
/* ARGSUSED */
int fru_analyzer(everror_t *errbuf, everror_ext_t *errbuf1, 
	       evcfginfo_t *ecbuf, int flags)
		/* flags is here for future expansion */
{
    int slot;
    int highest_confidence = 0;
    int pattern_confidence = 0;
    int error_counts[NUM_CONFIDENCE_LEVELS];
    int found_nothing = 1;	/* Did we find an error? */
    int forced_error = 0;

    for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
	/* Get our board */
	evbrdinfo_t *eb = &(ecbuf->ecfg_board[slot]);

	/* Set up our entry. */
	judgment[slot].slot_num = matcher[slot].slot_num = slot;
	judgment[slot].board_type = matcher[slot].board_type = eb->eb_type;

#ifdef DEBUG
	if (fru_debug)
	    FRU_PRINTF("slot %d\n", slot);
#endif

	/* Do a depth-first search within each Ebus board. */
	switch(eb->eb_type) {
#if !defined(_KERNEL) || defined(IP19)
	    case EVTYPE_IP19:
		check_ip19(eb, errbuf, errbuf1, &judgment[slot], ecbuf);
		break;
#endif /* !defined(_KERNEL) || defined(IP19) */

#if !defined(_KERNEL) || defined(IP21)
	    case EVTYPE_IP21:
		check_ip21(eb, errbuf, errbuf1, &judgment[slot], ecbuf);
		break;
#endif /* !defined(_KERNEL) || defined(IP21) */

	    case EVTYPE_MC3:
		check_mc3(eb, errbuf, errbuf1, &judgment[slot], ecbuf);
		break;

	    case EVTYPE_IO4:
		check_io4(eb, errbuf, errbuf1, &judgment[slot], ecbuf);
		break;
	}

    }

#ifdef FRU_PATTERN_MATCHER
    pattern_confidence = fru_check_patterns(errbuf, ecbuf, matcher);
#else
    pattern_confidence = 0;
#endif /* FRU_PATTERN_MATCHER */

    for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
	/* Keep track of highest confidence level */
	if (judgment[slot].confidence > highest_confidence)
	    highest_confidence = judgment[slot].confidence;
    }

    if (pattern_confidence > highest_confidence) {
	for (slot = 1; slot < EV_MAX_SLOTS; slot++)
		judgment[slot] = matcher[slot];
	highest_confidence = pattern_confidence;
    }

    /* Clear out our count array. */
    for (slot = 0; slot < NUM_CONFIDENCE_LEVELS; slot++)
	error_counts[slot] = 0;

    /* And set up the counts. */
    for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
	/* Increment confidence count */
	error_counts[judgment[slot].confidence]++;
    }

    FRU_PRINTF("\n");
    /*
     * Do the following only if the extended everest structure is setup.
     * If no errors are found, report this as a software error
     */
    if (errbuf1 && (highest_confidence == 0)) {
	  fru_element_t src;
	    
	  src.unit_type = FRU_SOFTWARE;
	  src.unit_num = NO_UNIT;
	  update_confidence(&judgment[1], DEFINITE, &src, NO_ELEM);
	  highest_confidence = judgment[1].confidence;
	  forced_error = 1;
    }

    if (highest_confidence == 0) {
	    FRU_PRINTF(FRU_INDENT "FRU ANALYZER (%s):  No errors found.\n",
			VERSION);
    } else {
	  if (!forced_error) 
		found_nothing = 0;
#ifdef OLDMESSAGE
	    FRU_PRINTF(FRU_INDENT
		       "FRU ANALYZER (%s): %she following has failed:\n",
		    VERSION,
		    error_counts[highest_confidence] > 1 ? "One of t" : "T");
#else
	    FRU_PRINTF(FRU_INDENT "FRU ANALYZER (%s):\n", VERSION);
#endif
	    for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
		    if (judgment[slot].confidence == highest_confidence) {
		        display_whatswrong(&(judgment[slot]), ecbuf);
		    }
	    }
	    FRU_PRINTF(FRU_INDENT "++ END OF ANALYSIS\n");
    }

    return found_nothing; /* 0 == success */
}

void clear_element(fru_element_t *element)
{
	element->unit_type = FRU_NONE;
	element->unit_num = 0;
}


int update_confidence(whatswrong_t *ww, int level,
		      fru_element_t *src, fru_element_t *dest)
{
	if (ww->confidence <= level) {
		ww->confidence = level;
		if (src)
			ww->src = *src;
		else
			clear_element(&(ww->src));
		if (dest)
			ww->dest = *dest;
		else
			clear_element(&(ww->dest));
		return 1;
	} else {
		return 0;
	}
}


int conditional_update(whatswrong_t *ww, int level, int data, int mask,
		       fru_element_t *src, fru_element_t *dest)
{
	if (data & mask)
		return update_confidence(ww, level, src, dest);
	else
		return 0;
}
