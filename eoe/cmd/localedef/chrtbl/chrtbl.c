/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)localedef:chrtbl/chrtbl.c	1.1.6.1"
/*
 * 	chrtbl - generate character class definition table
 */
#include <stdio.h>
#include <ctype.h>
#include <varargs.h>
#include <string.h>
#include <signal.h>

/*	Definitions	*/

#define HEX    			1
#define OCTAL  			2
#define RANGE  			1
#define UL_CONV 		2
#define SIZE			(2 * 257) + 7 + 2
#define START_UL		257
#define START_CSWIDTH		(2 * 257)
#define START_NUMERIC		(2 * 257) + 7
#define	ISUPPER			1
#define ISLOWER			2
#define ISDIGIT			4 
#define ISSPACE			8	
#define ISPUNCT			16
#define ISCNTRL			32
#define ISBLANK_OLD		64 /* Obsolete */
#define ISXDIGIT		128
#define ISALPHA			0x00004000
#define ISPRINT			0x00008000
#define ISGRAPH			0x40000000
#define ISBLANK		    	0x80000000
#define UL			255
#define LC_CTYPE		257
#define CSWIDTH 		258
#define	NUMERIC			259
#define	LC_NUMERIC		260
#define DECIMAL_POINT		261
#define THOUSANDS_SEP		262
#define GROUPING		263

#define OLD_FORMAT              0
#define NEW_FORMAT              1

#define UNDEFINED_CSWIDTH	10

#define CTYPE_MAGIC_NB		0x77229966

extern	char	*malloc();
extern  void	perror();
extern	void	exit();
extern	int	unlink();
extern	int	atoi();

/*   Internal functions  */

static	void	error();
static	void	init();
static	void	process();
static	void	check_posix();
static	void	create1();
static	void	create2();
static	void	create3();
static  void	parse();
static	int	empty_line();
static	void	check_chrclass();
static	void	clean();
static  void    n_comment1();
static	void	comment1();
static	void	comment2();
static	void	comment3();
static 	int	cswidth();
static 	int	setnumeric();
static	void	error();
static	int	check_digit();
static	unsigned  char	*clean_line();
static	char	*getstr();
static int 	newkey();
static void     print_new_ultable();

/*	Static variables	*/

struct  classname	{
	char	*name;
	int	num;
	char	*repres;
        char    *posix_members;
}  cln[]  =  {
	"isupper",		ISUPPER,		"_U",	"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
	"islower",		ISLOWER,		"_L",	"abcdefghijklmnopqrstuvwxyz",
	"isdigit",		ISDIGIT,		"_N",	"0123456789",
	"isspace",		ISSPACE,		"_S",	" \f\n\r\t\v",
	"ispunct",		ISPUNCT,		"_P",	NULL,
	"iscntrl",		ISCNTRL,		"_C",	NULL,
	"isblank",		ISBLANK,		"_BL",	" \t",
	"isxdigit",		ISXDIGIT,		"_X",	"0123456789ABCDEFabcdef",
	"isalpha",		ISALPHA,		"_A",	NULL,
	"isprint",		ISPRINT,		"_PR",	" ",
	"isgraph",		ISGRAPH,		"_GR",	NULL,
	"ul",			UL,			NULL,	NULL,
	"LC_CTYPE",		LC_CTYPE,		NULL,	NULL,
	"cswidth",		CSWIDTH,		NULL,	NULL,
	"LC_NUMERIC",		LC_NUMERIC,		NULL,	NULL,
	"decimal_point",	DECIMAL_POINT,		NULL,	NULL,
	"thousands_sep", 	THOUSANDS_SEP,		NULL,	NULL,
	"grouping",		GROUPING,		NULL,	NULL,
	NULL,			NULL,			NULL,	NULL
};

int	readstd;			/* Process the standard input */
unsigned char	linebuf[256];		/* Current line in input file */
unsigned char *p;
int	chrclass = 0;			/* set if LC_CTYPE is specified */
int	lc_numeric;			/* set if LC_NUMERIC is specified */
int	lc_ctype;
char	chrclass_name[20];		/* save current chrclass name */
int	chrclass_num;			/* save current chrclass number */
int     o_flag =1;                      /* by default, chrtbl generates the
				           the old and new format 32-bit n_ctype table.
				           specify -o to have it generated 
					    just the new one */
int	warn_posix = 1;			/* Print out POSIX warnings if a character class
					   does not include all required characters */

int	ul_conv = 0;			/* set when left angle bracket
					 * is encountered. 
					 * cleared when right angle bracket
					 * is encountered */
int	cont = 0;			/* set if the description continues
					 * on another line */
int	action = 0;			/*  action = RANGE when the range
					 * character '-' is ncountered.
					 *  action = UL_CONV when it creates
					 * the conversion tables.  */
int	in_range = 0;			/* the first number is found 
					 * make sure that the lower limit
					 * is set  */
int	ctype[SIZE];			/* character class and character
					 * conversion table */
int	range = 0;			/* set when the range character '-'
					 * follows the string */
int	width;				/* set when cswidth is specified */
int	numeric;			/* set when numeric is specified */
char	tablename1[24];			/* save name of the first data file */
char	tablename2[24];			/* save name of the second date file */
char	*cmdname;			/* Save command name */
char	input[256];			/* Save input file name */
char	tokens[] = ",:\0";

char	isblank_old_char = ' ';		/* Old character defined by isblank */

/* added for grouping */
char	*grouping_infop = (char *)NULL;	/* grouping information */

/* Error  messages */
/* vprintf() is used to print the error messages */

char	*msg[] = {
/*    0    */ "Usage: chrtbl [-n] [-s] [-r old space char ][ - | file ] ",
/*    1    */ "the name of the output file for \"LC_CTYPE\" is not specified",
/*    2    */ "incorrect character class name \"%s\"",
/*    3    */ "--- %s --- left angle bracket  \"<\" is missing",
/*    4    */ "--- %s --- right angle bracket  \">\" is missing",
/*    5    */ "--- %s --- wrong input specification \"%s\"",
/*    6    */ "--- %s --- number out of range \"%s\"",
/*    7    */ "--- %s --- nonblank character after (\"\\\") on the same line",
/*    8    */ "--- %s --- wrong upper limit \"%s\"",
/*    9    */ "--- %s --- wrong character \"%c\"",
/*   10    */ "--- %s --- number expected",
/*   11    */ "--- %s --- too many range \"-\" characters",
/*   12    */ "--- %s --- wrong specification, %s",
/*   13    */ "malloc error",
/*   14	   */ "--- %s --- wrong specification",
/*   15    */ "the name of the output file for \"LC_NUMERIC\" is not specified",
/*   16    */ "numeric editing information \"numeric\" must be specified",
/*   17    */ "character classification and conversion information must be specified",
/*   18    */ "the same output file name was used in \"LC_CTYPE\" and \"LC_NUMERIC\"",
/*   19    */ "Warning: locale is not POSIX-compliant because 0x%x is not in %s\n"
};

static char cswidth_info[] = "\n\nCSWIDTH FORMAT: n1[[:s1][,n2[:s2][,n3[:s3]]]]\n\tn1 byte width (SUP1)\n\tn2 byte width (SUP2)\n\tn3 byte width (SUP3)\n\ts1 screen width (SUP1)\n\ts2 screen width (SUP2)\n\ts3 screen width (SUP3)";


main(argc, argv)
int	argc;
char	**argv;
{
	int opt;
	extern int optind;

	p = linebuf;
        if (cmdname = strrchr(*argv, '/'))
                ++cmdname;
        else
                cmdname = *argv;

	while (( opt = getopt(argc, argv, "nsr:")) != -1)
	   switch (opt) {
		case 'n':
			/* only new format table will be produced */
			o_flag = 0;
			break;
		case 's':
			/* no posix warning, silent mode */
			warn_posix = 0;
			break;
		case 'r':
			/* to specify isblank_old_char */
			isblank_old_char= *optarg;
			break;
		default:
			error(cmdname, msg[0]);
			exit(1);
			}

	
	if ( optind == argc || ((optind == argc-1) && (strcmp(argv[argc-1], "-") == 0))){
		readstd++;
                (void)strcpy(input, "standard input");
	        }
	else
		(void)strcpy(input, argv[argc-1]);

	if (signal(SIGINT, SIG_IGN) == SIG_DFL)
		(void)signal(SIGINT, clean);
	if (!readstd && freopen(argv[argc-1], "r", stdin) == NULL) {
		perror(argv[argc-1]); exit(1);
		}
        init();
	if (o_flag)
	  process(OLD_FORMAT);
	else 
	 process(NEW_FORMAT);
	if (!lc_ctype && chrclass)
		error(input, msg[17]);
	if (lc_ctype) {
	        if ( warn_posix )
		    check_posix();

		if (!chrclass) 
			error(input, msg[1]);
		else {
		        if (o_flag) {
			  create1(OLD_FORMAT);
			  /* init();
			  process(NEW_FORMAT);  */
			  create1(NEW_FORMAT); 
			  create2(OLD_FORMAT);
			  create2(NEW_FORMAT);
			}
			else {
			  create1(NEW_FORMAT);
			  create2(NEW_FORMAT);
			}
		}
	}
	if (lc_numeric && !numeric)
		error(input, msg[16]);
	if (numeric && !lc_numeric)
		error(input, msg[15]);
	if (strcmp(tablename1, tablename2) == NULL)	
		error(input, msg[18]);
	if (lc_numeric && numeric)
		create3();
	exit(0);
}


/* Initialize the ctype array */

static void
init()
{
        register i;
 	for(i=0; i<256; i++) 
		(ctype + 1)[257 + i] = i;
	ctype[START_CSWIDTH] = ctype[START_CSWIDTH + 3] = ctype[START_CSWIDTH + 6] = 1;
}

/* Read line from the input file and check for correct
 * character class name */

static void
process(format)
int format;
{

	unsigned char	*token();
	register  struct  classname  *cnp;
	register unsigned char *c;
	for (;;) {
		if (fgets((char *)linebuf, sizeof linebuf, stdin) == NULL ) {
			if (ferror(stdin)) {
				perror("chrtbl (stdin)");
				exit(1);	
				}
				break;	
	        }
		p = linebuf;
		/*  comment line   */
		if ( *p == '#' ) 
			continue; 
		/*  empty line  */
		if ( empty_line() )  
			continue; 
		if ( ! cont ) 
			{
			c = token();
			for (cnp = cln; cnp->name != NULL; cnp++) 
				if(strcmp(cnp->name, (char *)c) == NULL) 
					break; 
			}	
		switch(cnp->num) {
		default:
		case NULL:
			error(input, msg[2], c);
		case ISUPPER:
		case ISLOWER:
		case ISDIGIT:
		case ISXDIGIT:
		case ISSPACE:
		case ISPUNCT:
		case ISCNTRL:
		case ISBLANK:
		case ISALPHA:
		case ISPRINT:
		case ISGRAPH:
		case UL:
				lc_ctype++;
				(void)strcpy(chrclass_name, cnp->name);
				chrclass_num = cnp->num;
				parse(cnp->num);
				break;

		case LC_CTYPE:
				chrclass++;
				if ( (c = token()) == NULL )
					error(input, msg[1]);
				(void)strcpy(tablename1, "\0");
				(void)strcpy(tablename1, (char *)c);
				if (freopen("ctype.c", "w", stdout) == NULL)
					perror("ctype.c"), exit(1);
				break;
		case LC_NUMERIC:
			      if ((o_flag && format == OLD_FORMAT)|| 
			        (!o_flag )){
				lc_numeric++;
				if ( (c = token()) == NULL )
					error(input, msg[15]);
				(void)strcpy(tablename2, "\0");
				(void)strcpy(tablename2, (char *)c);
				ctype[START_NUMERIC] = (int)'.';
				}
				break;
	
		case CSWIDTH:
				width++;
				(void)strcpy(chrclass_name, cnp->name);
				if (! cswidth() )
					error(input, msg[12], chrclass_name, cswidth_info);
				break;
		case DECIMAL_POINT:
		case THOUSANDS_SEP:
				numeric++;
				(void)strcpy(chrclass_name, cnp->name);
				if (! setnumeric(cnp->num) )
					error(input, msg[14], chrclass_name);
				break;
		case GROUPING:
				(void)strcpy(chrclass_name, cnp->name);
				p = clean_line(p);
				if (strlen((char *)p) == 0)
					break;
				grouping_infop = getstr(p);
				break;
		}
	} /* for loop */

	return;
}

/* Checks if all required characters are in the various classes */
void check_posix()
{
    struct classname * cl;
    char * c;

    for ( cl = cln; cl->name != NULL; cl ++ )
    {
	if ( cl->posix_members == NULL )
	    continue;

	for ( c = cl->posix_members; *c != NULL; c ++ )
	    if ( ! ( ( ctype + 1 )[ *c ] & cl->num ) )
		fprintf ( stderr, msg[19], *c, cl->name );
    }
}


static int
empty_line()
{
	register unsigned char *cp;
	cp = p;
	for (;;) {
		if ( (*cp == ' ') || (*cp == '\t') ) {
				cp++;
				continue; }
		if ( (*cp == '\n') || (*cp == '\0') )
				return(1); 
		else
				return(0);
	}
}

/* 
 * token() performs the parsing of the input line. It is sensitive
 * to space characters and returns a character pointer to the
 * function it was called from. The character string itself
 * is terminated by the NULL character.
 */ 

unsigned char *
token()
{
	register  unsigned char  *cp;
	for (;;) {
	check_chrclass(p);
	switch(*p) {
		case '\0':
		case '\n':
			in_range = 0;
			cont = 0;
			return(NULL);
		case ' ':
		case '\t':
			p++;
			continue;
		case '>':
			if (action == UL)
				error(input, msg[10], chrclass_name);
			ul_conv = 0;
			p++;
			continue;
		case '-':
			if (action == RANGE)
				error(input, msg[11], chrclass_name);
			action = RANGE;
			p++;
			continue;
		case '<':
			if (ul_conv)
				error(input, msg[4], chrclass_name);
			ul_conv++;
			p++;
			continue;
		default:
			cp = p;
			while(*p!=' ' && *p!='\t' && *p!='\n' && *p!='>' && *p!='-')  
				p++;   
			check_chrclass(p);
			if (*p == '>')
				ul_conv = 0;
			if (*p == '-')
				range++;
			*p++ = '\0';
			return(cp);

		}
	}
}


/* conv_num() is the function which converts a hexadecimal or octal
 * string to its equivalent integer represantation. Error checking
 * is performed in the following cases:
 *	1. The input is not in the form 0x<hex-number> or 0<octal-mumber>
 *	2. The input is not a hex or octal number.
 *	3. The number is out of range.
 * In all error cases a descriptive message is printed with the character
 * class and the hex or octal string.
 * The library routine sscanf() is used to perform the conversion.
 */


conv_num(s)
unsigned char	*s;
{
	unsigned char	*cp;
	int	i, j;
	int	num;
	cp = s;
	if ( *cp != '0' ) 
		error(input, msg[5], chrclass_name, s);
	if ( *++cp == 'x' )
		num = HEX;
	else
		num = OCTAL;
	switch (num) {
	case	HEX:
			cp++;
			for (j=0; cp[j] != '\0'; j++) 
				if ((cp[j] < '0' || cp[j] > '9') && (cp[j] < 'a' || cp[j] > 'f'))
					break;
				
				break;
	case   OCTAL:
			for (j=0; cp[j] != '\0'; j++)
				if (cp[j] < '0' || cp[j] > '7')
					break;
			break;
	default:
			error(input, msg[5], chrclass_name, s);
	}
	if ( num == HEX )  { 
		if (cp[j] != '\0' || sscanf((char *)s, "0x%x", &i) != 1)  
			error(input, msg[5], chrclass_name, s);
		if ( i > 0xff ) 
			error(input, msg[6], chrclass_name, s);
		else
			return(i);
	}
	if (cp[j] != '\0' || sscanf((char *)s, "0%o", &i) != 1) 
		error(input, msg[5], chrclass_name, s);
	if ( i > 0377 ) 
		error(input, msg[6], chrclass_name, s);
	else
		return(i);
/*NOTREACHED*/
}

/* parse() gets the next token and based on the character class
 * assigns a value to corresponding table entry.
 * It also handles ranges of consecutive numbers and initializes
 * the uper-to-lower and lower-to-upper conversion tables.
 */

static void
parse(type)
int type;
{
	unsigned char	*c;
	int	ch1 = 0;
	int	ch2;
	int 	lower = 0;
	int	upper;
	while ( (c = token()) != NULL) {
		if ( *c == '\\' ) {
			if ( ! empty_line()  || strlen((char *)c) != 1) 
				error(input, msg[7], chrclass_name);
			cont = 1;
			break;
			}
		switch(action) {
		case	RANGE:
			upper = conv_num(c);
			if ( (!in_range) || (in_range && range) ) 
				error(input, msg[8], chrclass_name, c);
			((ctype + 1)[upper]) |= type;
			if ( upper <= lower ) 
				error(input, msg[8], chrclass_name, c);
			while ( ++lower <= upper ) 
				((ctype + 1)[lower]) |= type;
			action = 0;
			range = 0;
			in_range = 0;
			break;
		case	UL_CONV:
			ch2 = conv_num(c);
			(ctype + 1)[ch1 + 257] = ch2;
			(ctype + 1)[ch2 + 257] = ch1;
			action = 0;
			break;   
		default:
			lower = ch1 = conv_num(c);
			in_range ++;
			if (type == UL) 
				if (ul_conv)
					{
					action = UL_CONV;
					break;
					}
				else
					error(input, msg[3], chrclass_name);
			else
				if (range)
					{
					action = RANGE;
					range = 0;
					}
				else
					;
			
			((ctype + 1)[lower]) |= type;
			break;
		}
	}
	if (action)
		error(input, msg[10], chrclass_name);
}

/* create1() produces a C source file based on the definitions
 * read from the input file. For example for the current ASCII
 * character classification where LC_CTYPE=ascii it produces a C source
 * file named ctype.c.
 */


static void
create1(int format)
{
	struct  field {
		unsigned char  ch[20];
	} out[8];
	unsigned char	*hextostr();
	unsigned char	outbuf[256];
	int	cond = 0;
	int	flag=0;
	int i, j, index1, index2;
	int	line_cnt = 0;
	register struct classname *cnp;
	int 	num;

	if ( format == NEW_FORMAT ) {
	  n_comment1();
	  (void)sprintf((char *)outbuf,"unsigned int\t_ctype_new[] =  { 0,");
	  (void)printf("%s\n",outbuf);
	}
	else {
	  comment1();
	  (void)sprintf((char *)outbuf,"unsigned char\t_ctype[] =  { 0,");
	  (void)printf("%s\n",outbuf);
	}
	
	index1 = 0;
	index2 = 3;
	while (flag <= 1) {
		for (i=0; i<=3; i++)
			(void)strcpy((char *)out[i].ch, "\0");
		for(i=index1; i<=index2; i++) {
			if ( ! ((ctype + 1)[i]) )  {
				(void)strcpy((char *)out[i - index1].ch, "0");
				continue; }
			num = (ctype + 1)[i];
			if (flag) {      
			        if ( format == NEW_FORMAT ) {
				 print_new_ultable();	
				 flag++;
				 break;
				}
				else {
				 (void)strcpy((char *)out[i - index1].ch, "0x");  
				 (void)strcat((char *)out[i - index1].ch, (char *)hextostr(num));
				 continue; }
				}
			while (num)  {
				for(cnp=cln;cnp->num != UL;cnp++) {
					if(!(num & cnp->num))  
						continue; 
					if ( (i == isblank_old_char) && 
						format == OLD_FORMAT )	
					if ( (strlen((char *)out[i - index1].ch))  == NULL)
						(void)strcat((char *)out[i - index1].ch, "_B");

					if ( (strlen((char *)out[i - index1].ch))  == NULL)  
						if ( newkey(format, cnp->num))
						  (void)strcat((char *)out[i - index1].ch,cnp->repres);
						else
						  (void)strcat((char *)out[i - index1].ch, "0");
					else if ( newkey(format, cnp->num)) {
						(void)strcat((char *)out[i - index1].ch,"|");
						(void)strcat((char *)out[i - index1].ch,
							cnp->repres); }

				num = num & ~cnp->num;  
					if (!num) 
						break; 
				}  /* end inner for */
			}  /* end while */
		} /* end outer for loop */
		 if ( flag < 2 )
		  (void)sprintf((char *)outbuf,"\t%s,\t%s,\t%s,\t%s,",
		 out[0].ch,out[1].ch,out[2].ch,out[3].ch);


		if ( ++line_cnt == 64 ) {
			line_cnt = 0;
			flag++; 
			cond = flag; }
		switch(cond) {
		case	1:
			(void)printf("%s\n", outbuf);
			comment2(format);
			index1++;
			index2++;
			cond = 0;
			break;
		case	2:
			(void)printf("%s\n", outbuf);
			if ( format == NEW_FORMAT)
			   printf("{\n");
			(void)printf("\n\t/* multiple byte character width information */\n\n");
			for(j=0; j<7; j++) 
				(void)printf("\t%d,", ctype[START_CSWIDTH + j]);
			(void)printf("\n\t/* Fill 3 bytes */\n");
			(void)printf("\t0,\t0,\t0\n");
			(void)printf("};\n");
			break;
		default:
			if ( flag < 2 )
			 (void)printf("%s\n", outbuf);
			break;
		}
		index1 += 4;
		index2 += 4;
	}  /* end while loop */
	if (width)
		comment3();

}



void 
print_new_ultable() 
{

  unsigned char   *hextostr();
  signed short s;
  int i,j=0;
   
 (void)printf("signed short \t_ctype_new2[] =  {");
 (void)printf("\t0,\n\t");

 /* Print lower table */
        for ( i=258; i < SIZE - 7 - 2; i++)
        {
            if ( ctype[i-257] & ISUPPER )
                /* The character is an upper case, store its lower case */
                s = ctype [i];
            else
                /* The character is not an upper case, store the char itself */
                s = i - 258 ;
	  (void)printf("0x");  
          (void)printf("%s,\t", (char *) hextostr(s));
	  if ( (++j % 4) == 0 ) 
	    (void)printf("\n\t");
	}
	  (void)printf("\n\t/* End of the Lower case table */\n\n\t");

        /* Print upper table */
        for ( i=258; i < SIZE - 7 - 2 ; i++)
        {
            if ( ctype [i-257] & ISLOWER )
                /* The character is an lower case, store its upper case */
                s = ctype [i];
            else
                /* The character is not an upper case, store the char itself */
                s = i - 258;
	  (void)printf("0x");  
          (void)printf("%s,\t", (char *) hextostr(s));
	  if ( (++j % 4) == 0 )
		(void)printf("\n\t");
        }
	  (void)printf("\n\t/* End of the Upper case table */\n};\n\n");
(void)printf("char \t_ctype_new2[] =  {\n");
	for(j=0; j<7; j++)
         (void)printf("\t%d,", ctype[START_CSWIDTH + j]);
         (void)printf("\n\t/* Fill one byte */\n");
                        (void)printf("\t0\n");
                        (void)printf("};\n");

}


/* create2() produces a data file containing the ctype array
 * elements. The name of the file is the same as the value
 * of the environment variable LC_CTYPE.
 */


static void
create2(int format)
{
    register   i=0;
    if ( !o_flag ||  ( o_flag  && format == OLD_FORMAT )) 
	if (freopen(tablename1, "w", stdout) == NULL)
	{
	    perror(tablename1);
	    exit(1);
	}
    if ( format == OLD_FORMAT)
    {
	/* Write magic number */
	int magic = CTYPE_MAGIC_NB;
	fwrite ( & magic, sizeof ( int ), 1, stdout );

	/* Print the entire table in old format */
	for (i=0; i< START_NUMERIC; i++) 
	    if ( (i-1) == isblank_old_char )
		(void)printf("%c", ctype[i] | ISBLANK_OLD );
	    else
		(void)printf("%c", ctype[i]);

	/* to fill up the table to make it aligned */
	(void)printf( "%1$c%1$c%1$c" , 0 );
    }
    else
    {
	signed short s;

	/* Ctype portion */
	for ( i=0; i < 257; i++) 
	    if (fwrite ( &ctype[i], sizeof (unsigned int ), 1, stdout) != 1)
		perror(tablename1),exit(1);	

	/* Print lower table */
	for ( i=START_UL; i < START_CSWIDTH ; i++)
	{
	    if ( ctype[i-START_UL] & ISUPPER )
		/* The character is an upper case, store its lower case */
		s = ctype [i];
	    else
		/* The character is not an upper case, store the char itself */	
		s = i - START_UL - 1  ; /* Substract 1 because first char is EOF */
	    fwrite ( &s, sizeof ( s ), 1, stdout );
	}
	
	/* Print upper table */
	for ( i=START_UL; i < START_CSWIDTH ; i++)
	{
	    if ( ctype [i-START_UL] & ISLOWER )
		/* The character is an lower case, store its upper case */
		s = ctype [i];
	    else
		/* The character is not an upper case, store the char itself */
		s = i - START_UL - 1  ; /* Substract 1 because first char is EOF */
	    
	    fwrite ( &s, sizeof ( s ), 1, stdout );
	}

	/* Print cswidth */
	for ( i = START_CSWIDTH ; i < SIZE -2 ; i ++ )
	    printf ( "%c", ctype[i] );

	/* to fill up the table to make it aligned */
	printf( "%c", 0 );
    }
    
}

static void
create3()
{
	int	length;
	char	*numeric_name;

	if (freopen(tablename2, "w", stdout) == NULL) {
		perror(tablename2);
		exit(1);
	}
	(void)printf("%c%c", ctype[START_NUMERIC], ctype[START_NUMERIC + 1]);
	if (grouping_infop != (char *)NULL) {
		(void)printf("%s", grouping_infop);
		(void)putchar('\0');
	}
}


/* Convert a hexadecimal number to a string */

unsigned char *
hextostr(num)
int	num;
{
	unsigned char	*idx;
	static unsigned char	buf[64];
	idx = buf + sizeof(buf);
	*--idx = '\0';
	do {
		*--idx = "0123456789abcdef"[num % 16];
		num /= 16;
	  } while (num);
	return(idx);
}

static void
comment1()
{
	(void)printf("#include <ctype.h>\n\n\n");
	(void)printf("\t/*\n");
	(void)printf("\t ************************************************\n");
	(void)printf("\t *		%s  CHARACTER  SET                \n", tablename1);
	(void)printf("\t ************************************************\n");
	(void)printf("\t */\n\n");
	(void)printf("\t/* The first 257 characters are used to determine\n");
	(void)printf("\t * the character class */\n\n");
}

static void
n_comment1()
{
      if ( !o_flag)
	(void)printf("#include <ctype.h>\n\n\n");
	(void)printf("\t/*\n");
	(void)printf("\t ************************************************\n");
	(void)printf("\t *		%s  CHARACTER  SET                \n", tablename1);
	(void)printf("\t ************************************************\n");
	(void)printf("\t */\n\n");
	(void)printf("\t/* NEW 32-bit character format\n");
	(void)printf("\t * The first 257 characters are used to determine\n");
	(void)printf("\t * the character class */\n\n");
}

static void
comment2(int format)
{
	if ( format == OLD_FORMAT) {
	 (void)printf("\n\n\t/* The next 257 characters are used for \n");
	 (void)printf("\t * upper-to-lower and lower-to-upper conversion */\n\n");
	}
	else {
	 (void)printf("};\n\n\t/* The next 514 characters are used for \n");
	 (void)printf("\t * upper-to-lower and lower-to-upper conversion */\n\n");
	}
}

static void
comment3()
{
	(void)printf("\n\n\t/*  CSWIDTH INFORMATION                           */\n");
	(void)printf("\t/*_____________________________________________   */\n");
	(void)printf("\t/*                    byte width <> screen width  */\n");
	(void)printf("\t/* SUP1	  		     %d    |     %d         */\n",
		ctype[START_CSWIDTH], ctype[START_CSWIDTH + 3]);
	(void)printf("\t/* SUP2			     %d    |     %d         */\n",
		ctype[START_CSWIDTH + 1], ctype[START_CSWIDTH + 4]);
	(void)printf("\t/* SUP3			     %d    |     %d         */\n",
		ctype[START_CSWIDTH + 2], ctype[START_CSWIDTH + 5]);
	(void)printf("\n\t/* MAXIMUM CHARACTER WIDTH        %d               */\n",
		ctype[START_CSWIDTH + 6]);
}

/*VARARGS*/
static	void
error(va_alist)
va_dcl
{
	va_list	args;
	char	*fmt;
	char	*file;

	va_start(args);
	file = va_arg(args, char *);
	(void)fprintf(stderr, "ERROR in %s: ", file);
	fmt = va_arg(args, char *);
	(void)vfprintf(stderr, fmt, args);
	(void)fprintf(stderr, "\n");
	va_end(args);
	clean();
}

static void
check_chrclass(cp)
unsigned char	*cp;
{
	if (chrclass_num != UL)
		if (*cp == '<' || *cp == '>')
			error(input, msg[9], chrclass_name, *cp);
		else
			;
	else
		if (*cp == '-')
			error(input, msg[9], chrclass_name, *cp);
		else
			;
}

static void
clean()
{
	(void)signal(SIGINT, SIG_IGN);
	(void)unlink("ctype.c");
	(void)unlink(tablename1);
	(void)unlink(tablename2);
	(void)exit(1);
}

/*
 *
 * 	n1[:[s1][,n2:[s2][,n3:[s3]]]]
 *      
 *	n1	byte width (supplementary code set 1)
 *	n2	byte width (supplementary code set 2)
 *	n3	byte width (supplementary code set 3)
 *	s1	screen width (supplementary code set 1)
 *	s2	screen width (supplementary code set 2)
 *	s3	screen width (supplementary code set 3)
 *
 */
static int
cswidth()
{
        char *byte_width[3];
	char *screen_width[3];
	char *buf;
	int length;
	unsigned int len;
	int suppl_set = 0;
	unsigned char *cp;

	if (*(p+strlen((char *)p)-1) != '\n') /* terminating newline? */
		return(0);
	p = clean_line(p);
	if (!(length = strlen((char *)p))) /* anything left */
		return(0);
	if (! isdigit((char)*p) || ! isdigit((char)*(p+length-1)) )
		return(0);
        if ((buf = malloc((unsigned)length + 1)) == NULL)
		error(cmdname, msg[13]);
	(void)strcpy(buf, (char *)p);
	ctype[START_CSWIDTH] = ctype[START_CSWIDTH + 6] = 0;
	ctype[START_CSWIDTH + 3] = ctype[START_CSWIDTH + 4] = ctype[START_CSWIDTH + 5] = UNDEFINED_CSWIDTH;
	cp = p;
	while (suppl_set < 3) {
		if ( !(byte_width[suppl_set] = strtok((char *)cp, tokens)) || ! check_digit(byte_width[suppl_set]) ){
			return(0);
		}
		ctype[START_CSWIDTH  + suppl_set] = atoi(byte_width[suppl_set]);
		if ( p + length == (unsigned char *)(byte_width[suppl_set] + 1) )
			break;
		len = (unsigned char *)(byte_width[suppl_set] + 1) - p;
		if (*(buf + len) == ',') {
			cp = (unsigned char *)(byte_width[suppl_set] + 2);
			suppl_set++;
			continue;
		}
		tokens[0] = ',';
		if ( !(screen_width[suppl_set] = strtok((char *)0, tokens)) || ! check_digit(screen_width[suppl_set]) )  {
			return(0);
		}
		ctype[START_CSWIDTH + suppl_set + 3] = atoi(screen_width[suppl_set]);
		if ( p + length == (unsigned char *)(screen_width[suppl_set] + 1) )
			break;
		cp = (unsigned char *)(screen_width[suppl_set] + 2);
		tokens[0] = ':';
		suppl_set++;
	}
	suppl_set = 0;
	while (suppl_set < 3) {
		if ( ctype[START_CSWIDTH + suppl_set + 3] == UNDEFINED_CSWIDTH )
			ctype[START_CSWIDTH + suppl_set + 3] = ctype[START_CSWIDTH + suppl_set];
		suppl_set++;
	}
	ctype[START_CSWIDTH + 6] = ctype[START_CSWIDTH];
	if (ctype[START_CSWIDTH + 1] + 1 > ctype[START_CSWIDTH + 6]) 
		ctype[START_CSWIDTH + 6] = ctype[START_CSWIDTH + 1] + 1;
	if (ctype[START_CSWIDTH + 2] + 1 > ctype[START_CSWIDTH + 6]) 
		ctype[START_CSWIDTH + 6] = ctype[START_CSWIDTH + 2] + 1;
return(1);
}

static unsigned char  *
clean_line(s)
unsigned char *s;
{
	unsigned char  *ns;

	*(s + strlen((char *)s) -1) = (char) 0; /* delete newline */
	if (!strlen((char *)s))
		return(s);
	ns = s + strlen((char *)s) - 1; /* s->start; ns->end */
	while ((ns != s) && (isspace((char)*ns))) {
		*ns = (char)0;	/* delete terminating spaces */
		--ns;
		}
	while (*ns)             /* delete beginning white spaces */
		if (isspace((char)*s))
			++s;
		else
			break;
	return(s);
}

static int
check_digit(s)
char *s;
{
	if (strlen(s) != 1 || ! isdigit(*s))
		return(0);
	else
		return(1);
}

static int
setnumeric(num_category)
int	num_category;
{
	int	len;
	char	c;

	p = clean_line(p);
	if ((len = strlen((char *)p)) == 0)
		return(1);
	if (len > 1)
	    c = (char) conv_num ( p );
	else
	    c = p[0];
	
	ctype[START_NUMERIC + num_category - DECIMAL_POINT] = (int) c;
	return(1);
}

/*
 * grouping processing
 */

#define SKIPWHITE(ptr)	while(isspace(*ptr)) ptr++

/*
 * getstr - get string value after keyword
 */
static char *
getstr(ptr)
char *ptr;
{
	char	buf[BUFSIZ];
	char	*nptr;
	int	j;

	if (*ptr++ != '"') {
		error(input, msg[14], chrclass_name);
	}
	j = 0;
	while (*ptr != '"') {
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
		} else if (*ptr == '\n' || *ptr == '\0') {
			error(input, msg[14], chrclass_name);
		} else
			buf[j++] = *ptr;
		ptr++;
	} /* while() */
	ptr++;
	SKIPWHITE(ptr);
	if (*ptr != '\0') {
		error(input, msg[14], chrclass_name);
	}
	buf[j] = '\0';
	if ((ptr = strdup(buf)) == NULL) {
		error(cmdname, msg[13]);
	}
	return(ptr);
}

static
int newkey(int format, int num)
{

	if (format == NEW_FORMAT || ( format == OLD_FORMAT && num != ISALPHA && 
		num != ISGRAPH && num != ISPRINT )) 
	 return 1;
	else
	 return 0;
}
