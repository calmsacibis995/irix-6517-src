/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.5 $"

/*
 * Tool to parse a lvtab file and generate an input file to xlv_make
 * to create the same logical volume configuration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/lvtab.h>

extern void		xlv_freelvent(struct lvtabent *tabent);
extern struct lvtabent	*xlv_getlvent(FILE *lvtabp, FILE *comment_stream);

#define	LVTAB	"/etc/lvtab"

char	*lvtab_name = NULL;
char	*sink_name = NULL;
FILE	*file = NULL;

char *progname = "lv_to_xlv";

/*
 * Create a xlv entry for this lv entry.
 *
 * Note that lv only has a XLV "data" subvolume equivalent and
 * lv does not support plexing.
 */
void
lv_to_xlv_entry(FILE *stream, struct lvtabent *lv)
{
	int	ndevs;
	int	i;


	fprintf(stream, "# Volume description: %s\n", lv->volname);

	fprintf(stream, "vol %s\n", lv->devname);
	fprintf(stream, "data\n");
	fprintf(stream, "plex\n");

	ndevs = 0;
	while (ndevs < lv->ndevs) {
		fprintf(stream, "ve -force");
		if (lv->stripe > 1) {
			fprintf(stream, " -stripe");
			if (lv->gran > 0)
				fprintf(stream, " -stripe_unit %d", lv->gran);
		}
		for (i = 0; i < lv->stripe; i++) {
			fprintf(stream, " %s", lv->pathnames[ndevs]);
			ndevs++;
		}
		fprintf(stream, "\n");
	}

	fprintf(stream, "end\n");
} /* end of lv_to_xlv_entry() */


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-f file] [-o sink]\n", progname);
	fprintf(stderr, "-f <file>  : Use <file> as the lvtab file.\n");
	fprintf(stderr, "-o <sink>  : Write xlv equivalence to <sink>.\n");
	exit(1);
}

void
main (int argc, char **argv)
{
	int		ch;
	FILE		*stream;
	struct lvtabent *lv_entry;
	char		buf[100];

#if 0
	if (getuid()) {
		fprintf(stderr, "%s: must be started by super-user\n", argv[0]);
		exit(1);
	}
#endif

	while ((ch = getopt(argc, argv, "f:o:")) != EOF)
		switch((char)ch) {
		case 'f':
			lvtab_name = optarg;
			break;
		case 'o':
			sink_name = optarg;
			break;
		default:
			usage();
		}

	if (argc -= optind)
		usage();

	if (lvtab_name == NULL)
		lvtab_name = LVTAB;

	if (! (file = fopen(lvtab_name, "r"))) {
		sprintf(buf, "Cannot open %s", lvtab_name);
		perror(buf);
		exit(1);
	}

	if (sink_name) {
		if (! (stream = fopen(sink_name, "wb"))) {
			sprintf(buf, "Cannot write to %s", sink_name);
			perror(buf);
			exit(1);
		}
	} else {
		stream = stdout;
	}

	/*
	 * Read each lv entry in the lvtab file and
	 * create a corresponding xlv entry.
	 */
	while (lv_entry = xlv_getlvent (file, stream)) {
		if (lv_entry->ndevs)
			lv_to_xlv_entry(stream, lv_entry);
		xlv_freelvent(lv_entry);
	}
	fprintf(stream, "exit\n");
	fclose(stream);

	exit(0);

} /* main() */
