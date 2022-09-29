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

#ident "$Revision: 1.9 $"
/* 
 *
 * chlabel - change file label
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <clearance.h>

static cap_value_t lstat_caps[] =
{
	CAP_DAC_EXECUTE,
	CAP_NOT_A_CID,		/* really: CAP_DAC_READ_SEARCH */
	CAP_NOT_A_CID,		/* really: CAP_MAC_READ */
};

static cap_value_t caps_for_mac_set_file[] =
{
	CAP_NOT_A_CID,		/* really: CAP_DAC_READ_SEARCH */
	CAP_NOT_A_CID,		/* really: CAP_MAC_READ */
	CAP_DAC_EXECUTE,
	CAP_FOWNER,
	CAP_MAC_DOWNGRADE,
	CAP_MAC_UPGRADE,
	CAP_MAC_WRITE,
	CAP_DEVICE_MGT,
	CAP_DAC_WRITE,
	CAP_MAC_RELABEL_OPEN,
};

static cap_value_t caps_for_mac_set_proc[] =
{
	CAP_MAC_MLD,
	CAP_MAC_RELABEL_SUBJ,
};

static void
print_usage_and_die (char *program)
{
	fprintf (stderr, "%s: usage:\n", program);
	fprintf (stderr, "\t%s [-f] -m pathname\n", program);
	fprintf (stderr, "\t%s [-f] label pathname ...\n", program);
	exit (EXIT_FAILURE);
}

static int
cap_inheritable (cap_t pcap, cap_value_t cap)
{
	cap_flag_value_t flag;

	return (cap_get_flag (pcap, cap, CAP_INHERITABLE, &flag) == 0 &&
		flag == CAP_SET);
}

static int
cap_permitted (cap_t pcap, cap_value_t cap)
{
	cap_flag_value_t flag;

	return (cap_get_flag (pcap, cap, CAP_PERMITTED, &flag) == 0 &&
		flag == CAP_SET);
}

static mac_t pmac;		/* current process label */
static mac_t pmold;		/* current process label, moldy */

static void
free_pmac (void)
{
	if (pmac);
		mac_free (pmac);
	if (pmold)
		mac_free (pmold);
}

int
main (int argc, char *argv[])
{
	mac_t file_mac;		/* existing file label */
	mac_t new_mac;		/* new file label */
	cap_t ocap;		/* saved process capabilities */
	char *program;		/* Program name saved */
	int c;			/* For use by getopt(3) */
	int force = 0;		/* relabel even if open */
	int mflag = 0;		/* set moldy bit */
	int lstat_ncap = 1;
	int msf_ncap = 2;

	/* find program name */
	program = strrchr (argv[0], '/');
	program = (program != NULL ? program + 1 : argv[0]);

	/* only work if MAC is enabled */
	if (sysconf (_SC_MAC) <= 0)
	{
		fprintf (stderr, "%s: MAC not enabled.\n", program);
		return (EXIT_FAILURE);
	}

	/*
	 * get current process label and
	 * create moldy version of current process label and
	 * register label destroy function
	 */
	if ((pmac = mac_get_proc ()) == NULL ||
	    (pmold = mac_set_moldy (pmac)) == NULL ||
	    atexit (free_pmac))
	{
		fprintf (stderr, "%s: fatal error at %s(%d).\n",
			 program, __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}

	/* only work if CAP is enabled */
	if (sysconf (_SC_CAP) <= 0)
	{
		fprintf (stderr, "%s: capabilities not enabled.\n", program);
		return (EXIT_FAILURE);
	}

	/* 
	 * Verify that the required capabilities are available.
	 *      CAP_DAC_READ_SEARCH is required by sgi_getclearancebyname
	 *      CAP_MAC_READ is required by sgi_getclearancebyname
	 *      CAP_PRIV_PORT is required by sgi_getclearancebyname
	 *
	 *	CAP_MAC_MLD is required by chlabel itself
	 *	CAP_MAC_RELABEL_SUBJ is required by chlabel itself
	 *
	 * If CAP_DAC_READ_SEARCH or CAP_MAC_READ were inherited, then
	 * it's ok to acquire them. If not, then the
	 * invoking user isn't cleared for these capabilities,
	 * so we must not acquire them.
	 */
	if ((ocap = cap_get_proc ()) == NULL)
	{
		fprintf (stderr, "%s: fatal error at %s(%d).\n",
			 program, __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}
	if (!cap_permitted (ocap, CAP_DAC_READ_SEARCH) ||
	    !cap_permitted (ocap, CAP_MAC_READ) ||
	    !cap_permitted (ocap, CAP_PRIV_PORT) ||
	    !cap_permitted (ocap, CAP_MAC_MLD) ||
	    !cap_permitted (ocap, CAP_MAC_RELABEL_SUBJ) ||
	    !cap_permitted (ocap, CAP_DAC_WRITE))
	{
		cap_free (ocap);
		fprintf (stderr, "%s: insufficient privilege\n",
			 program);
		return (EXIT_FAILURE);
	}
	if (cap_inheritable (ocap, CAP_DAC_READ_SEARCH))
	{
		lstat_caps[lstat_ncap++] = CAP_DAC_READ_SEARCH;
		caps_for_mac_set_file[--msf_ncap] = CAP_DAC_READ_SEARCH;
	}
	if (cap_inheritable (ocap, CAP_MAC_READ))
	{
		lstat_caps[lstat_ncap++] = CAP_MAC_READ;
		caps_for_mac_set_file[--msf_ncap] = CAP_MAC_READ;
	}
	cap_free (ocap);

	/* parse option arguments */
	while ((c = getopt (argc, argv, "fm")) != -1)
	{
		switch (c)
		{
			case 'f':
				force = 1;
				break;
			case 'm':
				mflag = 1;
				break;
			default:
				print_usage_and_die (program);
				break;
		}
	}

	/* 
	 * Unless the -m flag was set the first parameter is the label.
	 */
	if (!mflag)
	{
		struct clearance *clp;	/* user clearance information */
		struct passwd *pw;	/* user name information */

		if (optind >= argc)
			print_usage_and_die (program);

		/* convert human-readable label to internal form */
		if ((new_mac = mac_from_text (argv[optind])) == NULL)
		{
			fprintf (stderr, "%s: invalid label: \"%s\"\n",
				 program, argv[optind]);
			return (EXIT_FAILURE);
		}

		/* get /etc/passwd info */
		if ((pw = getpwuid (getuid ())) == NULL)
		{
			fprintf (stderr, "%s cannot find user's name.\n",
				 program);
			return (EXIT_FAILURE);
		}

		/* get the user's label clearance range */
		if ((clp = sgi_getclearancebyname (pw->pw_name)) == NULL)
		{
			fprintf (stderr, "%s cannot find user %s's clearance.\n",
				 program, pw->pw_name);
			return (EXIT_FAILURE);
		}

		/* check the label against the user's clearance range */
		if ((c = mac_cleared_fl (clp, new_mac)) != MAC_CLEARED)
		{
			mac_free (new_mac);
			fprintf (stderr, "%s: denied because %s.\n",
				 program, mac_clearance_error (c));
			return (EXIT_FAILURE);
		}

		optind++;
	}

	/* 
	 * There needs to be at least one path.
	 * If -m is given, there can be only one path.
	 */
	if (optind >= argc || (mflag && (optind + 1) != argc))
		print_usage_and_die (program);

	/* 
	 * Do everything with a moldy label. The case this has trouble
	 * with is:
	 *      chlabel A/B
	 * where A is a moldy directory. The argument is that one
	 * oughtn't be changing the label on a file in a moldy directory
	 * without first moving it to where it belongs.
	 */

	for (; optind < argc; optind++)
	{
		struct stat plain_st;	/* attribute of file, normal label */
		struct stat moldy_st;	/* attribute of file, moldy label */

		c = 0;

		/* give ourselves a moldy label */
		ocap = cap_acquire (2, caps_for_mac_set_proc);
		if (mac_set_proc (pmold) == -1)
		{
			cap_surrender (ocap);
			fprintf (stderr, "%s: fatal error at %s(%d).\n",
				 program, __FILE__, __LINE__);
			return (EXIT_FAILURE);
		}
		cap_surrender (ocap);

		/* get moldy file attributes */
		ocap = cap_acquire (lstat_ncap, lstat_caps);
		c += (lstat (argv[optind], &moldy_st) == -1) ? 1 : 0;
		cap_surrender (ocap);

		/* go back to our normal label */
		ocap = cap_acquire (2, caps_for_mac_set_proc);
		if (mac_set_proc (pmac) == -1)
		{
			cap_surrender (ocap);
			fprintf (stderr, "%s: fatal error at %s(%d).\n",
				 program, __FILE__, __LINE__);
			return (EXIT_FAILURE);
		}
		cap_surrender (ocap);

		/* get non-moldy file attributes */
		ocap = cap_acquire (lstat_ncap, lstat_caps);
		c += (lstat (argv[optind], &plain_st) == -1) ? 1 : 0;
		file_mac = mac_get_file (argv[optind]);
		cap_surrender (ocap);

		/* go to next file on error */
		if (file_mac == NULL || c)
		{
			perror (argv[optind]);
			mac_free (file_mac);
			continue;
		}

		/* 
		 * Verify that using a moldy label doesn't result
		 * in changing the wrong file's label. In other words,
		 * avoid changing the label of a file through a path
		 * which does moldy redirection.
		 */
		if (plain_st.st_ino != moldy_st.st_ino ||
		    plain_st.st_dev != moldy_st.st_dev)
		{
			fprintf (stderr, "%s: %s, %s.\n",
				 program, argv[optind],
			     "Multilevel directory redirection confusion.");
			mac_free (file_mac);
			continue;
		}

		/* create moldy new_mac as appropriate */
		if (mflag)
			new_mac = mac_set_moldy (file_mac);

		/* 
		 * Only directories can be moldy.
		 */
		if (!S_ISDIR (plain_st.st_mode) && mac_is_moldy (new_mac))
		{
			fprintf (stderr,
				 "%s: Only directories can be moldy.\n",
				 argv[optind]);
			mac_free (file_mac);
			if (mflag)
				mac_free (new_mac);
			continue;
		}

		/* 
		 * Shortcut out if the old and new labels are the same.
		 * Moldy and normal labels compare the same with
		 * mac_equal(), so we must also check the label types
		 * here e.g. it should be possible to set a
		 * msenlow/minthigh file to msenmldlow/minthigh.
		 */
		if (new_mac->ml_msen_type == file_mac->ml_msen_type &&
		    new_mac->ml_mint_type == file_mac->ml_mint_type &&
		    mac_equal (new_mac, file_mac))
		{
			mac_free (file_mac);
			if (mflag)
				mac_free (new_mac);
			continue;
		}

		/* 
		 * Do the deed, if at all possible.
		 */
		ocap = cap_acquire (9 - msf_ncap + (force ? 1 : 0),
				    &caps_for_mac_set_file[msf_ncap]);
		if (mac_set_file (argv[optind], new_mac) == -1)
			perror (argv[optind]);
		cap_surrender (ocap);

		mac_free (file_mac);
		if (mflag)
			mac_free (new_mac);
	}

	if (!mflag)
		mac_free (new_mac);
	return (EXIT_SUCCESS);
}
