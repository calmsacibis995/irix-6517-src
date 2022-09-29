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

/*
 * newlabel - create a new process at the requested label
 *
 *	E	don't close and reopen stderr
 *	F	don't close and reopen stderr,in,out
 *	I	don't close and reopen stdin
 *	O	don't close and reopen stdout
 *	a	all subdir labels for a moldy dir
 *	e	reopen stderr as next arg
 *	f	reopen stderr,in,out as next arg
 *	h	all non-equal subdir labels for a moldy dir
 *	i	reopen stdin as next arg
 *	o	reopen stdout as next arg
 *	m	change label to moldy
 *	t	change label to TCSEC
 *
 */

#ident	"$Revision: 1.13 $"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <dirent.h>
#include <stdio.h>
#include <pwd.h>
#include <ia.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <strings.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <clearance.h>

static mac_label read_mold = {MSEN_MLD_HIGH_LABEL, MINT_EQUAL_LABEL, };

static char namebuf_in[MAXPATHLEN] = "/dev/null";	/* new stdin  */
static char namebuf_out[MAXPATHLEN] = "/dev/null";	/* new stdout */
static char namebuf_err[MAXPATHLEN] = "/dev/null";	/* new stderr */

int
main(int argc, char *argv[], char **envp)
{
	int c;				/* For use by getopt(3) */
	int label_gotten = 0;		/* set if label is computed */
	mac_t plabel;			/* original process label value */
	mac_t lp;			/* The desired label value */
	struct clearance *clp;		/* user clearance */
	struct passwd *pwd;		/* passwd entry */ 
	int do_equals = 0;		/* differentiate  -a and -h */
	char *ttyis;			/* return from ttyname(3) */
	char *molddir = NULL;		/* target dir for -a and -h */
	char *shell = NULL;		/* user's preferred shell */
	char *program = argv[0];	/* program name saved */
	char *toopen_in = namebuf_in;	/* what to reopen stdin  as */
	char *toopen_out = namebuf_out;	/* what to reopen stdout as */
	char *toopen_err = namebuf_err;	/* what to reopen stderr as */
	char *lsc_msg = "%s: Label specification conflict\n";
	char *EIO_msg = "newlabel -%c requires super-user privilege\n";
	char *lbl_msg = "newlabel: cannot get process label\n";

	/*
	 * The desired label may be a deviant of the current
	 * label. Fetch it.
	 */
	plabel = mac_get_proc();
	lp = mac_get_proc();
	if (plabel == (mac_t) NULL || lp == (mac_t) NULL) {
		printf(lbl_msg);
		return(1);
	}

	/*
	 * Parse command line arguments
	 */
	while ((c = getopt(argc, argv, "EFIOa:e:f:h:i:mo:t")) != -1) {
		switch (c) {
		case 'E':
			if (getuid() != 0) {
				printf(EIO_msg, c);
				exit(1);
			}
			toopen_err = NULL;
			break;
		case 'F':
			if (getuid() != 0) {
				printf(EIO_msg, c);
				exit(1);
			}
			toopen_err = NULL;
			toopen_in = NULL;
			toopen_out = NULL;
			break;
		case 'I':
			if (getuid() != 0) {
				printf(EIO_msg, c);
				exit(1);
			}
			toopen_in = NULL;
			break;
		case 'O':
			if (getuid() != 0) {
				printf(EIO_msg, c);
				exit(1);
			}
			toopen_out = NULL;
			break;
		case 'a':
			do_equals = 1;
			/* NO BREAK */
		case 'h':
			if (getuid() != 0) {
				printf(EIO_msg, c);
				exit(1);
			}
			molddir = optarg;
			break;
		case 'e':
			toopen_err = optarg;
			break;
		case 'f':
			toopen_err = optarg;
			toopen_in = optarg;
			toopen_out = optarg;
			break;
		case 'i':
			toopen_in = optarg;
			break;
		case 'm':
			if (label_gotten++) {
				printf(lsc_msg, c);
				exit(1);
			}
			switch(lp->ml_msen_type) {
			case MSEN_TCSEC_LABEL:
				lp->ml_msen_type = MSEN_MLD_LABEL;
				break;
			case MSEN_LOW_LABEL:
				lp->ml_msen_type = MSEN_MLD_LOW_LABEL;
				break;
			case MSEN_HIGH_LABEL:
				lp->ml_msen_type = MSEN_MLD_HIGH_LABEL;
				break;
			}
			break;
		case 'o':
			toopen_out = optarg;
			break;
		case 't':
			if (label_gotten++) {
				printf(lsc_msg, c);
				exit(1);
			}
			switch (lp->ml_msen_type) {
			case MSEN_MLD_LABEL:
				lp->ml_msen_type = MSEN_TCSEC_LABEL;
				break;
			case MSEN_MLD_LOW_LABEL:
				lp->ml_msen_type = MSEN_LOW_LABEL;
				break;
			case MSEN_MLD_HIGH_LABEL:
				lp->ml_msen_type = MSEN_HIGH_LABEL;
				break;
			}
			break;
		case '?':
		default:
			printf("newlabel: unknown option\n");
			exit(1);
		}
	}

	if (molddir != NULL) {
		DIR *dp;		/* dir pointer */
		struct dirent *dep;	/* dir entry */
		struct stat statbuf;	/* for stat */
		char path[MAXPATHLEN];	/* to form full path */
		/*
		 * Make this process able to read the specified
		 * moldy directory. Note that the superuser check
		 * has already been made.
		 */
		if (mac_set_proc(&read_mold) == -1) {
			printf("%s: Label change failure.\n", program);
			exit(1);
		}
		if ((dp = opendir(molddir)) == NULL) {
			printf("%s: Directory open failure.\n", program);
			exit(1);
		}
		while ((dep = readdir(dp)) != NULL) {
			if (strcmp(dep->d_name, ".") == 0)
				continue;
			if (strcmp(dep->d_name, "..") == 0)
				continue;
			sprintf(path, "%s/%s", molddir, dep->d_name);
			if (stat(path, &statbuf) < 0) {
				/* Very Strange! */
				continue;
			}
			if ((statbuf.st_mode & S_IFMT) != S_IFDIR)
				continue;
			mac_free(lp);
			if ((lp = mac_get_file(path)) == NULL) {
				/* Very Strange! */
				continue;
			}
			if (do_equals == 0 &&
			    (lp->ml_msen_type == MSEN_EQUAL_LABEL ||
			     lp->ml_mint_type == MINT_EQUAL_LABEL))
				continue;
			if (fork() == 0) {
				closedir(dp);
				label_gotten = 1;
				break;
			}
			(void) wait(&c);
		}
		/*
		 * Exit gracefully from the parent.
		 */
		if (!label_gotten) {
			exit(0);
		}
	}

	if ((pwd = getpwuid(getuid())) == NULL) {
		printf("%s: User data is unobtainable\n", program);
		exit(1);
	}
	if ((clp = sgi_getclearancebyname(pwd->pw_name)) == NULL) {
		printf("%s: Clearance is unobtainable\n", program);
		exit(1);
	}

	if (!label_gotten) {
		int state;
		/*
		 * Get the label from the command line.
		 */
		if (optind >= argc) {
			printf("%s: No label specified\n", program);
			exit(1);
		}
		if ((state = mac_cleared(clp,argv[optind]))!=MAC_CLEARED) {
			char *message;
			/*
			 * A few custom messages.
			 */
			switch (state) {
        		    case MAC_LBL_TOO_LOW:
				message =
				"requested label is below your allowed range";
				break;
        		    case MAC_LBL_TOO_HIGH:
				message =
				"requested label is above your allowed range";
				break;
        		    case MAC_INCOMPARABLE:
				message =
				"requested label is outside your allowed range";
				break;
			    default:
				message = mac_clearance_error(state);
			}

			printf("%s: %s: %s.\n", program, argv[optind],
			    message);
			exit(1);
		}
		lp = mac_from_text(argv[optind]);
		optind++;
	}

	/*
	 * Verify that the new label dominates the old.
	 * superuser is exempt.
	 */
	if (getuid() != 0 && mac_dominate(lp, plabel) == 0) {
		printf("%s: requested label doesn't dominate current label.\n",
		       program);
		exit(1);
	}

	/*
	 * Find name of currently open std in, out, & err
	 */
	if (ttyis = ttyname(0))
		strcpy(namebuf_in, ttyis);
	if (ttyis = ttyname(1))
		strcpy(namebuf_out, ttyis);
	if (ttyis = ttyname(2))
		strcpy(namebuf_err, ttyis);

	/*
	 * Reset process label.
	 * Do this before closing so that errors can be reported.
	 */
	if (mac_set_proc(lp) == -1) {
		perror("newlabel: cannot change label");
		exit(1);
	}

	/*
	 * Shut down all open files, except those which which must remain
	 * for the super-user.
	 */
	for (c = getdtablesize(); c >= 3; c--)
		close(c);
	if (toopen_err)
		close(2);
	if (toopen_out)
		close(1);
	if (toopen_in)
		close(0);

	/*
	 * Reset process attributes. Do this with communication to the
	 * outside world closed down.
	 */
	if (setuid(getuid()) < 0)
		exit(1);

	/*
	 * Attempt to reopen stdin, stdout, and stderr.
	 * If this fails (probably due to MAC policy) open /dev/null
	 * instead. If that fails accept the fact and continue in
	 * hopes that all will be for the best.
	 */
	if (toopen_in && open(toopen_in, O_RDONLY) < 0)
		(void)open("/dev/null", O_RDWR);

	if (toopen_out && open(toopen_out, O_RDWR|O_CREAT|O_APPEND, 0600) < 0)
		(void)open("/dev/null", O_RDWR);

	if (toopen_err && open(toopen_err, O_RDWR|O_CREAT|O_APPEND, 0600) < 0)
		(void)open("/dev/null", O_RDWR);

	/*
	 * Find the user's preferred shell
	 */
	if (pwd->pw_shell) {
		shell = strrchr(pwd->pw_shell, (int)'/');
		if (shell)
			shell++;
		else
			shell = pwd->pw_shell;
	}

	/*
	 * If no program is specified invoke a shell
	 */

	if (argc == optind) {
		if (shell)
			execl(pwd->pw_shell, shell, NULL);
		execl("/bin/csh", "csh", NULL);
		execl("/bin/sh", "sh", NULL);
		execl("/bin/echo", "/bin/echo", "newlabel: No Shell", NULL);
		exit(1);
	}
	else {
		/*
		 * build an argument string (code borrowed from rsh)
		 * then exec sh -c "command arguments"
		 */
		int len;
		char **argp;
		char *cp, *args;

		len = 0;
		for (argp = argv + optind; *argp; argp++)
			len += strlen(*argp) + 1;
		cp = args = malloc(len);
		for (argp = argv + optind; *argp; argp++) {
			strcpy(cp, *argp);
			while (*cp)
				cp++;
			if (argp[1])
				*cp++ = ' ';
		}

		if (shell)
			execl(pwd->pw_shell, shell, "-c", args, NULL);
		execl("/bin/sh", "sh", "-c", args, NULL);
		execl("/bin/echo", "/bin/echo", "newlabel: No Shell", NULL);
		exit(1);
	}
	return(0);
}
