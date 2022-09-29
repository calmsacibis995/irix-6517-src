/*
 * COPYRIGHT NOTICE
 * Copyright 1990, Silicon Graphics, Inc. All Rights Reserved.
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 *
 */

#ident	"$Revision: 1.11 $"

#ifdef __STDC__
	#pragma weak mac_cleared = _mac_cleared
	#pragma weak mac_clearedlbl = _mac_clearedlbl
	#pragma weak mac_clearance_error = _mac_clearance_error
	#pragma weak sgi_getclearancebyname = _sgi_getclearancebyname
	#pragma weak mac_cleared_pl = _mac_cleared_pl
	#pragma weak mac_cleared_ps = _mac_cleared_ps
	#pragma weak mac_cleared_fl = _mac_cleared_fl
	#pragma weak mac_cleared_fs = _mac_cleared_fs
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <clearance.h>
#include <ns_api.h>

#define	ABOVE		1
#define	BELOW		2
#define	FOUND		4

/*
 * mac_cleared - returns 0 iff the user is cleared for the label.
 *		(label is specified as a string)
 */

int
mac_cleared(const struct clearance *clp, const char *labelname)
{
	return (mac_cleared_ps (clp, labelname));
}


/*
 * mac_clearedlbl - returns 0 iff the user is cleared for the label.
 *		(label is specified as a mac_label *)
 */
int
mac_clearedlbl(const struct clearance *clp, mac_t desired_lp)
{
	return (mac_cleared_pl (clp, desired_lp));
}

/*
 * mac_cleared_label
 *
 * Internal function to check MAC clearance for setting labels given
 * a label.
 *
 * Return 0 iff user is cleared to set the specified label.
 */
static int
mac_cleared_label (const struct clearance *clp, mac_t desired_lp, int isproc)
{
	mac_t low_lp;		/* user's low label */
	mac_t high_lp;		/* user's high label */
	int too_low;		/* user's label lower than low bound */
	int too_high;		/* user's label higher than high bound */
	int flag;
	int msen_equal;		/* user's label has equal sensitivity */
	int mint_equal;		/* user's label has equal integrity */
	char *clearance, *label, *next, *min, *max;
	
	/*
	 * check arguments for validity
	 */
	if (clp == (struct clearance *) NULL)
		return (MAC_NULL_USER_INFO);
	if (clp->cl_allowed == (char *) NULL || strlen(clp->cl_allowed) == 0)
		return (MAC_NULL_CLEARANCE);
	if (desired_lp == (mac_t) NULL)
		return (MAC_NULL_REQD_LBL);
	if (mac_valid (desired_lp) <= 0)
		return (MAC_BAD_REQD_LBL);

	/* check for equal label components */
	msen_equal = (desired_lp->ml_msen_type == MSEN_EQUAL_LABEL);
	mint_equal = (desired_lp->ml_mint_type == MINT_EQUAL_LABEL);

	/* copy clearance information */
	if ((clearance = strdup(clp->cl_allowed)) == NULL)
		return (MAC_NO_MEM);
	label = next = clearance;

	/* skip leading whitespace */
	while (*next && isspace(*next)) 
		next++;

	/* if the clearance is empty, it's an error */
	if (*next == '\0') {
		free(clearance);
		return (MAC_NULL_CLEARANCE);
	}

	flag = 0;
	while (*next != '\0') {
		/* skip whitespace */
		while (isspace(*next))
			next++;
		label=next;

		/* skip non-whitespace */
		while (*next && !isspace(*next))
			next++;
		if (*next)
			*next++ = '\0';

		/*
		 * If only a single label then make the
		 * min & max equal to make a range.
		 */
		if ((max=strstr(label,"...")) == NULL) {
			min = label;
			max = min;
		}
		else {
			min = label;
			*max++ = '\0';
			while (*max == '.')
				max++;
		}

		/*
		 * The entry in the database needs to have valid labels. If
		 * both labels are valid then test the label range with
		 * mac_dominate().
		 */

		low_lp = mac_from_text (min);
		if (low_lp == (mac_t) NULL) {
			free(clearance);
			return(MAC_BAD_USER_INFO);
		}

		high_lp = mac_from_text (max);
		if (high_lp == (mac_t) NULL) {
			(void) mac_free(low_lp);
			free(clearance);
			return(MAC_BAD_USER_INFO);
		}
	
		/*
		 * If the three dots are there ...
		 * Simple range check.
		 */
		if (max != min && mac_dominate (high_lp, low_lp) <= 0) {
			(void) mac_free(low_lp);
			(void) mac_free(high_lp);
			free(clearance);
			return(MAC_BAD_RANGE);
		}

		/*
		 * If desired_lp has equal sensitivity or equal integrity,
		 * then the user must be cleared for the full range of
		 * sensitivity and/or integrity, from minimum to maximum.
		 * If not, then exit with error.
		 */
		if (msen_equal) {
			if (isproc ||
			    (!(low_lp->ml_msen_type == MSEN_LOW_LABEL &&
			       high_lp->ml_msen_type == MSEN_HIGH_LABEL))) {
				(void) mac_free(low_lp);
				(void) mac_free(high_lp);
				free(clearance);
				return(MAC_MSEN_EQUAL);
			}
			msen_equal = 0;
		}

		if (mint_equal) {
			if (!(low_lp->ml_mint_type == MINT_HIGH_LABEL &&
			      high_lp->ml_mint_type == MINT_LOW_LABEL)) {
				(void) mac_free(low_lp);
				(void) mac_free(high_lp);
				free(clearance);
				return(MAC_MINT_EQUAL);
			}
			mint_equal = 0;
		}

		too_low = mac_dominate (desired_lp, low_lp) <= 0;
		too_high = mac_dominate (high_lp, desired_lp) <= 0;

		(void) mac_free(low_lp);
		(void) mac_free(high_lp);

		if (too_high) 
			flag |= ABOVE;
		if (too_low)
			flag |= BELOW;
		else if (!too_high)
			flag |= FOUND;
	}

	free(clearance);

	if (flag & FOUND)
		return(MAC_CLEARED);

	if (flag == ABOVE)
		return(MAC_LBL_TOO_HIGH);

	if (flag == BELOW)
		return(MAC_LBL_TOO_LOW);

	return(MAC_INCOMPARABLE);
}

/*
 * mac_cleared_fl
 *
 * Check MAC clearance for setting labels on 'f'iles given a 'l'abel.
 *
 * Return 0 iff user is cleared to set the specified label on a file.
 */
int
mac_cleared_fl (const struct clearance *clp, mac_t desired_lp)
{
	return (mac_cleared_label (clp, desired_lp, 0));
}

/*
 * mac_cleared_pl
 *
 * Check MAC clearance for setting labels on 'p'rocesses given a 'l'abel.
 *
 * Return 0 iff user is cleared to set the specified label on a process.
 */
int
mac_cleared_pl (const struct clearance *clp, mac_t desired_lp)
{
	return (mac_cleared_label (clp, desired_lp, 1));
}

/*
 * mac_cleared_string
 *
 * Internal function to check MAC clearance for setting labels given
 * a 's'tring.
 *
 * Return 0 iff user is cleared to set the specified label.
 */
static int
mac_cleared_string (const struct clearance *clp, const char *labelname,
		    int isproc)
{
	mac_t desired_lp;
	int result;
	
	if (clp == (struct clearance *) NULL)
		return (MAC_NULL_USER_INFO);

	if (labelname == (char *) NULL)
		return (MAC_NULL_REQD_LBL);

	/* Convert the passed label into its binary form */
	desired_lp = mac_from_text (labelname);
	if (desired_lp == (mac_t) NULL)
		return (MAC_BAD_REQD_LBL);

	/* check clearance */
	result = mac_cleared_label (clp, desired_lp, isproc);
	(void) mac_free(desired_lp);
	return (result);
}

/*
 * mac_cleared_fs
 *
 * Check MAC clearance for setting labels on 'f'iles given a 's'tring.
 *
 * Return 0 iff user is cleared to set the specified label on a file.
 */
int
mac_cleared_fs (const struct clearance *clp, const char *labelname)
{
	return (mac_cleared_string (clp, labelname, 0));
}

/*
 * mac_cleared_ps
 *
 * Check MAC clearance for setting labels on 'p'rocesses given a 's'tring.
 *
 * Return 0 iff user is cleared to set the specified label on a process.
 */
int
mac_cleared_ps (const struct clearance *clp, const char *labelname)
{
	return (mac_cleared_string (clp, labelname, 1));
}

static const struct {
	short	 err;
	char	*str;
} errors[] = {
	MAC_CLEARED,          "User is cleared at the requested label",
	MAC_NULL_USER_INFO,   "clearance argument is null",
	MAC_NULL_REQD_LBL,    "label argument is null",
	MAC_BAD_REQD_LBL,     "requested label is not valid",
	MAC_MSEN_EQUAL,       "label cannot contain the MsenEqual label type",
	MAC_MINT_EQUAL,       "label cannot contain the MintEqual label type",
	MAC_BAD_USER_INFO,    "bad field in user.info file",
	MAC_BAD_RANGE,	      "bad clearance range in user.info file",
	MAC_NULL_CLEARANCE,   "no clearance field in user.info file",
	MAC_LBL_TOO_LOW,      "requested label is below the allowed range",
	MAC_LBL_TOO_HIGH,     "requested label is above the allowed range",
	MAC_INCOMPARABLE,     "requested label is outside the allowed range",
	MAC_NO_MEM,           "no memory",
	0,		      NULL,
};

/*
 * mac_clearance_error - translates one of the errors returned by
 *			mac_[cleared,lowest][lbl] into string form
 */
char *
mac_clearance_error(int err)
{
        int i;

	for (i = 0; errors[i].str != NULL; i++)
		if (errors[i].err == err)
			return (errors[i].str);
	return ("Undefined error status");
}

#define FIELD_SIZE	((size_t) 255)
static struct clearance user_cap;
/* move out of function scope so we get a global symbol for use with data cording */
static int allocated = 0;

static int
allocate_user_cap(void)
{

	if (!allocated)
	{
		user_cap.cl_name = (char *) malloc(FIELD_SIZE + 1);
		if (user_cap.cl_name == NULL)
			return(-1);

		user_cap.cl_default = (char *) malloc(FIELD_SIZE + 1);
		if (user_cap.cl_default == NULL)
		{
			free(user_cap.cl_name);
			return(-1);
		}

		user_cap.cl_allowed = (char *) malloc(FIELD_SIZE + 1);
		if (user_cap.cl_allowed == NULL)
		{
			free(user_cap.cl_name);
			free(user_cap.cl_default);
			return(-1);
		}

		allocated = 1;
	}
	return(0);
}

static struct clearance *
copy_on_match(char *buffer, size_t keylen)
{
	char *name;
	char *dflt;
	char *allowed;

	/* name starts at the beginning of the line */
	name = buffer;

	/* colon must immediately follow key */
	if (*(name + keylen) != ':')
		return(NULL);

	/* assign dflt and zero out colon */
	dflt = name + keylen;
	*dflt++ = '\0';

	/* assign allowed and zero out colon */
	allowed = strchr(dflt, ':');
	if (allowed == NULL || (allowed - dflt) > FIELD_SIZE)
		return(NULL);
	*allowed++ = '\0';
	if (strlen(allowed) > FIELD_SIZE)
		return(NULL);

	/* copy into user buffer */
	strcpy(user_cap.cl_name, name);
	strcpy(user_cap.cl_default, dflt);
	strcpy(user_cap.cl_allowed, allowed);

	/* return user buffer */
	return(&user_cap);
}

/* move out of function scope so we get a global symbol for use with data cording */
static ns_map_t _map_byname;

struct clearance *
sgi_getclearancebyname(const char *key)
{
	char buffer[1024];
	char *p;
	FILE *fp;
	size_t keylen;
	struct clearance *cap;
	struct stat st;

	/* reject bogus arg and allocate user buffer */
	if (key == NULL || allocate_user_cap() == -1)
		return(NULL);

	/* get key length */
	keylen = strlen(key);
	if (keylen == 0 || keylen > FIELD_SIZE)
		return(NULL);

	/* update cache time */
	if (stat(CLEARANCE, &st) == 0)
		_map_byname.m_version = st.st_mtim.tv_sec;

	/* look up key in database */
	switch(ns_lookup(&_map_byname, 0, "clearance.byname", key, 0,
			 buffer, sizeof(buffer)))
	{
		case NS_SUCCESS:
			/* strip trailing NUL */
			if ((p = strchr(buffer, '\n')) != NULL)
				*p = '\0';

			/* return match */
			return(copy_on_match(buffer, keylen));
		case NS_NOTFOUND:
			return(NULL);
	}

	/* fall back to local files */
	fp = fopen(CLEARANCE, "r");
	if (fp == NULL)
		return(NULL);
	while(fgets(buffer, (int) sizeof(buffer), fp) != NULL)
	{
		/* strip trailing NUL */
		if ((p = strchr(buffer, '\n')) != NULL)
			*p = '\0';

		/* skip leading whitespace */
		for (p = buffer; *p != '\0' && isspace(*p); p++)
			/* empty loop */;

		/* ignore empty or comment lines */
		if (*p == '\0' || *p == '#')
			continue;

		/* match key in line */
		if (strncmp(key, p, keylen) == 0)
		{
			if ((cap = copy_on_match(p, keylen)) == NULL)
				continue;
			fclose(fp);
			return(cap);
		}
	}
	fclose(fp);
	return(NULL);
}
