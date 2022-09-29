/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifdef __STDC__
	#pragma weak mac_from_text = _mac_from_text
	#pragma weak mac_to_text = _mac_to_text
	#pragma weak mac_to_text_long = _mac_to_text_long
	#pragma weak mac_free = _mac_free
	#pragma weak mac_get_fd = _mac_get_fd
	#pragma weak mac_get_file = _mac_get_file
	#pragma weak mac_get_proc = _mac_get_proc
	#pragma weak mac_set_fd = _mac_set_fd
	#pragma weak mac_set_file = _mac_set_file
	#pragma weak mac_set_proc = _mac_set_proc
	#pragma weak mac_valid = _mac_valid
	#pragma weak mac_dominate = _mac_dominate
	#pragma weak mac_equal = _mac_equal
	#pragma weak mac_size = _mac_size
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "mac_util.h"

mac_t
mac_from_text(const char *str)
{
	char *tmpstr;
	char *slashp;
	label_segment ls;
	mac_t lp;

	/* reject bogus arg */
	if (str == NULL)
		return (NULL);

	/*
	 * Treat str as an alias for a real label e.g.
	 *	dblow -> msenlow/minthigh
	 * If the alias exists, treat its label spec like
	 * any other label and break it up below.
	 */
	if ((tmpstr = __mac_spec_from_alias(str)) == NULL)
		if ((tmpstr = strdup(str)) == NULL)
			return(NULL);

	/* if no slash, not a valid label spec */
	if ((slashp = strchr(tmpstr, '/')) == NULL)
	{
		free(tmpstr);
		return(NULL);
	}
	*slashp++ = '\0';

	/* create mac label */
	if ((lp = (mac_t) malloc(sizeof(struct mac_label))) == NULL)
	{
		free(tmpstr);
		return(NULL);
	}
	memset((void *) lp, '\0', sizeof(struct mac_label));

	/* look up MSEN type field */
	ls.type = 0;
	ls.level = 0;
	ls.count = 0;
	ls.list = lp->ml_list;
	if (__msen_from_text(&ls, tmpstr) == -1)
	{
		free(tmpstr);
		free(lp);
		return(NULL);
	}
	lp->ml_msen_type = ls.type;
	lp->ml_level = ls.level;
	lp->ml_catcount = ls.count;

	/* look up MINT type field */
	ls.type = 0;
	ls.level = 0;
	ls.count = 0;
	ls.list = lp->ml_list + lp->ml_catcount;
	if (__mint_from_text(&ls, slashp) == -1)
	{
		free(tmpstr);
		free(lp);
		return(NULL);
	}
	lp->ml_mint_type = ls.type;
	lp->ml_grade = ls.level;
	lp->ml_divcount = ls.count;

	/* Success! */
	free(tmpstr);
	return(lp);
}

/*
 *      returns either a label name string or a label component string
 *      which represents the mac label.
 */
static char *
mac_to_text_internal(mac_t lp, size_t *len_p, int long_only)
{
	char *buf;			/* buffer to hold data */
	label_segment ls;

	/* check validity of passed in label */
	if (mac_valid(lp) <= 0)
		return(NULL);

	/* allocate buffer */
	buf = (char *) malloc((size_t) 8192);
	if (buf == NULL)
		return(NULL);

	/* get msen portion */
	ls.type = lp->ml_msen_type;
	ls.level = lp->ml_level;
	ls.count = lp->ml_catcount;
	ls.list = lp->ml_list;
	if (__msen_to_text(&ls, buf) == -1)
	{
		free(buf);
		return(NULL);
	}

	/* append trailing '/' */
	strcat(buf, "/");

	/* get mint portion */
	ls.type = lp->ml_mint_type;
	ls.level = lp->ml_grade;
	ls.count = lp->ml_divcount;
	ls.list = lp->ml_list + lp->ml_catcount;
	if (__mint_to_text(&ls, buf + strlen(buf)) == -1)
	{
		free(buf);
		return(NULL);
	}

	/* reallocate buffer to smallest size */
	buf = realloc(buf, strlen(buf) + 1);
	if (buf == NULL)
		return(NULL);
	
	if (!long_only)
	{
		char *tmp = __mac_alias_from_spec(buf);
		if (tmp != NULL)
		{
			buf = realloc(buf, strlen(tmp) + 1);
			if (buf == NULL)
			{
				free(tmp);
				return(NULL);
			}
			strcpy(buf, tmp);
			free(tmp);
		}
	}

	/* Success! */
	if (len_p != NULL)
		*len_p = strlen(buf);
	return(buf);
}

/*
 *      returns either a label name string or a label component string
 *      which represents the mac label.
 */
char *
mac_to_text(mac_t lp, size_t *len_p)
{
	return(mac_to_text_internal(lp, len_p, 0));
}

/*
 * always returns a label component string
 * which represents the mac label.
 */
char *
mac_to_text_long(mac_t lp, size_t *len_p)
{
	return(mac_to_text_internal(lp, len_p, 1));
}

int
mac_free(void *obj_p)
{
        if (obj_p)
                free(obj_p);
        return 0;
}

/*
 * Get the label of a file designated by a file descriptor.
 */
mac_t
mac_get_fd(int fd)
{
	mac_t mac;

        if (!(mac = malloc(sizeof(struct mac_label)))) {
                setoserror(ENOMEM);
                return (mac_t)0;
        }

        if (syssgi(SGI_MAC_GET, 0, fd, mac) == -1) {
                free((void *)mac);
                return (mac_t)0;
        }
        else
                return mac;
}

/*
 * Get the mac label of a file.
 */
mac_t
mac_get_file(const char *path)
{
        mac_t mac;

        if (!(mac = malloc(sizeof(struct mac_label)))) {
                setoserror(ENOMEM);
                return (mac_t)0;
        }

        if (syssgi(SGI_MAC_GET, path, -1, mac) == -1) {
                free((void *)mac);
                return (mac_t)0;
        }
        else
                return mac;
}

/*
 * Obtain the current process mac label.
 */
mac_t
mac_get_proc(void)
{
        mac_t mac;

        if (!(mac = malloc(sizeof(struct mac_label)))) {
                setoserror(ENOMEM);
                return (mac_t)0;
        }

	if (syssgi(SGI_PROC_ATTR_GET, SGI_MAC_PROCESS, mac) == -1) {
                free((void *)mac);
                return (mac_t)0;
        }
        else
                return mac;
}

/*
 * Set the mac label of an open file.
 */
int
mac_set_fd(int fd, mac_t mac)
{

        if (!mac) {
                setoserror(EINVAL);
                return (-1);
        }

        if (syssgi(SGI_MAC_SET, 0, fd, mac) == -1)
                return (-1);
        else
                return 0;
}

/*
 * Set the mac label of a file.
 */
int
mac_set_file(const char *path, mac_t mac)
{

        if (!mac) {
                setoserror(EINVAL);
                return (-1);
        }

        if (syssgi(SGI_MAC_SET, path, -1, mac) == -1)
                return (-1);
        else
                return 0;
}

/*
 * Set the process mac label.
 */
int
mac_set_proc(mac_t mac)
{

        if (!mac) {
                setoserror(EINVAL);
                return (-1);
        }

	if (syssgi(SGI_PROC_ATTR_SET, SGI_MAC_PROCESS, mac, sizeof(*mac)) == -1)
                return (-1);
        else
                return 0;
}

/*
 * mac_valid(lp)
 * check the validity of a mac label
 */
int
mac_valid(mac_t lp)
{
	if (lp == NULL)
		return (0);

	/*
	 * if the total category set and division set is greater than 250
	 * report error
	 */
	if ((lp->ml_catcount + lp->ml_divcount) > MAC_MAX_SETS)
		return(0);

	/*
	 * check whether the msentype value is valid, and do they have
  	 * appropriate level, category association.
         */
	switch (lp->ml_msen_type) {
		case MSEN_ADMIN_LABEL:
		case MSEN_EQUAL_LABEL:
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			if (lp->ml_level != 0 || lp->ml_catcount > 0 )
				return (0);
			break;
		case MSEN_TCSEC_LABEL:
		case MSEN_MLD_LABEL:
			if (lp->ml_catcount > 0 &&
			    __check_setvalue(lp->ml_list,
					     lp->ml_catcount) == -1)
				return (0);
			break;
		case MSEN_UNKNOWN_LABEL:
		default:
			return (0);
	}

	/*
	 * check whether the minttype value is valid, and do they have
	 * appropriate grade, division association.
	 */
	switch (lp->ml_mint_type) {
		case MINT_BIBA_LABEL:
			if (lp->ml_divcount > 0 &&
			    __check_setvalue(lp->ml_list + lp->ml_catcount,
					     lp->ml_divcount) == -1)
				return(0);
			break;
		case MINT_EQUAL_LABEL:
		case MINT_HIGH_LABEL:
		case MINT_LOW_LABEL:
			if (lp->ml_grade != 0 || lp->ml_divcount > 0 )
				return(0);
			break;
		default:
			return(0);
	}

	return (1);
}

/*
 * mac_dominate
 * 	Returns 1 iff the first label dominates the second label
 *	Returns 0 otherwise.
 *	Return -1 if errors.
 */
int
mac_dominate(mac_t lp1, mac_t lp2)
{
	label_segment ls1, ls2;

	if (mac_valid (lp1) <= 0 || mac_valid (lp2) <= 0) {
		setoserror(EINVAL);
		return (-1);
	}

	if (lp1 == lp2)
		return (1);

	/*
	 * If either label is MSEN_EQUAL don't check sensitivity further.
	 */
	if (lp1->ml_msen_type != MSEN_EQUAL_LABEL &&
	    lp2->ml_msen_type != MSEN_EQUAL_LABEL) {

		switch (lp1->ml_msen_type) {
		case MSEN_ADMIN_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_ADMIN_LABEL:
			case MSEN_LOW_LABEL:
			case MSEN_MLD_LOW_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
			/*
			 * High always dominates.
			 */
			break;
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_LOW_LABEL:
			case MSEN_MLD_LOW_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_MLD_LABEL:
		case MSEN_TCSEC_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_LOW_LABEL:
			case MSEN_MLD_LOW_LABEL:
				break;
			case MSEN_TCSEC_LABEL:
			case MSEN_MLD_LABEL:
				ls1.type = lp1->ml_msen_type;
				ls1.level = lp1->ml_level;
				ls1.count = lp1->ml_catcount;
				ls1.list = lp1->ml_list;

				ls2.type = lp2->ml_msen_type;
				ls2.level = lp2->ml_level;
				ls2.count = lp2->ml_catcount;
				ls2.list = lp2->ml_list;

				if (!__tcsec_dominate(&ls1, &ls2))
					return (0);
				break;
			default:
				return (0);
			}
			break;
		default:
			return (0);
		}
	}
	/*
	 * If either label is MINT_EQUAL we're done.
	 */
	if (lp1->ml_mint_type == MINT_EQUAL_LABEL ||
	    lp2->ml_mint_type == MINT_EQUAL_LABEL)
		return (1);

	switch (lp1->ml_mint_type) {
		case MINT_LOW_LABEL:
			return (1);
		case MINT_BIBA_LABEL:
			switch (lp2->ml_mint_type) {
			case MINT_BIBA_LABEL:
				ls1.type = lp1->ml_mint_type;
				ls1.level = lp1->ml_grade;
				ls1.count = lp1->ml_divcount;
				ls1.list = lp1->ml_list + lp1->ml_catcount;

				ls2.type = lp2->ml_mint_type;
				ls2.level = lp2->ml_grade;
				ls2.count = lp2->ml_divcount;
				ls2.list = lp2->ml_list + lp2->ml_catcount;

				return(__biba_dominate(&ls1, &ls2));
			case MINT_LOW_LABEL:
				return (0);
			default:
				break;
			}
		case MINT_HIGH_LABEL:
			return (lp2->ml_mint_type == MINT_HIGH_LABEL);
	}
	return (0);
}

/*
 * mac_equal
 * 	Returns 1 iff the labels are equal
 *	Returns 0 otherwise.
 *	Returns -1 if errors.
 */
int
mac_equal(mac_t lp1, mac_t lp2)
{
	label_segment ls1, ls2;

	if (mac_valid (lp1) <= 0 || mac_valid (lp2) <= 0) {
		setoserror(EINVAL);
		return (-1);
	}

	if (lp1 == lp2)
		return (1);

	/*
	 * If either label is MSEN_EQUAL don't check sensitivity further.
	 */
	if (lp1->ml_msen_type != MSEN_EQUAL_LABEL &&
	    lp2->ml_msen_type != MSEN_EQUAL_LABEL) {

		switch (lp1->ml_msen_type) {
		case MSEN_ADMIN_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_ADMIN_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_HIGH_LABEL:
			case MSEN_MLD_HIGH_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LOW_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_LOW_LABEL:
			case MSEN_MLD_LOW_LABEL:
				break;
			default:
				return (0);
			}
			break;
		case MSEN_MLD_LABEL:
		case MSEN_TCSEC_LABEL:
			switch (lp2->ml_msen_type) {
			case MSEN_TCSEC_LABEL:
			case MSEN_MLD_LABEL:
				ls1.type = lp1->ml_msen_type;
				ls1.level = lp1->ml_level;
				ls1.count = lp1->ml_catcount;
				ls1.list = lp1->ml_list;

				ls2.type = lp2->ml_msen_type;
				ls2.level = lp2->ml_level;
				ls2.count = lp2->ml_catcount;
				ls2.list = lp2->ml_list;

				if (!__segment_equal(&ls1, &ls2))
					return (0);
				break;
			default:
				return (0);
			}
			break;
		default:
			return (0);
		}
	}
	/*
	 * If either label is MINT_EQUAL we're done.
	 */
	if (lp1->ml_mint_type == MINT_EQUAL_LABEL ||
	    lp2->ml_mint_type == MINT_EQUAL_LABEL)
		return (1);

	if (lp1->ml_mint_type != lp2->ml_mint_type)
		return (0);

	switch (lp1->ml_mint_type) {
		case MINT_HIGH_LABEL:
		case MINT_LOW_LABEL:
			return (1);
		case MINT_BIBA_LABEL:
			ls1.type = lp1->ml_mint_type;
			ls1.level = lp1->ml_grade;
			ls1.count = lp1->ml_divcount;
			ls1.list = lp1->ml_list + lp1->ml_catcount;

			ls2.type = lp2->ml_mint_type;
			ls2.level = lp2->ml_grade;
			ls2.count = lp2->ml_divcount;
			ls2.list = lp2->ml_list + lp2->ml_catcount;

			return(__segment_equal(&ls1, &ls2));
		default:
			break;
	}
	return (0);
}

ssize_t
mac_size(mac_t lp)
{
	if (mac_valid(lp) <= 0)
	{
                setoserror(EINVAL);
		return((ssize_t) -1);
	}

	return ((ssize_t) sizeof (struct mac_label));
}
