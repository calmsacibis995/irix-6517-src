#include	<getopt.h>
#include 	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<sys/lock.h>
#include	"osview.h"

# define	DEFINTV		5

int		lines = 0;	/* maximum lines to use */
int		counts = 0;	/* counts only? or interval */
int		interval = DEFINTV;
int		persec = 1;
cell_t		cellid = -1;
int		ps;
int		categ = 1;

static char		*pname;

main(int argc, char *argv[])
{
   int		c;
   int		err = 0;
   int		i;

	pname = argv[0];
	while ((c = getopt(argc, argv, "an:i:scC:")) != EOF) {
		switch (c) {
		case 'a':
			categ = 0;
			break;
		case 'n':
			if ((lines = strtol(optarg, (char **) 0, 0)) <= 0)
				err++;
			break;
		case 'i':
			if ((interval = strtol(optarg, (char **) 0, 0)) <= 0)
				err++;
			break;
		case 's':
			persec = 0;
			break;
		case 'c':
			/* display count from 'reset' */
			counts = 1;
			persec = 0;
			break;
		case 'C':
			/* display specific cell */
			cellid = atoi(optarg);
			break;
		case '?':
			err++;
			break;
		}
	}
	if (err) {
		fprintf(stderr, "usage: %s [-a][-i sec][-n lines][-s][-c][-u unix]\n", pname);
		exit(1);
	}
	for (i = optind; i < argc; i++) {
		fprintf(stderr, "%s: extra argument %s ignored\n", pname,
			argv[i]);
	}
	ps = getpagesize();
	runit();
	exit(0);
	/* NOTREACHED */
}
