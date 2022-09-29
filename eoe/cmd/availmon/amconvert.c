#ident "$Revision: 1.4 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <time.h>
#include <sys/types.h>
#include <sys/param.h>
#include "defs.h"

#define PIPE		'|' /* the field separator for availlog records */

char	cmd[MAX_STR_LEN];
char	file[MAXPATHLEN];
FILE	*fp = NULL, *fp1 = NULL;

int	nreports;
char	*usage = "Usage: %s [ -o <old_logfile> } { -n <new_logfile> ]\n";


main(int argc, char *argv[])
{
    FILE	*fp = NULL, *fp1 = NULL;
    int		c, errflg = 0, bad = 1, inputsum = 0, t, sumlen = 0, sr;
    int		checkfru = 0;
    time_t	ti, frutime;
    char	linebuf[MAX_LINE_LEN], word[MAX_LINE_LEN], sumbuf[BUFSIZ];
    char	*p;

    strcpy(cmd, basename(argv[0]));

    /* process online options */
    while ((c = getopt(argc, argv, "o:n:t:")) != EOF)
	switch (c) {
	case 'o':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else if ((fp = fopen(optarg, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open file %s\n", cmd, optarg);
		exit(-1);
	    }
	    break;
	case 'n':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else if ((fp1 = fopen(optarg, "w")) == NULL) {
		fprintf(stderr, "%s: cannot open file %s\n", cmd, optarg);
		exit(-1);
	    }
	    break;
	case 't':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else {
		checkfru = 1;
		frutime = atol(optarg);
	    }
	    break;
	case '?':
	default:
	    errflg++;
	}

    if (errflg) {
	fprintf(stderr, usage, cmd);
	exit(-1);
    }

    if (fp == NULL)
	fp = stdin;

    if (fp1 == NULL)
	fp1 = stdout;

    while (fgets(linebuf, MAX_LINE_LEN, fp) != NULL) {
	 if (strncmp("STOP|", linebuf, 5) == 0) {
	    if (getword(linebuf, 2, word) == -1) {
	  	sr = 0;
	    } else {
	  	sr = atol(word);
	    }
	    if (getword(linebuf, 1, word) == -1) {
		ti = -1;
	    } else {
		ti = atol(word);
	    }
	    if (checkfru) {
		if (checkfru > 0) {
	    	    if (ti >= frutime) {
			checkfru = -1;
			if (sr == -3)
			    sr = -14;
		    }
		} else {
		    if (sr == -3)
			sr = -14;
		}
	    }
	    if (ti == -1)
		fprintf(fp1, "EVENT|%d|%d|%s", sr, ti, "unknown");
	    else
		fprintf(fp1, "EVENT|%d|%d|%s", sr, ti, ctime(&ti));
	} else {
	    fputs(linebuf, fp1);
	}
    }

    fclose(fp);
    fclose(fp1);
}

/*
 * words are numbered 0,1,2 and are in format 0|1|2<NULL> (slash is
 * the field separator)
 */
int
getword(char *line, int wno, char *word)
{
    int i;
    char *p1, *p2;

    for (i = 0, p1 = line; i < wno; i++)
	if ((p2 = strchr(p1, PIPE)) == NULL)
	    break;
	else
	    p1 = ++p2; /* start looking from next field */

    if (i != wno)
	return -1;
    /*
     * we have looked at wno-1 separators (wno-1 words),  p1 points
     * at beginnig of wno'th word; find end of wno'th word 
     */
    if ((p2 = strchr(p1, PIPE)) == NULL) /* was the last word in line */
	strcpy(word, p1);
    else {
	*p2 = NULL; /* will repair after copying */
	strcpy(word, p1);
	*p2 = PIPE;
    }
    return 0;
}
