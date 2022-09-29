/*
 * Program to insert function entry/exit points in kernel assembly code
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	"trace.h"


/* Tramp stack size */
#define	BUMPUP		16

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
	/* Note that we place the instruction exit code immediately
	 * following the entrance preamble.  Since we now have multiple
	 * exit points and since the assembly code emitted from the "cc"
	 * has statements like "beq rx, ry, .-156" it is required that we
	 * not change the number of statements in the ".s".  So all exits
	 * now branch back to the exit postamble.
	 * NOTE: _trentry knows that the exit postamble follows the
	 * entrance preamble and it adjusts the "ra" value to skip the
	 * exit postamble.
	 */

	/* _trentry pre-amble */

	fprintf(fp, "\tmove\t$24,$31\n");
	fprintf(fp, "\tjal\t_trentry\n");
	fprintf(fp, "\tli\t$1,%d\n", ord);

	/* _trexit post-amble */

	fprintf(fp, "trexit%d:\tj\t_trexit\n", ord);
	fprintf(fp, "\tli\t$1,%d\n", ord);

}

/*
 * Do all the operations needed on the function block
 */
process_function(FILE *fpi, FILE *fpo, int ord)
{
	char line[BUFSIZ], inst[BUFSIZ];
	char arg1[BUFSIZ], arg2[BUFSIZ], arg3[BUFSIZ];
	int first = 0, stack = 0, frame = 0, index, value, scancnt;

	/* Read source file, do function interior work */
	while (fgets(line, sizeof line, fpi) != NULL) {
		/* Skip over comments */
		if (comment(line, fpo)) {
			continue;
		}

		/* Do some primitive parsing */
		arg1[0] = arg2[0] = arg3[0] = NULL;
		scancnt = sscanf(line, "%s %s %s %s", inst, arg1, arg2, arg3);

		/* Stop at function end */
		if (!strcmp(inst, ".end")) {
			fputs(line, fpo);
			return;
		}

		/* if no ord number, then don't add trace code to procedure.
		 * Since the trace code uses the ord number to generate a
		 * unique label we don't want to end up with multiple labels
		 * with 0 (besides we can't enable the trace anyway).
		 */
		if (ord == 0) {
#ifndef COMPILER_BUG462195_FIXED
			if ((!strcmp(inst, "dsll32")) && !strcmp(arg3, "$0")) {

				printf("workaround bug 462195\n");
				fprintf(fpo,"\t%s\t%s%s0\n", inst, arg1, arg2);
				continue;
			}
#endif /* COMPILER_BUG462195_FIXED */		
			fputs(line, fpo);
			continue;
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
				if (!strcmp(inst, "jr") && !strcmp(arg1, "$31")) {
					fputs(line, fpo);
					continue;
				}
				/* Otherwise fall through and process the line */
			}
			else
				continue;
		}
#ifndef COMPILER_BUG464284_FIXED
		if ((!strcmp(inst, "move")) && !strcmp(arg1, arg2)) {
			printf("POSSIBLE BUG 464284: \t%s\t%s\t%s\n",
			       inst, arg1, arg2);
		}
#endif /* COMPILER_BUG464284_FIXED */
#ifndef COMPILER_BUG462195_FIXED
		if ((!strcmp(inst, "dsll32")) && !strcmp(arg3, "$0")) {

			printf("workaround bug 462195\n");
			fprintf(fpo,"\t%s\t%s%s0\n", inst, arg1, arg2);
			continue;
		}
#endif /* COMPILER_BUG462195_FIXED */		

		/* Trace exit replacement */
		if (!strcmp(inst, "jr") && !strcmp(arg1, "$31")) {
			fprintf(fpo, "\tj\ttrexit%d\n", ord);
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
#ifndef COMPILER_BUG462063_FIXED
			/* compiler ".s" output may contain a value which the 
			 * assembler can not handle (double word entry with
			 * only the sign bit set).  We change that into
			 * something the assmbler can handle. (BUG 462063).
			 */
			int scancnt;

			scancnt = sscanf(line, "%s %s %s %s", inst, arg1, arg2,
					 arg3);
			if ((scancnt == 2) && (!strcmp(inst, ".dword")) &&
			    (!strcmp(arg1, "-9223372036854775808"))) {
				printf("FIXING compiler bug 462063\n");
				fprintf(fpo, "\t.dword\t0x8000000000000000ll");
			} else
#endif /* COMPILER_FIXED */
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
