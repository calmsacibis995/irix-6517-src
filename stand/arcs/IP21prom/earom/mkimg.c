/***********************************************************************\
*	File:		mkimg.c						*
*	Author:		jfk						*
* 									*
*	Takes an EAROM data file and produces an image file suitable	*
*	for use as input to the nprom EAROM burner program.		*
*									*
*									*
\***********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <strings.h>
#include <getopt.h>
#include <sys/EVEREST/addrs.h>

#define MAX_IMG_SIZE	1024
#define NUM_COPIES	2
#define MAX_EAROM_SIZE	MAX_IMG_SIZE * NUM_COPIES
#define NAN 		-2
#define CHECKSUM	-3

/*
 * Global Variable Definitions
 */
u_char 	EAROM[MAX_EAROM_SIZE];	/* Array containing values for EAROM */ 
int     LineCount = 1;		/* Global count of lines, used for reporting
				   errors */

/*
 * Skips all non-terminal white space.  It doesn't skip comments, 
 * because we don't want to allow putting comments between the offset
 * and value tokens.  If the stop_at_newline flag is set, the routine
 * stops at nl chars instead of treating them as space, since we need
 * those to find the end of the values field on the line.
 */

int
skip_spaces(FILE *file, int stop_at_newline)
{
    int ch;			/* Character just read */

    for (;;) {
    	 
	if ((ch = getc(file)) == EOF) 
	    return EOF;	

	if ((stop_at_newline && ch == '\n') || !isspace(ch)) {
	    ungetc(ch, file);
	    return 0;
	}
    }
}


/*
 * Skip over white-space and comments.  Stop as soon as a non-whitespace
 * character outside of a comment is found.
 */

int
skip_comments(FILE *file)
{
    int ch;

    for (;;) {
	if (skip_spaces(file, 0) == EOF) 
	    return EOF;

	ch = getc(file);

	if (ch == '#') {
	    for (;;) {
		if ((ch = getc(file)) == EOF)
		    return EOF;
	
		if (ch == '\n') {
		    LineCount++; 
		    break;
		}
	    }
	} else {
	    ungetc(ch, file);
	    return 0;
	}
    }
}


/*
 * Parse a number.  We determine whether a value is a number by
 * examining the first character.  If the character is a number
 * or an "'", we go ahead and scan the rest of the characters until
 * whitespace is encountered or we hit EOF.  We then parse the
 * scanned string according to the first character.
 */

int
parse_number(FILE *file, u_long *value)
{
    int  ch;
    char buf[20];
    char *rem;
    int  i = 1;

    /*
     * Scan in the string corresponding to the number
     */
    ch = getc(file);
    if (isdigit(ch) || ch == '\'' || ch == '%' || ch == '*') {
	buf[0] = ch;
        for (;;) {
	    if ((ch = getc(file)) == EOF)
		return EOF;
	    else if (strchr("01234567890abcdefABCDEFxX%'", ch) == NULL) {
		ungetc(ch, file);
		break;
	    } else 
		buf[i++] = ch;
	}
	buf[i] = 0;	/* Terminate the string */
    } else {
	ungetc(ch, file);
	return NAN;
    }

    /*
     * Check and process the number.
     */
    if (isdigit(buf[0])) {
	*value = strtoul(buf, &rem, 0);
	if (*value == 0 && strlen(rem) > 0)
	    return NAN;
    } else if (buf[0] == '\'') {
	*value = buf[1];
    } else if (buf[0] == '%') {
        *value = strtoul(buf+1, &rem, 2);
	if (*value == 0 && strlen(rem) > 0) 
	    return NAN;
    } else if (buf[0] == '*') {
	*value = strtoul(buf+1, &rem, 0);
	if (*value == 0 && strlen(rem) > 0)
	    return NAN;
	 else
	    return CHECKSUM;
    } else {
	return NAN;
    }
    
    return 0;
}

/*
 * Calculate the checksum of this EAROM image from byte 0 to byte checklen - 1
 * and store it in the two bytes starting at address.  Next, store the
 * complement of the checksum in the next two bytes.
 */
void
store_checksum(u_char *EAROM, uint checklen, uint address)
{
    int i;
    uint sum = 0;
    
    for (i = 0; i < checklen; i++) {
	sum += EAROM[i];
    }

    EAROM[address] = sum & 0xff;		/* Store checksum LSB */
    EAROM[address + 1] = (sum >> 8) & 0xff;	/* Store checksum MSB */

    EAROM[address + 2] = (sum & 0xff) ^ 0xff;	/* Store ~checksum LSB */
    EAROM[address + 3] = ((sum >> 8) & 0xff) ^ 0xff; /*  ~checksum MSB */

    printf("\tStored EAROM checksum is 0x%x\n\n", (sum & 0xffff));
}



/*
 * Parse a line.  A line is made up of an address field followed by a 
 * ':', followed by 1 or more values.  After each value found the 
 * address is incremented, allowing us to enter multiple values without
 * having to specify a new address each time.
 */

int
parse_line(FILE *file)
{
    u_long 	address;
    u_long	value;
    int		retval;
    int		ch;
    int		found_one;

    if ((retval = parse_number(file, &address)) == EOF) {
	fprintf(stderr, "Error: unexpected EOF at line %d\n", LineCount);
	return EOF;
    }

    if ((retval == NAN) || (retval == CHECKSUM)) {
	fprintf(stderr, "Error: Bogus address at line %d\n", LineCount);
	return EOF;
    }

    if (skip_spaces(file, 1) == EOF) {
	fprintf(stderr, "Error: unexpected EOF in middle of expr at line %d\n",
		LineCount);
	return EOF;
    }

    ch = getc(file);
    if (ch != ':') {
	fprintf(stderr, "Error: missing ':' at line %d\n", LineCount);
	return EOF;
    }

    /*
     * Scan for a sequence of numbers
     */

    found_one = 0;
    for (;;) {
	if (skip_spaces(file, 1) == EOF) {
	    fprintf(stderr, "Error: unexpected EOF in values on line %d\n",
		    LineCount);
	    return EOF;
	}

	if ((retval = parse_number(file, &value)) == EOF) {
	    fprintf(stderr, "Error: unexpected EOF at end of line %d\n", 
		    LineCount);
	    return EOF;
	}

	if (retval == NAN) {
	    if (! found_one)
		fprintf(stderr, "Warning: No values found on line %d\n",
			LineCount);
	       
	    break;
	} else if (retval == CHECKSUM) {
	    found_one = 1;
	    store_checksum(EAROM, value, address);
	    address += 4;
	} else {
	    found_one = 1;
	    EAROM[address++] = (u_char) value;
	}
    }

    /*
     * When we get here, we encountered an non-number on the line.
     * If it isn't a '#' or a newline, complain.
     */
    
    ch = getc(file);
    switch (ch) {
      case '\n' :
	LineCount++;
	return 0;

      case '#' :
	ungetc(ch, file);
	return 0;

      default :
	fprintf(stderr, "Error: unexpected character '%c' found on line %d\n",
		ch, LineCount);
	return EOF;
    }
}


/*
 * Parse the entire file. 
 */

void
parse_file(FILE *file)
{
    for (;;) {
	if (skip_comments(file) == EOF)
	    break;
	
        if (parse_line(file) == EOF) 
	    break;
    }
}


/*
 * Fix the funky IP19 EAROM problem.
 */

unsigned int
remap(unsigned int value, int map[])
{
    int i;
    unsigned int result = 0;

    for (i = 0; i < 11; i++) {
	result |= (value & 1) << map[i];
	value >>= 1;
    }
    return result;
}

void
do_swap_bits()
{
    u_char new[MAX_EAROM_SIZE];
    int i;
    unsigned int newaddr;
    static int dmap[] = { 0, 2, 1, 3, 5, 6, 7, 4, 0, 0, 0 };
    static int amap[] = { 2, 5, 0, 3, 1, 6, 10, 4, 8, 9, 7 };

    /* Zero the new array */
    for (i = 0; i < MAX_EAROM_SIZE; i++)
        new[i] = 0;

    for (i = 0; i < MAX_EAROM_SIZE; i++) 
	new[i] = remap(EAROM[i], dmap);
   
    /* Reposition the data bytes to account for the address flipping */
    for (i = 0; i < MAX_EAROM_SIZE; i++) {
	newaddr = remap(i, amap);
	if (newaddr >= MAX_EAROM_SIZE)
	    printf("Generated address > %d, i = %d\n", MAX_EAROM_SIZE, i);
	else
	    EAROM[newaddr] = new[i];
    }
}

/*
 * Main Program.  Checks the arguments and opens the file
 * appropriately.
 */

void 
main(int argc, char **argv)
{
    FILE *in  = stdin;
    FILE *out = stdout;
    int found_input = 0;	/* Flag indicating input file was found */
    int swap_bits = 0;		/* Flag indicating whether we need to 
				   swap the bits all around to deal with
				   with the funky EAROM on the IP19 */
    int c, i, j;

    if (argc > 6) {
	fprintf(stderr, "Usage: %s [-r] [-o output_file] [input_file]\n");
	exit(1);
    }

    while ((c = getopt(argc, argv, "ro:i:")) != -1) {
	switch (c) {
	  case 'r':
	    swap_bits = 1;
	    break;

	  case 'o':
	    if ((out = fopen(optarg, "w")) == NULL) {
	        fprintf(stderr, "%s: could not open output file %s ",
		        argv[0], optarg);
		perror(NULL);
		exit(1);
	    }
	    break;

	  case 'i':
	    if ((in = fopen(optarg, "r")) == NULL) {
		fprintf(stderr, "%s: could not open input file %s ", 
			argv[0], optarg);
		perror(NULL);
		exit(1);
	    }
	    break;

	  default:
	    fprintf(stderr, "Usage: %s [-i infile] [-o outfile] [-r]\n", 
		    argv[0]);
	    break;
	}
    }

    if (optind < argc) {
        fprintf(stderr, "Found bogus arguments. Feh\n");
	fprintf(stderr, "Usage: %s [-i infile] [-o outfile] [-r]\n", argv[0]);
	exit(1);
    }

    /* Zero out the EAROM image. */
    for (i = 0; i < MAX_IMG_SIZE; i++)
	EAROM[i] = 0;

    parse_file(in);

    /*
     * Make NUM_COPIES - 1 duplicates of EAROM in the EAROM array
     */
    for (i = 1; i < NUM_COPIES; i++)
	for (j = 0; j < MAX_IMG_SIZE; j++)
	    EAROM[i * MAX_IMG_SIZE + j] = EAROM[j];

    /* Flip the bits in all of the data words */
    /*
     * Write out the image file
     */
    if (swap_bits)
       do_swap_bits();

    if (fwrite(EAROM, sizeof(char), MAX_EAROM_SIZE, out) != MAX_EAROM_SIZE) {
	perror("Could not write output file");

	if (out != stdout) {
	    unlink(argv[2]);
	    close(out);
	}

	exit(1);
    }

    exit(0);
}
