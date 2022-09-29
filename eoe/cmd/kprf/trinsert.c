#ident  "$Revision: 1.1 $"

/*
 * Program to insert function entry/exit points in kernel assembly code
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<fcntl.h>


/* Tramp stack size */
#define	BUMPUP		16

struct	nameent {
	char	name[28];
	int	ord;
};
#define	MAXNAMETAB	10000

int	numnames;
struct	nameent	nametab[MAXNAMETAB];

char	*ordfile = NULL;


/*
 * Comment lines
 */
comment(char *line, FILE *fp)
{
	if (line[1] == '#') {
		fputs(line, fp);
		return 1;
	}

	return 0;
}

/*
 * Put out the code for the _gcount call
 */
emit_gcount_jal(FILE *fp, int ord)
{
	fprintf(fp, "\tli\t$1,%d\n", ord);
	fprintf(fp, "\tmove\t$12,$31\n");
	fprintf(fp, "\tjal\t_trentry\n");
	fprintf(fp, "\tnop\n");
	fprintf(fp, "\tmove\t$31,$24\n");
}

/*
 * Do all the operations needed on the function block
 */
process_function(FILE *fpi, FILE *fpo, int ord)
{
	char line[BUFSIZ], inst[BUFSIZ];
	char arg1[BUFSIZ], arg2[BUFSIZ], arg3[BUFSIZ];
	int first = 0, stack = 0, frame = 0, index, value;

	/* Read source file, do function interior work */
	while (fgets(line, sizeof line, fpi) != NULL) {
		/* Skip over comments */
		if (comment(line, fpo)) {
			continue;
		}

		/* Do some primitive parsing */
		arg1[0] = arg2[0] = arg3[0] = NULL;
		sscanf(line, "%s %s %s %s", inst, arg1, arg2, arg3);

		/* Stop at function end */
		if (!strcmp(inst, ".end")) {
			fputs(line, fpo);
			return;
		}

		/* Look for first non-directive */
		if (!first && (inst[0] != '.')) {
			emit_gcount_jal(fpo, ord);
			first = 1;
		}

		/* Look for use of $ra in generating jumps in case stmnts */
		if (!strcmp(inst, "ld") && !strcmp(arg1, "$31,")) {
			fputs(line, fpo);

			arg3[0] = NULL;
			sscanf(arg2, "%d($%2s", &index, arg3);

			if (!strcmp(arg3, "31")) {
				fgets(line, sizeof line, fpi);

				/* Do some primitive parsing */
				arg1[0] = arg2[0] = arg3[0] = NULL;
				sscanf(line, "%s %s %s %s", inst, arg1, arg2, arg3);
				if (!strcmp(inst, "j") && !strcmp(arg1, "$31")) {
					fputs(line, fpo);
					continue;
				}
				/* Otherwise fall through and process the line */
			}
			else
				continue;
		}

		/* Trace exit replacement */
		if (!strcmp(inst, "j") && !strcmp(arg1, "$31")) {
			fprintf(fpo, "\tli\t$1,%d\n", ord);
			fprintf(fpo, "\tmove\t$12,$31\n");
			fprintf(fpo, "\tj\t_trexit\n");
			continue;
		}

		/* Watch for .frame directives */
		if (!strcmp(inst, ".frame")) {
			/* error check (only one .frame per function) */
			if (frame) {
				fprintf(stderr, "second .frame's!!!\n");
			}

			/* get the local stack size */
			sscanf(arg2, "%d", &frame);
			fputs(line, fpo);
			continue;
		}


		fputs(line, fpo);
	}
}

/*
 * Main
 */
main(int argc, char **argv)
{
	char line[1024];
	FILE *fpi, *fpo;
	int c;
	char	inst[128];
	char	arg1[128], arg2[128], arg3[128];
	int	fd;
	int	n;
	int	ord;

	/* Argument parser */
	while ((c = getopt(argc, argv, "f:")) != EOF) {
		switch (c) {
			case 'f':
				ordfile = optarg;
		}
	}

	if (ordfile == NULL) {
		fprintf(stderr, "Usage:  %s [-tg] -f ordfile infile outfile\n", argv[0]);
		exit(1);
	}

	/* Read in binary ordfile */
	if ((fd = open(ordfile, O_RDONLY)) < 0)
		syserr(ordfile);
	if ((n = read(fd, nametab, MAXNAMETAB * sizeof (struct nameent))) < 0)
		syserr("read ordfile");
	close(fd);
	numnames = n / sizeof (struct nameent);

	/* Open source file */
	if ((fpi = fopen(argv[optind], "r")) == NULL) {
		fprintf(stderr, "src: fopen(%s,\"r\") - %m", argv[optind]);
		exit(-1);
	}

	/* Open destination file */
	if ((fpo = fopen(argv[optind+1], "w")) == NULL) {
		fprintf(stderr, "src: fopen(%s,\"w\") - %m", argv[optind+1]);
		exit(-2);
	}

	/* Read source file, add _gcount calls */
	while (fgets(line, sizeof line, fpi) != NULL) {
		/* Skip over comments */
		if (comment(line, fpo)) {
			continue;
		}

		/* Search for a function entry point */
		if (!strncmp(&line[1], ".ent", 4)) {

		/* Do some primitive parsing */
		arg1[0] = arg2[0] = arg3[0] = NULL;
		sscanf(line, "%s %s %s %s", inst, arg1, arg2, arg3);

			/* Emit entry */
			fputs(line, fpo);


			fgets(line, sizeof line, fpi);
			fputs(line, fpo);
			if (!strncmp(&line[1], ".globl", 6)) {
				/* If previous line was ".globl"
				   then next line is the function label
				   else the previous line itself
				   was the function label
				*/
				fgets(line, sizeof line, fpi);
				fputs(line, fpo);
			}
			ord = findname(arg1);

			/* Process this function */
			process_function(fpi, fpo, ord);
		} else {
			fputs(line, fpo);
		}
	}

	/* close & exit */
	fclose(fpo);
	fclose(fpi);
}

/*
 * Binary sort exact match.
 */
findname(name)
char	*name;
{
	int	i, low, high;

	low = 0;
	high = numnames;
	i = (low + high)/2;

	while (1) {
		if (strcmp(name, nametab[i].name) == 0)
			return (nametab[i].ord);

		if ((high - low) <= 1)
			break;

		if (strcmp(name, nametab[i].name) < 0)
			high = i;
		else
			low = i;
		i = (low + high) / 2;
	}

	return (0);	/* unrecognized name */
}

syserr(s)
char	*s;
{
	perror(s);
	exit(1);
}
