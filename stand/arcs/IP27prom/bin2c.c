#include <stdlib.h>
#include <stdio.h>

/*
 * bin2c				by Curt McDowell, 12-11-92, 05-01-96
 *
 *	Usage: bin2c [-s] [-w maxwidth] [-i indent]
 *		     array-name < datafile > c-file
 *
 *	Takes a binary file on stdin and turns it into a compilable C file.
 *	The C file contains an array of unsigned char with the binary data.
 *	Binary data may appear as a list of comma-separated decimal numbers
 *	or as an ANSI-C split string.  The latter form may compile up to 98%
 *	faster depending on the compiler.
 *
 *	The name of the array is specified on the command line.
 *
 *	Optionally, an integer value is also defined, named using the array
 *	name postpended with "_len", and statically initialized to contain
 *	the length of the input data in bytes.
 */

int	lenvar		= 0;
int	maxwidth	= 79;
int	indent		= 8;
int	string		= 0;

char   *array;
int	count;

char   *buf;
char   *scol;
char   *col;
char   *colend;

void usage(void)
{
	fprintf(stderr,
		"Usage: bin2c [-s] [-i indent] [-w maxwidth]\n");
	fprintf(stderr,
		"             [-c] arrayname\n");
	fprintf(stderr,
		"    -s            Output a quoted string instead of array\n");
	fprintf(stderr,
		"    -i indent     Indent at beginning of lines\n");
	fprintf(stderr,
		"    -w maxwidth   Maximum width of output lines\n");
	fprintf(stderr,
		"    -c            Also output a variable w/ data length\n");
	fprintf(stderr,
		"    Binary file is read from standard input.\n");
	fprintf(stderr,
		"    Resulting C module is written to standard output.\n");
	fprintf(stderr,
		"    Example:  bin2c mydata < mydata.bin > mydata.c\n");

	exit(1);
}

void format_string(void)
{
	char		strs[256][8], *s;
	int		c;

	/*
	 * Preformat all the characters for speed.
	 */

	for (c = 0; c < 256; c++) {
		s = strs[c];
		if (c == 8 || c == 9 || c == 10 || c == 13) {
			*s++ = '\\';
			*s++ = "btn  r"[c - 8];
		} else {
			if (c < 32 || c == 34 ||
			    c == '?' ||     /* Protect from ANSI trigraphs */
			    c == '\\' || c > 126)
				*s++ = '\\';

			if (c < 32 || c > 126) {
				*s++ = '0' + c / 64;
				*s++ = '0' + c % 64 / 8;
				*s++ = '0' + c % 8;
			} else
				*s++ = c;
		}
		*s = 0;
	}

	printf("unsigned char %s[] =\n", array);

	for (c = indent, scol = buf; c > 7; c -= 8)
		*scol++ = '\t';

	while (c-- > 0)
		*scol++ = ' ';

	*scol++ = 34;

	/*
	 * The extra 7 characters is room for worst case: quotes,
	 * backslashed octal constant, and semicolon due to EOF.
	 */

	col	= scol;
	colend	= scol + maxwidth - indent - 7;

	while ((c = getchar()) != EOF) {
		char	       *s;

		if (col > colend) {
			*col++ = 34;
			*col = 0;
			puts(buf);
			col = scol;
		}

		for (s = strs[c]; *s; )
			*col++ = *s++;

		count++;
	}

	*col++ = 34;
	*col++ = ';';
	*col = 0;
	puts(buf);
}

void format_array(void)
{
	char		strs[256][8];
	int		c;

	/*
	 * Preformat all the integers for speed.
	 */

	for (c = 0; c < 256; c++)
		sprintf(strs[c], "%d,", c);

	printf("unsigned char %s[] = {\n", array);

	for (c = indent, scol = buf; c > 7; c -= 8)
		*scol++ = '\t';

	while (c-- > 0)
		*scol++ = ' ';

	/*
	 * The extra 4 characters is room for worst case:
	 * three-digit number and comma.
	 */

	col	= scol;
	colend	= scol + maxwidth - indent - 4;

	while ((c = getchar()) != EOF) {
		char	       *s;

		for (s = strs[c]; *s; )
			*col++ = *s++;

		if (col > colend) {
			*col = 0;
			puts(buf);
			col = scol;
		}

		count++;
	}

	if (col > scol) {
		*col = 0;
		puts(buf);
	}

	puts("};");
}

void main(int argc, char **argv)
{
	int		c;
	extern	char   *optarg;
	extern	int	optind;

	while ((c = getopt(argc, argv, "w:i:cs")) >= 0)
		switch (c) {
		case 'i':
			indent		= atoi(optarg);
			break;
		case 'w':
			maxwidth	= atoi(optarg);
			break;
		case 'c':
			lenvar		= 1;
			break;
		case 's':
			string = 1;
			break;
		default:
			usage();
		}

	if (optind != argc - 1)
		usage();

	array	= argv[optind];

	if (indent < 0 || indent > 999 || maxwidth < 4) {
		fprintf(stderr,
			"bin2c: Illegal value for option.\n");
		exit(1);
	}

	if ((buf = malloc(indent + maxwidth + 10)) == 0) {
		fprintf(stderr,
			"bin2c: Not enough memory.\n");
		exit(1);
	}

	if (string)
		format_string();
	else
		format_array();

	if (lenvar)
		printf("\nint %s_len = %d;\n", array, count);

	exit(0);
}
