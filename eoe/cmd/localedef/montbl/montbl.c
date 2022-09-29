/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)localedef:montbl/montbl.c	1.2.1.1"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <limits.h>
#include <stddef.h>

#define MONSIZ	16
#define NUMSTR	8
#define NUMCHAR	8

extern char	*optarg;
extern int	optind;

#define OFFSET(s,m)     ( (ptrdiff_t) &(((s *) NULL )->m) )
#define SIZEOF(array)	( sizeof ( array ) / sizeof ( (array)[0] ) )

/* This is the structure to write out to LC_MONETARY.
   It contains 32-bit offsets rather than pointers to
   strings. */
struct 	lconvoff 	{
	__uint32_t decimal_point;
	__uint32_t thousands_sep;
	__uint32_t grouping;
	__uint32_t int_curr_symbol;
	__uint32_t currency_symbol;
	__uint32_t mon_decimal_point;
	__uint32_t mon_thousands_sep;
	__uint32_t mon_grouping;
	__uint32_t positive_sign;
	__uint32_t negative_sign;
	char int_frac_digits;
	char frac_digits;
	char p_cs_precedes;
	char p_sep_by_space;
	char n_cs_precedes;
	char n_sep_by_space;
	char p_sign_posn;
	char n_sign_posn;
};

struct {
    char *	keyword;
    int		isstring;
    char *	val;
    ptrdiff_t	offset;
}
keys[] =
{
    /* Default values comes from the C locale */
    /* Warning: The decimal_point, thousands_sep and grouping members
       come from LC_NUMERIC */

    { "int_curr_symbol",   1, "",           OFFSET ( struct lconvoff, int_curr_symbol ) },
    { "currency_symbol",   1, "",           OFFSET ( struct lconvoff, currency_symbol ) },
    { "mon_decimal_point", 1, "",           OFFSET ( struct lconvoff, mon_decimal_point ) },
    { "mon_thousands_sep", 1, "",           OFFSET ( struct lconvoff, mon_thousands_sep ) },
    { "mon_grouping",	   1, "",           OFFSET ( struct lconvoff, mon_grouping ) },
    { "positive_sign",	   1, "",           OFFSET ( struct lconvoff, positive_sign ) },
    { "negative_sign",	   1, "",           OFFSET ( struct lconvoff, negative_sign ) },
    { "int_frac_digits",   0, (char *) 255, OFFSET ( struct lconvoff, int_frac_digits ) },
    { "frac_digits",	   0, (char *) 255, OFFSET ( struct lconvoff, frac_digits ) },
    { "p_cs_precedes",	   0, (char *) 255, OFFSET ( struct lconvoff, p_cs_precedes ) },
    { "p_sep_by_space",	   0, (char *) 255, OFFSET ( struct lconvoff, p_sep_by_space ) },
    { "n_cs_precedes",	   0, (char *) 255, OFFSET ( struct lconvoff, n_cs_precedes ) },
    { "n_sep_by_space",	   0, (char *) 255, OFFSET ( struct lconvoff, n_sep_by_space ) },
    { "p_sign_posn",	   0, (char *) 255, OFFSET ( struct lconvoff, p_sign_posn ) },
    { "n_sign_posn",	   0, (char *) 255, OFFSET ( struct lconvoff, n_sign_posn ) },
};

static char * get_strval ( char * from );
static int get_intval ( char * from, int * to );
static int process_line ( char * line, struct lconvoff * mon, int lineno );

main(argc, argv)
int argc;
char **argv;
{
	FILE	*infile, *outfile;
	char	line[BUFSIZ];
	int	i;
	int	lineno;
	int	offset;
	int	c;
	int	errflag = 0;
	char	*outname = NULL;
	struct lconvoff	mon;

	/* Process command line */
	while ((c = getopt(argc, argv, "o:")) != -1) {
		switch(c) {
		case 'o':
			outname = optarg;
			break;
		case '?':
			errflag++;
		}
	}
	if (errflag || optind != argc - 1) {
		fprintf(stderr, "usage: montbl [-o outfile] infile\n");
		exit(1);
	}
	if (outname == NULL)
		outname = "LC_MONETARY";
	if ((infile = fopen(argv[optind], "r")) == NULL) {
		fprintf(stderr, "Cannot open input: %s\n", argv[1]);
		exit(1);
	}

	/* Process input file */
	i = 0;
	lineno = 0;
	while (fgets(line, BUFSIZ, infile) != NULL) {
		lineno++;
		if (strlen(line) == BUFSIZ-1 && line[BUFSIZ-1] != '\n') {
			fprintf(stderr,"line %d: line too long\n", lineno);
			exit(1);
		}
		if (line[0] == '#')	/* comment */
			continue;
		if ( ! process_line ( line, &mon, lineno ) )
		    exit ( 1 );
	}
	fclose ( infile );
	
	/* Initialize the lconvoff structure for output */
	i = 0;
	offset = 1;

	/* These two fields come from LC_NUMERIC */
	mon.decimal_point = NULL;
	mon.thousands_sep = NULL;
	mon.grouping = NULL;

	for ( i = 0; i < SIZEOF ( keys ) ; i ++ )
	{
	    if ( keys[i].isstring )
	    {
		* (char**) ( (ptrdiff_t)&mon + keys[i].offset ) = (char *)offset;
		offset += strlen ( keys[i].val ) + 1;
	    }
	    else
		* (char *) ( (ptrdiff_t)&mon + keys[i].offset ) = (char)((int)(keys[i].val));
	}

	/* Sanity checks */
	if (mon.p_cs_precedes > 1 && mon.p_cs_precedes < CHAR_MAX)
		fprintf(stderr, "Bad value for p_cs_precedes\n");
	if (mon.p_sep_by_space > 1 && mon.p_sep_by_space < CHAR_MAX)
		fprintf(stderr, "Bad value for p_sep_by_space\n");
	if (mon.n_cs_precedes > 1 && mon.n_cs_precedes < CHAR_MAX)
		fprintf(stderr, "Bad value for n_cs_precedes\n");
	if (mon.n_sep_by_space > 1 && mon.n_sep_by_space < CHAR_MAX)
		fprintf(stderr, "Bad value for n_sep_by_space\n");
	if (mon.p_sign_posn > 4 && mon.p_sign_posn < CHAR_MAX)
		fprintf(stderr, "Bad value for p_sign_posn\n");
	if (mon.n_sign_posn > 4 && mon.n_sign_posn < CHAR_MAX)
		fprintf(stderr, "Bad value for n_sign_posn\n");

	/* Write out data file */
	if ((outfile = fopen(outname, "w")) == NULL) {
		fprintf(stderr, "Cannot open output file\n");
		exit(1);
	}
	if (fwrite(&mon, sizeof(struct lconvoff), 1, outfile) != 1) {
		fprintf(stderr, "Cannot write to output file, %s\n", outname);
		exit(1);
	}

	/* Write strings */
	fwrite ( "", 1, 1, outfile );
	for ( i = 0; i < SIZEOF ( keys ) && keys[i].isstring; i ++ )
	    if (fwrite( keys[i].val, strlen(keys[i].val) +1, 1, outfile) != 1)
	    { 
		fprintf(stderr, "Cannot write to output file, %s\n", outname);
		exit(1);
	    }
	exit(0);
}

char *
get_strval ( char * ptr )
{
    int		j = 0;
    char	buf[BUFSIZ];
    char *	nptr;

    if ( *ptr++ != '"' )
	return 0;
    
    while ( *ptr != '\n' && *ptr != '\0' && *ptr != '"' ) {
	if (*ptr == '\\') {
	    switch (ptr[1]) {
	    case '"': buf[j++] = '"'; ptr++; break;
	    case 'n': buf[j++] = '\n'; ptr++; break;
	    case 't': buf[j++] = '\t'; ptr++; break;
	    case 'f': buf[j++] = '\f'; ptr++; break;
	    case 'r': buf[j++] = '\r'; ptr++; break;
	    case 'b': buf[j++] = '\b'; ptr++; break;
	    case 'v': buf[j++] = '\v'; ptr++; break;
	    case 'a': buf[j++] = '\7'; ptr++; break;
	    case '\\': buf[j++] = '\\'; ptr++; break;
	    default:
		if (ptr[1] == 'x') {
		    ptr += 2;
		    buf[j++] = strtol(ptr, &nptr, 16);
		    if (nptr != ptr) {
			ptr = nptr;
			continue;
		    } else
			buf[j-1] = 'x';
		} else if (isdigit(ptr[1])) {
		    ptr++;
		    buf[j++] = strtol(ptr, &nptr, 8);
		    if (nptr != ptr) {
			ptr = nptr;
			continue;
		    } else
			buf[j-1] = *ptr;
		} else
		    buf[j++] = ptr[1];
	    }
	} else
	    buf[j++] = *ptr;
	ptr++;
    }
    if ( *ptr != '"' )
	return 0;

    buf[j] = '\0';

    if ( (ptr = strdup(buf)) == NULL)
    {
	fprintf(stderr, "Out of space\n");
	exit(1);
    }
    return ptr;
}

int
get_intval ( char * from, int * to )
{
    char * end;
    *to = strtol ( from, &end, 0 );
    if ( from == end || *end != '\n' || *to > CHAR_MAX )
	return 0;
    else
	return 1;
}

int
process_line ( char * line, struct lconvoff * mon, int lineno )
{
    char * c, * keyword, *value;
    int	intval;
    int i;

    c = line;
    while ( *c && isspace ( *c ) ) c++;	/* Skip initial spaces */
    if ( ! *c )
	/* Empty line: ignore it */
	return 1;

    keyword = c;

    while ( *c && !isspace ( *c ) ) c++; /* Go to next space */
    if ( ! *c )
    {
	fprintf ( stderr, "line %d: no value\n", lineno );
	return 0;
    }
    *c++ = '\0';
    while ( *c && isspace ( *c ) ) c++;	/* Skip spaces */
    if ( ! *c )
    {
	fprintf ( stderr, "line %d: no value\n", lineno );
	return 0;
    }
    value = c;

    for ( i = 0; i < SIZEOF( keys ); i ++ )
    {
	if ( ! strcmp ( keys[i].keyword, keyword ) )
	{
	    if ( keys[i].isstring )
		if ( value = get_strval ( value ) )
		    keys[i].val = value;
		else
		{
		    fprintf ( stderr, "line %d: not a string value\n", lineno );
		    return 0;
		}
	    else
		if ( get_intval ( value, & intval ) )
		    keys[i].val = (char *) intval;
		else
		{
		    fprintf ( stderr, "line %d: not an integer value\n", lineno );
		    return 0;
		}
	    return 1;
	}
    }
    fprintf ( stderr, "line %d: unrecognized keyword '%s'\n", lineno, keyword );
    return 0;
}
