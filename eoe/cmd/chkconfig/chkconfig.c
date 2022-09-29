/*
 * chkconfig.c --
 *
 * 	Print or change configuration flag files in /etc/config.
 *
 * Copyright 1988,1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
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
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <paths.h>
#include <string.h>
#include <unistd.h>

#define LINE_SIZE 128
#define FIELDWIDTH 20		/* when printing, width of flag name field */

/* Exit statuses */
#define ON	0
#define OFF	1
#define ERROR	2

#define NUM_FLAGS 1000
static struct flag {
    char name[LINE_SIZE];	/* flag name (not including directory) */
    int  state;			/* if >= 0, ON or OFF, else negated errno */
} flags[NUM_FLAGS];

static int  set_flag_state(char *, int, int);
static int  get_flag_state(char *);
static void print_flags(struct flag *, int);
static char *progname;


static void
usage(char *str)
{
	if (str) {
	    fprintf(stderr, "%s: %s\n", progname, str);
	}
	fprintf(stderr, "usage:\n\
\tchkconfig [-s]\t\t\t# print state of all flags\n\
\tchkconfig <flag>\t\t# test flag state\n\
\tchkconfig [-f] <flag> <state>\t# set flag 'on' or 'off'\n");
	exit(2);
}


main(argc, argv)
	int argc;
	char *argv[];
{
	char flag_name[LINE_SIZE];
	int  state;
	DIR  *dirp;
	register struct dirent *dp;
	int c;
	int force = 0;
	int sort_by_state = 0;
	char *cp;

	if ((cp = strrchr(argv[0], '/')) != NULL) {
	    progname = cp+1;
	} else {
	    progname = argv[0];
	}

	while ((c = getopt(argc, argv, "fs")) != EOF) {
	    switch (c) {
		case 'f':
		    force = 1;
		    if (sort_by_state) {
			usage("-f incompatible with -s");
			/*NOTREACHED*/
		    }
		    break;
	        case 's':
		    sort_by_state = 1;
		    if (force) {
			usage("-s incompatible with -f");
			/*NOTREACHED*/
		    }
		    break;
		default:
		    usage(NULL);
		    /*NOTREACHED*/
	    }
	}
	/* Skip past options */
	if (force) {
	    if (argc != 4) {
		usage("wrong number of arguments");
		/*NOTREACHED*/
	    }
	    argc--;
	    argv++;
	} else if (sort_by_state) {
	    if (argc != 2) {
		usage("-s does not allow arguments");
		/*NOTREACHED*/
	    }
	    argc--;
	    argv++;
	} else if (argc > 3) {
	    usage("wrong number of arguments");
	    /*NOTREACHED*/
	}

	if (argc == 1) {
	    register struct flag *fp = flags;
	    char *dot;

	    printf("\t%-*s %-*s\n",   FIELDWIDTH, "Flag", FIELDWIDTH, "State");
	    printf("\t%-*s %-*s\n\n", FIELDWIDTH, "====", FIELDWIDTH, "=====");

	    if ((dirp = opendir(_PATH_CONFIG)) == NULL) {
		fprintf(stderr, "%s: unable to access directory %s: \"%s\"\n", 
			    progname, _PATH_CONFIG, strerror(errno));
		exit(ERROR);
	    }
	    while ( (dp = readdir(dirp)) != NULL) {
		/* Skip any name that has . in it (includes "." and "..") */
		if (strchr(dp->d_name, '.') != NULL) {
		    continue;
		}
		/* Skip files with .options suffix */
		if ((dot = strrchr(dp->d_name, '.')) != NULL &&
		    !strcasecmp(dot, ".options")) {
		    continue;
		}
		(void) strcpy(flag_name, _PATH_CONFIG);
		(void) strcat(flag_name, "/");
		(void) strcat(flag_name, dp->d_name);
		(void) strcpy(fp->name, dp->d_name);
		fp->state = get_flag_state(flag_name);
		if (++fp > &flags[NUM_FLAGS]) {
		    print_flags(fp, sort_by_state);
		    fp = flags;
		}
	    }
	    if (fp != flags) {
		print_flags(fp, sort_by_state);
	    }
	    state = 0;
	} else if (argc == 2) {
	    (void) strcpy(flag_name, _PATH_CONFIG);
	    (void) strcat(flag_name, "/");
	    (void) strcat(flag_name, argv[1]);
	    if (get_flag_state(flag_name) == ON) {
		state = ON;
	    } else {
		state = OFF;	/* it's off or can't read the file */
	    }
	} else {
	    (void) strcpy(flag_name, _PATH_CONFIG);
	    (void) strcat(flag_name, "/");
	    (void) strcat(flag_name, argv[1]);
	    if (strncasecmp("on", argv[2], 2) == 0) {
		state = ON;
	    } else if (strncasecmp("off", argv[2], 3) == 0) {
		state = OFF;
	    } else {
		usage("invalid state: use \"on\" or \"off\"");
		/*NOTREACHED*/
	    }
	    if (set_flag_state(flag_name, state, force)) {
		fprintf(stderr, "%s: cannot set flag \"%s\": %s\n", 
			    progname, argv[1], strerror(errno));
		if (errno == ENOENT) {
		    fprintf(stderr, 
		      "use \"chkconfig -f %s %s\" to create and set the flag\n",
			argv[1], argv[2]);
		}
		state = ERROR;
	    }
	}
	exit(state);
	/*NOTREACHED*/
}


static int
set_flag_state(char * fname, int fstate, int force)
{
	FILE *fp;

	if (!force && access(fname, F_OK) < 0) {
	    return(1);
	}
	if ((fp = fopen(fname, "w")) == NULL) {
	    return(1);
	}
	if (fstate == ON) {
	    fwrite("on\n", 3, 1, fp);
	} else {
	    fwrite("off\n", 4, 1, fp);
	}
	fclose(fp);
	return(0);
}

static int
get_flag_state(char *namep)
{
	char line[LINE_SIZE];
	FILE *fp;
	int  state = OFF;	/* assume off */

	if ((fp = fopen(namep, "r")) == NULL) {
	    state = -errno;
	} else {
	    if ((fgets(line, LINE_SIZE, fp) != NULL) &&
	        (strncasecmp(line, "on", 2) == 0)) {
		    state = ON;
	    }
	    fclose(fp);
	}
	return(state);
}

static int
cmpstate(struct flag *f1, struct flag *f2)
{
	if (f1->state < f2->state) {
	    return(-1);
	} else if (f1->state == f2->state) {
	    return(strcasecmp(f1->name, f2->name));
	} else {
	    return(1);
	}
}

static int
cmpname(struct flag *f1, struct flag *f2)
{
	return(strcasecmp(f1->name, f2->name));
}

typedef int (*qsort_cmp)(const void *, const void *);

static void
print_flags(struct flag *fp, int sort_by_state)
{
	register struct flag *f;

	qsort(flags, (size_t) (fp - flags), sizeof(*fp), 
		sort_by_state ? (qsort_cmp)cmpstate : (qsort_cmp)cmpname);
	for (f = flags; f < fp; f++) {
	    printf("\t%-*s ", FIELDWIDTH, f->name);
	    switch (f->state) {
		case ON:
		    printf("on\n");
		    break;
		case OFF:
		    printf("off\n");
		    break;
		default:
		    printf("?   (cannot open: %s)\n", strerror(-f->state));
		    break;
	    }
	}
}
