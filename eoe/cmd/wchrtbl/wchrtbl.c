/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)wchrtbl:wchrtbl.c	1.3"				
/*								*/
/* 	wchrtbl - generate character class definition table	*/
/*			for supplementary code set		*/
/*								*/
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <widec.h>
#include <wctype.h>
#include <locale.h>
#include <pfmt.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include "_wchar.h"

/*	Definitions	*/
#define CTYPE_MAGIC_NB	0x77229966

#define BtoVAL(x)	((x & 0x7f)|((x & 0x7f00)>>1)|((x & 0x7f0000)>>2))
#define VALtoB(x)	((x & 0x7f)|((x & 0x3f80)<<1)|((x & 0x1fc000)<<2))
#define HEX    1
#define OCTAL  2
#define RANGE  1
#define UL_CONV 2
#define	CSLEN	10
#define SIZE	2 * 257 + CSLEN 	/* SIZE must be multiple of 4	*/
#define START_UL	257
#define	START_CSWIDTH	(2 * 257)
#define	START_NUMERIC	(2 * 257) + 7
#define	ISUPPER		1
#define ISLOWER		2
#define ISDIGIT		4 
#define ISSPACE		8	
#define ISPUNCT		16
#define ISCNTRL		32
#define ISBLANK_OLD	64  /*  Obsolete */
#define ISXDIGIT	128
#define ISALPHA         0x00004000
#define ISPRINT         0x00008000
#define ISGRAPH         0x40000000
#define ISBLANK         0x80000000

#define UL		0xff
#define	ISWCHAR1	0x00000100	/* phonogram (international use) */
#define	ISWCHAR2	0x00000200	/* ideogram (international use) */
#define	ISWCHAR3	0x00000400	/* English (international use) */
#define	ISWCHAR4	0x00000800	/* number (international use) */
#define	ISWCHAR5	0x00001000	/* special (international use) */
#define	ISWCHAR6	0x00002000	/* reserved (international use) */
/*  the two below were reserved, now use for ISALPHA and ISPRINT    **
 * #define	ISWCHAR7	0x00004000	
 * #define	ISWCHAR8	0x00008000	
*/
#define	ISWCHAR9	0x00010000
#define	ISWCHAR10	0x00020000
#define	ISWCHAR11	0x00040000
#define	ISWCHAR12	0x00080000
#define	ISWCHAR13	0x00100000
#define	ISWCHAR14	0x00200000
#define	ISWCHAR15	0x00400000
#define	ISWCHAR16	0x00800000
#define	ISWCHAR17	0x01000000
#define	ISWCHAR18	0x02000000
#define	ISWCHAR19	0x04000000
#define	ISWCHAR20	0x08000000
#define	ISWCHAR21	0x10000000
#define	ISWCHAR22	0x20000000
/*  the two below were reserved, now use for ISGRAPH and ISBLANKCLASS
 * #define	ISWCHAR23	0x40000000
 * #define	ISWCHAR24	0x80000000
*/

#define OLD_FORMAT              0
#define NEW_FORMAT              1


#define	WLC_CTYPE	10
#define	CSWIDTH		11
#define	WLC_NUMERIC	12
#define	DECIMAL_POINT	13
#define	THOUSANDS_SEP	14
#define GROUPING	15

#define LC_CTYPE1	21
#define LC_CTYPE2	22
#define LC_CTYPE3	23
#define TMIN		239
#define TMAX		238
#define CMIN		237
#define CMAX		236
#define	WSIZE		16*1024

extern	char	*malloc();
extern	void	exit();
extern	int	unlink();
extern	int	atoi();

/*   Internal functions  */

static	void	error(char *fmt, ...);
static	void	init();
static	void	process();
static  void    check_posix();
static	void	create1();
static	void	create2();
static	void	create3();

static	void	pre_process();
static	void	create0w();
static	void	create1w();
static	void	create2w();
static	void	setmem();
static	void	resetmem();

static  void	parse();
static	int	empty_line();
static	void	check_chrclass();
static	void	clean();
static	void	comment1();
static	void	comment2();
static	void	comment3();

static	void	comment4();
static	void	comment5();

static 	int	cswidth();
static 	int	setnumeric();
static	int	check_digit();
static	unsigned  char	*clean_line();
static	char	*getstr();
static int      newkey();

/*	Static variables	*/

static	int	qtmin[4] = {0,0,0,0};
static	int	qtmax[4] = {WSIZE,WSIZE,WSIZE,WSIZE};
static	int	qcmin[4] = {0,0,0,0};
static	int	qcmax[4] = {WSIZE,WSIZE,WSIZE,WSIZE};

struct  classname	{
	char	*name;
	int	num;
	char	*repres;
	char    *posix_members;
}  cln[]  =  {
	"isupper",	ISUPPER,	"_U",	"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
	"islower",	ISLOWER,	"_L",	"abcdefghijklmnopqrstuvwxyz",
	"isdigit",	ISDIGIT,	"_N",	"0123456789",
	"isspace",	ISSPACE,	"_S",	" \f\n\r\t\v",
	"ispunct",	ISPUNCT,	"_P",	NULL,
	"iscntrl",	ISCNTRL,	"_C",	NULL,
	"isblank",	ISBLANK,	"_BL",	" \t",
	"isxdigit",	ISXDIGIT,	"_X",	"0123456789ABCDEFabcdef",
	"isalpha",      ISALPHA,        "_A",	NULL,
        "isprint",      ISPRINT,        "_PR",	" ",
        "isgraph",      ISGRAPH,        "_G",	NULL, 
	"iswchar1",	ISWCHAR1,	"_E1",	NULL,
	"isphonogram",	ISWCHAR1,	"_E1",	NULL,
	"iswchar2",	ISWCHAR2,	"_E2",	NULL,
	"isideogram",	ISWCHAR2,	"_E2",	NULL,
	"iswchar3",	ISWCHAR3,	"_E3",	NULL,
	"isenglish",	ISWCHAR3,	"_E3",	NULL,
	"iswchar4",	ISWCHAR4,	"_E4",	NULL,
	"isnumber",	ISWCHAR4,	"_E4",	NULL,
	"iswchar5",	ISWCHAR5,	"_E5",	NULL,
	"isspecial",	ISWCHAR5,	"_E5",	NULL,
	"iswchar6",	ISWCHAR6,	"_E6",	NULL,
	"iswchar9",	ISWCHAR9,	"_E9",	NULL,
	"iswchar10",	ISWCHAR10,	"_E10",	NULL,
	"iswchar11",	ISWCHAR11,	"_E11",	NULL,
	"iswchar12",	ISWCHAR12,	"_E12",	NULL,
	"iswchar13",	ISWCHAR13,	"_E13",	NULL,
	"iswchar14",	ISWCHAR14,	"_E14",	NULL,
	"iswchar15",	ISWCHAR15,	"_E15",	NULL,
	"iswchar16",	ISWCHAR16,	"_E16",	NULL,
	"iswchar17",	ISWCHAR17,	"_E17",	NULL,
	"iswchar18",	ISWCHAR18,	"_E18",	NULL,
	"iswchar19",	ISWCHAR19,	"_E19",	NULL,
	"iswchar20",	ISWCHAR20,	"_E20",	NULL,
	"iswchar21",	ISWCHAR21,	"_E21",	NULL,
	"iswchar22",	ISWCHAR22,	"_E22",	NULL,
	"ul",		UL,		NULL,	NULL,
	"LC_CTYPE",	WLC_CTYPE,	NULL,	NULL,
	"cswidth",	CSWIDTH,	NULL,	NULL,
	"LC_NUMERIC",	WLC_NUMERIC,	NULL,	NULL,
	"decimal_point", DECIMAL_POINT,	NULL,	NULL,
	"thousands_sep", THOUSANDS_SEP,	NULL,	NULL,
	"grouping",	GROUPING,	NULL,	NULL,
	"LC_CTYPE1",	LC_CTYPE1,	NULL,	NULL,
	"LC_CTYPE2",	LC_CTYPE2,	NULL,	NULL,
	"LC_CTYPE3",	LC_CTYPE3,      NULL,	NULL,
	"tmin",		TMIN,		NULL,	NULL,
	"tmax",		TMAX,		NULL,	NULL,
	"cmin",		CMIN,		NULL,	NULL,
	"cmax",		CMAX,		NULL,	NULL,
	NULL,		NULL,		NULL,	NULL
};

int	readstd;			/* Process the standard input */
unsigned char	linebuf[256];		/* Current line in input file */
unsigned char	 *p = linebuf;
int	chrclass = 0;			/* set if LC_CTYPE is specified */
int	lc_numeric;			/* set if LC_NUMERIC is specified */
int	lc_ctype;
char	chrclass_name[20];		/* save current chrclass name */
int	chrclass_num;			/* save current chrclass number */
int     o_flag =1;                      /* by default, chrtbl generates the
                                           the old and new format 32-bit n_ctype table.
                                           specify -o to have it generated
                                            just the new one */
int     warn_posix = 1;                 /* Print out POSIX warnings if a character class
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
char	input[PATH_MAX];		/* Save input file name */
char	tokens[] = ",:\0";

int	codeset=0;			/* code set number */
int	codeset1=0;			/* set when charclass1 found */
int	codeset2=0;			/* set when charclass2 found */
int	codeset3=0;			/* set when charclass3 found */
unsigned *wctyp[3];			/* tmp table for wctype */
long	*wconv[3];			/* tmp table for conversion */
struct	_wctype	*wcptr[3];		/* pointer to ctype table */
struct	_wctype	wctbl[3];		/* table for wctype */
unsigned char *index[3];		/* code index	*/
unsigned *type[3];			/* code type	*/
long	*code[3];			/* conversion code	*/
int	cnt_index[3];			/* number of index	*/
int	cnt_type[3];			/* number of type	*/
int	cnt_code[3];			/* number conversion code */

char	isblank_old_char = ' ';		/* Old character defined by isblank */

/* added for grouping */
char	*grouping_infop = (char *)NULL;	/* grouping information */

/* Error  messages */

char	*msg[] = {
/*    0    */ ":12:Usage: wchrtbl [-n] [-s] [ - | file ] ",
/*    1    */ ":13:%s: name of output file for \"LC_CTYPE\" is not specified",
/*    2    */ ":14:%s: incorrect character class name \"%s\"",
/*    3    */ ":15:%s: --- %s --- left angle bracket  \"<\" is missing",
/*    4    */ ":16:%s: --- %s --- right angle bracket  \">\" is missing",
/*    5    */ ":17:%s: --- %s --- wrong input specification \"%s\"",
/*    6    */ ":18:%s: --- %s --- number out of range \"%s\"",
/*    7    */ ":19:%s: --- %s --- nonblank character after (\"\\\") on the same line",
/*    8    */ ":20:%s: --- %s --- wrong upper limit \"%s\"",
/*    9    */ ":21:%s: --- %s --- wrong character \"%c\"",
/*   10    */ ":22:%s: --- %s --- number expected",
/*   11    */ ":23:%s: --- %s --- too many range \"-\" characters",
/*   12    */ ":24:%s: --- %s --- wrong specification, %s",
/*   13    */ ":25:malloc error",
/*   14	   */ ":26:%s: --- %s --- wrong specification",
/*   15    */ ":27:%s: name of output file for \"LC_NUMERIC\" is not specified",
/*   16    */ ":28:%s: \"decimal_point\" or \"thousands_sep\" must be specified",
/*   17    */ ":29:%s: character classification and conversion information must be specified",
/*   18    */ ":30:%s: same output file name used in \"LC_CTYPE\" and \"LC_NUMERIC\"",
/*   19    */ ":31:%s: character class \"LC_CTYPE%d\" duplicated",
/*   20    */ ":32:%s: character class table \"LC_CTYPE%d\" exhausted",
/*   21    */ ":48:Warning: locale is not POSIX-compliant because 0x%x is not in %s\n"
};

static char cswidth_info[] = "\n\nCSWIDTH FORMAT: n1[[:s1][,n2[:s2][,n3[:s3]]]]\n\tn1 byte width (SUP1)\n\tn2 byte width (SUP2)\n\tn3 byte width (SUP3)\n\ts1 screen width (SUP1)\n\ts2 screen width (SUP2)\n\ts3 screen width (SUP3)";


main(argc, argv)
int	argc;
char	**argv;
{
	int opt;
        extern int optind;

	/* Initialize locale, label, and message catalog. */
	(void)setlocale(LC_ALL, "");
	(void)setlabel("UX:wchrtbl");
	(void)setcat("uxsysadm");

	p = linebuf;
        while (( opt = getopt(argc, argv, "ns")) != -1)
           switch (opt) {
                case 'n':
                        /* only new format table will be produced */
                        o_flag = 0;
                        break;
                case 's':
                        /* no posix warning, silent mode */
                        warn_posix = 0;
                        break;
                default:
                        error(msg[0]);
                        exit(1);
                        }

        if ( optind == argc || ((optind == argc-1) 
		&& (strcmp(argv[argc-1], "-") == 0))){
                   readstd++;
                   (void)strcpy(input, gettxt(":33", "standard input"));
                }
        else
                (void)strncpy(input, argv[argc-1], PATH_MAX);

        if (signal(SIGINT, SIG_IGN) == SIG_DFL)
                (void)signal(SIGINT, clean);
        if (!readstd && freopen(argv[argc-1], "r", stdin) == NULL) {
		(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", argv[argc-1], strerror(errno));
                exit(1);
                }
	init();
	if (o_flag)
	  process(OLD_FORMAT);
        else
         process(NEW_FORMAT);
	if (!lc_ctype && chrclass)
		error(msg[17], input);
	if (lc_ctype) {
		if (!chrclass) 
			error(msg[1], input);
		else {
			if (o_flag) {
			 if (codeset1 || codeset2 || codeset3)
				create0w();
			 create1(OLD_FORMAT);
			 create1(NEW_FORMAT);
			 if (codeset1 || codeset2 || codeset3)
				create1w();

			 create2(OLD_FORMAT);
			 create2(NEW_FORMAT);
			 if (codeset1 || codeset2 || codeset3)
				create2w();
			}
			else {
			 if (codeset1 || codeset2 || codeset3)
                                create0w();
                          create1(NEW_FORMAT);
                         if (codeset1 || codeset2 || codeset3)
                                create1w();
                         create2(NEW_FORMAT);
                         if (codeset1 || codeset2 || codeset3)
                                create2w();
                }

		}
	}
	if (lc_numeric && !numeric)
		error(msg[16], input);
	if (numeric && !lc_numeric)
		error(msg[15], input);
	if (strcmp(tablename1, tablename2) == NULL)	
		error(msg[18], input);
	if (lc_numeric && numeric)
		create3();
	exit(0);
}


/* Initialize the ctype array */

static	void
init()
{
	register i;
	register j;

	for(i=0; i<256; i++)
		(ctype + 1)[257 + i] = i;

	ctype[START_CSWIDTH] = 1;
	ctype[START_CSWIDTH + 1] = 0;
	ctype[START_CSWIDTH + 2] = 0;
	ctype[START_CSWIDTH + 3] = 1;
	ctype[START_CSWIDTH + 4] = 0;
	ctype[START_CSWIDTH + 5] = 0;
	ctype[START_CSWIDTH + 6] = 1;
	for(i=0; i<3; i++){
		wcptr[i] = 0;
		cnt_index[i] = 0;
		cnt_type[i] = 0;
		cnt_code[i] = 0;
	}
}


/* Read line from the input file and check for correct
 * character class name */

static	void
process(format)
{

	unsigned char	*token();
	register  struct  classname  *cnp;
	register unsigned char *c;
	int	i;

	for (;;) {
		if (fgets((char *)linebuf, sizeof linebuf, stdin) == NULL ) {
			if (ferror(stdin)) {
				(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", "stdin", strerror(errno));
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
			error(msg[2], input, c);
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
		case ISWCHAR1:
		case ISWCHAR2:
		case ISWCHAR3: 
		case ISWCHAR4:
		case ISWCHAR5:
		case ISWCHAR6:
		case ISWCHAR9:
		case ISWCHAR10:
		case ISWCHAR11:
		case ISWCHAR12:
		case ISWCHAR13:
		case ISWCHAR14:
		case ISWCHAR15:
		case ISWCHAR16:
		case ISWCHAR17:
		case ISWCHAR18:
		case ISWCHAR19:
		case ISWCHAR20:
		case ISWCHAR21:
		case ISWCHAR22:
		case UL:
				lc_ctype++;
				(void)strcpy(chrclass_name, cnp->name);
				chrclass_num = cnp->num;
				parse(cnp->num);
				break;

		case WLC_CTYPE:
				chrclass++;
				if ( (c = token()) == NULL )
					error(msg[1], input);
				(void)strcpy(tablename1, "\0");
				(void)strcpy(tablename1, (char *)c);
				if (freopen("wctype.c", "w", stdout) == NULL) {
					(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", "wctype.c", strerror(errno));
					exit(1);
				}
				break;
		case WLC_NUMERIC:
				if ((o_flag && format == OLD_FORMAT) || (!o_flag )){
				 lc_numeric++;
				 if ( (c = token()) == NULL )
					error(msg[15], input);
				 (void)strcpy(tablename2, "\0");
				 (void)strcpy(tablename2, (char *)c);
				}
				break;
		case CSWIDTH:
				width++;
				(void)strcpy(chrclass_name, cnp->name);
				if (! cswidth() )
					error(msg[12], input, chrclass_name, cswidth_info);
				break;
		case DECIMAL_POINT:
		case THOUSANDS_SEP:
				numeric++;
				(void)strcpy(chrclass_name, cnp->name);
				if (! setnumeric(cnp->num) )
					error(msg[14], input, chrclass_name);
				break;
		case GROUPING:
				(void)strcpy(chrclass_name, cnp->name);
				p = clean_line(p);
				if (strlen((char *)p) == 0)
					break;
				grouping_infop = getstr(p);
				break;

		case LC_CTYPE1:
				if (codeset1)
					error(msg[19], input, codeset);
				codeset=1;
				codeset1=1;
				(void)setmem(1);
				break;
		case LC_CTYPE2:
				if (codeset2)
					error(msg[19], input, codeset);
				codeset=2;
				codeset2=1;
				(void)setmem(2);
				break;
		case LC_CTYPE3:
				if (codeset3)
					error(msg[19], input, codeset);
				codeset=3;
				codeset3=1;
				(void)setmem(3);
				break;
		case TMIN:
				if( (c=token()) == NULL ) 
					error(msg[1], input);
				i = conv_num(c);
				qtmin[codeset] = BtoVAL(i);
				break;
		case TMAX:   
				if( (c=token()) == NULL ) 
					error(msg[1], input);
				i = conv_num(c);
				qtmax[codeset] = BtoVAL(i);
				break;                                 
		case CMIN:                                          
				if( (c=token()) == NULL )              
				        error(msg[1], input);          
				i = conv_num(c);                       
				qcmin[codeset] = BtoVAL(i);
				break;                     
		case CMAX:                              
				if( (c=token()) == NULL )  
				        error(msg[1], input);
				i = conv_num(c);           
				qcmax[codeset] = BtoVAL(i);
				resetmem(codeset);
				break;
		}
	} /* for loop */

        if ( warn_posix )
            check_posix();

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
               pfmt( stderr,MM_WARNING,":48:Warning: locale is not POSIX-compliant because 0x%x is not in %s\n",  *c, cl->name );
    }
}


static	int
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
				error(msg[10], input, chrclass_name);
			ul_conv = 0;
			p++;
			continue;
		case '-':
			if (action == RANGE)
				error(msg[11], input, chrclass_name);
			action = RANGE;
			p++;
			continue;
		case '<':
			if (ul_conv)
				error(msg[4], input, chrclass_name);
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
		error(msg[5], input, chrclass_name, s);
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
			error(msg[5], input, chrclass_name, s);
	}
	if ( num == HEX )  { 
		if (cp[j] != '\0' || sscanf((char *)s, "0x%x", &i) != 1)  
			error(msg[5], input, chrclass_name, s);
		if ((codeset == 0 && (i > 0xff)) ||
		    (codeset != 0 && ((i > 0xffffff) ||
			(i >> (ctype[START_CSWIDTH+codeset-1] << 3)))))
			error(msg[6], input, chrclass_name, s);
		else
			return(i);
	}
	if (cp[j] != '\0' || sscanf((char *)s, "0%o", &i) != 1) 
		error(msg[5], input, chrclass_name, s);
	if ((codeset == 0 && (i > 0xff)) ||
	    (codeset != 0 && ((i > 0xffffff) ||
		(i >> (ctype[START_CSWIDTH+codeset-1] << 3)))))
		error(msg[6], input, chrclass_name, s);
	else
		return(i);
/*NOTREACHED*/
}

/* parse() gets the next token and based on the character class
 * assigns a value to corresponding table entry.
 * It also handles ranges of consecutive numbers and initializes
 * the uper-to-lower and lower-to-upper conversion tables.
 */

static	void
parse(type)
int type;
{
	unsigned char	*c;
	int	ch1 = 0;
	int	ch2;
	int 	lower= 0;
	int	upper;
	int	idx;
	while ( (c = token()) != NULL) {
		if ( *c == '\\' ) {
			if ( ! empty_line()  || strlen((char *)c) != 1) 
				error(msg[7], input, chrclass_name);
			cont = 1;
			break;
			}
		switch(action) {
		case	RANGE:
			upper = conv_num(c);
			if ( (!in_range) || (in_range && range) ) 
				error(msg[8], input, chrclass_name, c);
			if (codeset == 0)
				((ctype + 1)[upper]) |= type;
			else {
				idx = BtoVAL(upper) - qtmin[codeset]; 
				wctyp[codeset-1][idx]|=type;
			}
			if ( upper <= lower ) 
				error(msg[8], input, chrclass_name, c);
			while ( ++lower <= upper ) 
				if (codeset == 0)
				        ((ctype + 1)[lower]) |= type;
				else{
					idx = BtoVAL(lower) - qtmin[codeset];
if(idx >= qtmax[codeset] - qtmin[codeset])
pfmt(stderr,MM_ERROR, ":34:Idx (%d) too large...\n", idx);
				        wctyp[codeset - 1][idx] |= type;
				}
			action = 0;
			range = 0;
			in_range = 0;
			break;
		case	UL_CONV:
			ch2 = conv_num(c);
			if (codeset == 0){				
				(ctype + 1)[ch1 + 257] = ch2;           
				(ctype + 1)[ch2 + 257] = ch1;           
			}						
			else{		                                
				wconv[codeset - 1][BtoVAL(ch1)-qcmin[codeset]]= ch2;
				wconv[codeset - 1][BtoVAL(ch2)-qcmin[codeset]]= ch1;
			}						
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
					error(msg[3], input, chrclass_name);
			else
				if (range)
					{
					action = RANGE;
					range = 0;
					}
				else
					;
			
			if (codeset == 0)
				((ctype + 1)[lower]) |= type;
			else {
				idx = BtoVAL(lower) - qtmin[codeset];
				wctyp[codeset - 1][idx] |= type;
			}
			break;
		}
	}
	if (action)
		error(msg[10], input, chrclass_name);
}

/* create0w() produce wctype structure in memory */

static	void
create0w()
{
	register i,j,k,l;
	int	s,mask;
	off_t	index_off, type_off, code_off;
	unsigned sv_type;
	if (codeset1)
		wcptr[0] = &wctbl[0];
	if (codeset2)
		wcptr[1] = &wctbl[1];
	if (codeset3)
		wcptr[2] = &wctbl[2];
	index_off = sizeof (struct _wctype) * 3;
	for (s = 0; s < 3; s++){
		if (wcptr[s] == 0) continue;
		for (i = 0 ; i<(qtmax[s+1]-qtmin[s+1])&&wctyp[s][i] ==0; i++)
			;       /* search for REAL minimum code in wctype table */
		if (i != (qtmax[s+1]-qtmin[s+1])){
			wctbl[s].tmin = i;
			sv_type = wctyp[s][i];
			cnt_type[s] = 1;
			for (i=(qtmax[s+1]-qtmin[s+1]-1); i>=0 && wctyp[s][i]==0; i--)
			;	/* search maximum code in wctype table */
			wctbl[s].tmax = i;
			cnt_index[s] = wctbl[s].tmax - wctbl[s].tmin + 1;
			if (cnt_index[s] > (qtmax[s+1] - qtmin[s+1]))
				error(msg[20], input, ++s);
			if ((cnt_index[s] % 8) != 0)
				cnt_index[s] = ((cnt_index[s] / 8) + 1) * 8;
if ((index[s]=(unsigned char *)(malloc((unsigned)cnt_index[s]))) == NULL ||
    (type[s]=(unsigned *)(malloc((unsigned)(cnt_index[s] * 4)))) == NULL)
				error(msg[13]);
			type[s][0] = sv_type;
			for (i = 0, j = 0, k = wctbl[s].tmin; i < cnt_index[s]; i++,k++){
				for (l = j; l >= 0; l--)
					if (type[s][l] == wctyp[s][k])
						break;
				if (l >= 0)
					index[s][i] = l;
				else{
					type[s][++j] = wctyp[s][k];
					index[s][i] = j;
					cnt_type[s]++;
					if (j > 256)
						error(msg[20], input, ++s);
				}
			}
			if ((cnt_type[s] % 8) != 0)
				cnt_type[s] = ((cnt_type[s] / 8) + 1) * 8;
		}
		for (i = 0; i<(qcmax[s+1]-qcmin[s+1]) && wconv[s][i]==0xffff; i++)
			;	/* search minimum code in conv table */
		if (i != (qcmax[s+1]-qcmin[s+1])){
			wctbl[s].cmin = i;
			for(i=(qcmax[s+1]-qcmin[s+1]-1);i>=0&&wconv[s][i]==0xffff;i--)
			;	/* search maximum code in conv table */
			wctbl[s].cmax = i;
			cnt_code[s] = wctbl[s].cmax - wctbl[s].cmin + 1;
			if ((cnt_code[s] % 8) != 0)
				cnt_code[s] = ((cnt_code[s] / 8) + 1) * 8;
		    	if ((code[s] = (long *)(malloc((unsigned)cnt_code[s] * 4))) == NULL)
				error(msg[13]);
/* Run time routine now resonsible for building process code format... */
/*			if (s == 0)             */
/*				mask = 0x8080;  */      /* 0x60000000 */
/*			else if (s == 1)        */        
/*				mask = 0x0080;  */      /* 0x20000000 */
/*			else                    */        
/*				mask = 0x8000;  */      /* 0x40000000 */
			j = wctbl[s].cmin + qcmin[s+1];
			for (i = 0; i < cnt_code[s]; i++)
				code[s][i] = j+i;	/* VALtoB(j+i) | mask; */
			if (cnt_code[s] > WSIZE)
				error(msg[20], input, ++s);
			
			for (i = 0, j = wctbl[s].cmin; i < cnt_code[s]; i++, j++){
				if (wconv[s][j] != 0xffff)
					code[s][i] = BtoVAL(wconv[s][j]);
			}
		}
		if (cnt_index[s] != 0)
			wctbl[s].index_off = index_off;
		type_off = index_off + cnt_index[s] * sizeof ( **index );
		if (cnt_type[s] != 0)
			wctbl[s].type_off = type_off;
		code_off = type_off + cnt_type[s] * sizeof ( **type );
		if (cnt_code[s] != 0){
			wctbl[s].code_off = code_off;
		index_off = code_off + cnt_code[s] * sizeof ( **code );
		}
	}
}


/* create1() produces a C source file based on the definitions
 * read from the input file. For example for the current ASCII
 * character classification where LC_CTYPE=ascii it produces a C source
 * file named wctype.c.
 */


static	void
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
	int	k;

	if ( format == NEW_FORMAT ) {
          comment1(NEW_FORMAT);
          (void)sprintf((char *)outbuf,"unsigned int\t_ctype_new[] =  { 0,");
          (void)printf("%s\n",outbuf);
        }
        else {
	 comment1(OLD_FORMAT);
	 (void)sprintf((char *)outbuf,"unsigned char\t_ctype[] =  { 0,");
	 (void)printf("%s\n",outbuf);
	}
	
	index1 = 0;
	index2 = 7;
	while (flag <= 1) {
		for (i=0; i<=7; i++)
			(void)strcpy((char *)out[i].ch, "\0");
		for(i=index1; i<=index2; i++) {
			if ( ! ((ctype + 1)[i]) )  {
				(void)strcpy((char *)out[i - index1].ch, "0");
				continue; }
			num = (ctype + 1)[i];
			if (flag) {      
				(void)strcpy((char *)out[i - index1].ch, "0x");  
				(void)strcat((char *)out[i - index1].ch, (char *)hextostr(num));
				continue; }
			while (num)  {
				for(cnp=cln;cnp->num != UL;cnp++) {
					if(!(num & cnp->num))  
						continue; 
					if ( (strlen((char *)out[i - index1].ch))  == NULL)  
					if ( newkey(format, cnp->num))
						(void)strcat((char *)out[i - index1].ch,cnp->repres);
					else
						(void)strcat((char *)out[i - index1].ch, "0");
					else if ( newkey(format, cnp->num)) {
	
						(void)strcat((char *)out[i - index1].ch,"|");
						(void)strcat((char *)out[i - index1].ch,cnp->repres); }  
				num = num & ~cnp->num;  
					if (!num) 
						break; 
				}  /* end inner for */
			}  /* end while */
		} /* end outer for loop */
		(void)sprintf((char *)outbuf,"\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,",
		out[0].ch,out[1].ch,out[2].ch,out[3].ch,out[4].ch,out[5].ch,
		out[6].ch,out[7].ch);
		if ( ++line_cnt == 32 ) {
			line_cnt = 0;
			flag++; 
			cond = flag; }
		switch(cond) {
		case	1:
			(void)printf("%s\n", outbuf);
			if ( format == NEW_FORMAT) {
			  (void)printf("\n\t/* Add three more bytes to make the table aligned properly */\n");
                          (void)printf("\t0,\t0,\t0\n");
                          (void)printf("};\n");
			 }
			comment2();
			if ( format == NEW_FORMAT)
                          (void)printf("unsigned char\t_ctype_new2[] =  {");
			(void)printf("\t0,\n");
			index1++;
			index2++;
			cond = 0;
			break;
		case	2:
			(void)printf("%s\n", outbuf);
			(void)pfmt(stdout, MM_NOSTD, ":35:\n\t/* multiple byte character width information */\n\n");
			if (codeset1 || codeset2 || codeset3)
				k = CSLEN - 1;
			else
				k = 6;
			for(j=0; j<k; j++){
				if ((j%8 == 0) && (j !=0))
					(void)printf("\n");
				(void)printf("\t%d,", ctype[START_CSWIDTH + j]);
			}
			(void)printf("\t%d\n", ctype[START_CSWIDTH + k]);
			if ( k == 6 ) {
			    (void)printf(",\n\t/* Add three more bytes to make the table aligned properly */\n");
                            (void)printf("\t0,\t0,\t0\n");
			  }
			(void)printf("};\n");
			break;
		default:
			printf("%s\n", outbuf);
			break;
		}
		index1 += 8;
		index2 += 8;
	}  /* end while loop */
	if (width)
		comment3();
}


/* create1w() produces a C source program for supplementary code sets */

static	void
create1w()
{
	struct  field {
		unsigned char  ch[100];
	} out[8];
	unsigned char	*hextostr();
	unsigned char	outbuf[256];
	unsigned char	*cp;
	int	cond = 0, flag = 0, line_cnt = 0;
	int	i, j, index1, index2, num, s;
	register struct classname *cnp;

	comment4();
	(void)sprintf((char *)outbuf,"struct _wctype wctbl[3] = {\n");
	(void)printf("%s",outbuf);
	for (s = 0; s < 3; s++){
		(void)printf("\t{");
		if ((wctbl[s].tmin + qtmin[s+1]) == 0)
			(void)printf("0,\t");
		else
			(void)printf("0x%s,\t",hextostr(wctbl[s].tmin+qtmin[s+1]));
		if ((wctbl[s].tmin + qtmin[s+1]) == 0)
			(void)printf("0,\t");
		else
			(void)printf("0x%s,\t",hextostr(wctbl[s].tmax+qtmin[s+1]));
		if (wctbl[s].index_off == 0)
			(void)printf("0,\t");
		else
			(void)printf("(unsigned char *)0x%s,\t",hextostr(wctbl[s].index_off));
		if (wctbl[s].type_off == 0)
			(void)printf("0,\t");
		else
			(void)printf("(unsigned *)0x%s,\t",hextostr(wctbl[s].type_off));
		if ((wctbl[s].cmin+qcmin[s+1]) == 0)
			(void)printf("0,\t");
		else
			(void)printf("0x%s,\t",hextostr(wctbl[s].cmin+qcmin[s+1]));
		if ((wctbl[s].cmax+qcmin[s+1]) == 0)
			(void)printf("0,\t");
		else
			(void)printf("0x%s,\t",hextostr(wctbl[s].cmax+qcmin[s+1]));
		if (wctbl[s].code_off == 0)
			(void)printf("0");
		else
			(void)printf("(wchar_t *)0x%s",hextostr(wctbl[s].code_off));
		if (s < 2)
			(void)printf("},\n");
		else
			(void)printf("}\n");
	}
	(void)printf("};\n");
	

	comment5();
   for (s = 0; s < 3; s++){
	index1 = 0;
	index2 = 7;
	flag = 0;
	while (flag <= 2) {
		if (line_cnt == 0){
			if (flag == 0 && cnt_index[s])
				(void)printf("unsigned char index%d[] = {\n",s+1);
			else if ((flag == 0 || flag == 1) && cnt_type[s]){
				(void)printf("unsigned type%d[] = {\n",s+1);
				flag = 1;
			}
			else if (cnt_code[s]){
				(void)printf("wchar_t code%d[] = {\n",s+1);
				flag = 2;
			}
			else
				break;
		}
		for (i=0; i<=7; i++)
			(void)strcpy((char *)out[i].ch, "\0");
		for(i=index1; i<=index2; i++) {
			if ((flag == 0 && !index[s][i]) ||
			    (flag == 1 && !type[s][i]) ||
			    (flag == 2 && !code[s][i])){ 
				(void)strcpy((char *)out[i - index1].ch, "0");
				continue; }
			if (flag == 0)
				num = index[s][i];
			else if (flag == 1)
				num = type[s][i];
			else
				num = code[s][i];
			if (flag == 0 || flag ==2) {      
				(void)strcpy((char *)out[i - index1].ch, "0x");  
				(void)strcat((char *)out[i - index1].ch, (char *)hextostr(num));
				continue; }
			while (num)  {
				for(cnp=cln;cnp->num != UL;cnp++) {
					if(!(num & cnp->num))  
						continue; 
					if ( (strlen((char *)out[i - index1].ch))  == NULL)  
						(void)strcat((char *)out[i - index1].ch,cnp->repres);
					else  {
						(void)strcat((char *)out[i - index1].ch,"|");
						(void)strcat((char *)out[i - index1].ch,cnp->repres); }  
				num = num & ~cnp->num;  
					if (!num) 
						break; 
				}  /* end inner for */
			}  /* end while */
		} /* end outer for loop */
		(void)sprintf((char *)outbuf,"\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,\t%s,",
		out[0].ch,out[1].ch,out[2].ch,out[3].ch,out[4].ch,out[5].ch,
		out[6].ch,out[7].ch);
		line_cnt++;
		if ((flag == 0 && line_cnt == cnt_index[s]/8) ||
		    (flag == 1 && line_cnt == cnt_type[s]/8) ||
		    (flag == 2 && line_cnt == cnt_code[s]/8)){
			line_cnt = 0;
			flag++; 
			cond = flag; }
		switch(cond) {
		case	1:
		case	2:
		case	3:
			cp = outbuf + strlen((char *)outbuf);
			*--cp = ' ';
			*++cp = '\0';
			(void)printf("%s\n", outbuf);
			(void)printf("};\n");
			index1 = 0;
			index2 = 7;
			cond = 0;
			break;
		default:
			(void)printf("%s\n", outbuf);
			index1 += 8;
			index2 += 8;
			break;
		}
	}  /* end while loop */
   }
}

/* create2() & create2w() produces a data file containing the ctype array
 * elements. The name of the file is the same as the value
 * of the environment variable LC_CTYPE.
 */


static	void
create2(int format)
{
	register   i=0;
	int	j;
	int 	add_b=0;

	if ( !o_flag ||  ( o_flag  && format == OLD_FORMAT ))
	if (freopen(tablename1, "w", stdout) == NULL){
		(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", "tablename1", strerror(errno));
		exit(1);
	}
	if ( format == OLD_FORMAT) {
	    /* Old ctype table */

	    /* Write magic number */
	    int magic = CTYPE_MAGIC_NB;
	    fwrite ( & magic, sizeof ( int ), 1, stdout );

	    for (i=0; i < START_NUMERIC ; i++)
		if ( (i-1) == isblank_old_char )
		    (void)printf("%c", ctype[i] | ISBLANK_OLD );
		else
		    (void)printf("%c", ctype[i]);

	    /* to fill up the table to make it aligned */
	    (void)printf( "%1$c%1$c%1$c" , 0 );
	    
	}
	else
	{
	    /* New ctype table */
	    signed short s;

	    for (i=0; i< 257; i++) 
		if (fwrite( &ctype[i], sizeof (unsigned int), 1, stdout) != 1)
		{
		    (void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", 
			       "tablename1", strerror(errno));
		    exit(1);
		}

	    /* Print lower table */
	    for ( i = START_UL ; i < START_CSWIDTH ; i++)
	    {
		if ( ctype[i-START_UL] & ISUPPER )
		    /* The character is an upper case, store its lower case */
		    s = ctype [i];
		else
		    /* The character is not an upper case, store the char itself */	
		    s = i - START_UL - 1; /* Substract 1 because first char is EOF */
		fwrite ( &s, sizeof ( s ), 1, stdout );
	    }
	
	    /* Print upper table */
	    for ( i = START_UL ; i < START_CSWIDTH ; i++)
	    {
		if ( ctype [i-START_UL] & ISLOWER )
		    /* The character is an lower case, store its upper case */
		    s = ctype [i];
		else
		    /* The character is not an upper case, store the char itself */
		    s = i - START_UL - 1; /* Substract 1 because first char is EOF */
	    
		fwrite ( &s, sizeof ( s ), 1, stdout );
	    }

	    /* Print cswidth */
	    for ( i = START_CSWIDTH ; i < START_NUMERIC ; i ++ )
		printf ( "%c", ctype[i] );

	    /* to fill up the table to make it aligned */
	    printf( "%c", 0 );

	}
}

static	void
create2w()
{
	int	s;
/* Patch wctbl according to qtmin[], qcmin[]. */
	for(s=0; s<3; s++) {
		wctbl[s].tmin += qtmin[s+1];
		wctbl[s].tmax += qtmin[s+1];
		wctbl[s].cmin += qcmin[s+1];
        	wctbl[s].cmax += qcmin[s+1];
	}
	if (fwrite(wctbl,(sizeof (struct _wctype)) * 3,1,stdout) == NULL) {
		(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", "tablename1", strerror(errno));
		exit(1);
	}
	for (s = 0; s < 3; s++){
		if (cnt_index[s] != 0) 
			if (fwrite(&index[s][0],cnt_index[s], 1, stdout) == NULL) {
				(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", "tablename1", strerror(errno));
				exit(1);
		}
		if (cnt_type[s] != 0)
			if (fwrite(&type[s][0],4,cnt_type[s], stdout) == NULL) {
				(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", "tablename1", strerror(errno));
				exit(1);
			}
		if (cnt_code[s] != 0)
			if (fwrite(&code[s][0],(sizeof((long) 0)),cnt_code[s], stdout) == NULL) {
				(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", "tablename1", strerror(errno));
				exit(1);
			}
	}
}
	
static void
create3()
{
	int	length;
	char	*numeric_name;

	if (freopen(tablename2, "w", stdout) == NULL) {
		(void)pfmt(stderr, MM_ERROR|MM_NOGET, "%s: %s\n", "tablename2", strerror(errno));
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
comment1(int format)
{
	if ( !o_flag  || ( o_flag && format == OLD_FORMAT))
	 if (codeset1 || codeset2 || codeset3){
		(void)printf("#include <ctype.h>\n");
		(void)printf("#include <widec.h>\n");
		(void)printf("#include <wctype.h>\n\n\n");
	 }
	 else
		(void)printf("#include <ctype.h>\n\n\n");
	(void)printf("\t/*\n");
	(void)printf("\t ************************************************\n");
	(void)pfmt(stdout, MM_NOSTD, ":36:\t *		%s  CHARACTER  SET                \n", tablename1);
	(void)printf("\t ************************************************\n");
	(void)printf("\t */\n\n");
	if ( format == NEW_FORMAT )
	  (void)pfmt(stdout, MM_NOSTD, ":49:\t/* NEW 32-bit character format */\n");
	(void)pfmt(stdout, MM_NOSTD, ":37:\t/* The first 257 characters are used to determine\n");
	(void)pfmt(stdout, MM_NOSTD, ":38:\t * the character class */\n\n");
}

static	void
comment2()
{
	(void)pfmt(stdout, MM_NOSTD, ":39:\n\n\t/* The next 257 characters are used for \n");
	(void)pfmt(stdout, MM_NOSTD, ":40:\t * upper-to-lower and lower-to-upper conversion */\n\n");
}

static void
comment3()
{
	(void)pfmt(stdout, MM_NOSTD, ":41:\n\n\t/*  CSWIDTH INFORMATION                           */\n");
	(void)printf("\t/*_____________________________________________   */\n");
	(void)pfmt(stdout, MM_NOSTD, ":42:\t/*                    byte width <> screen width  */\n");
	(void)printf("\t/* SUP1	  		     %d    |     %d         */\n",
		ctype[START_CSWIDTH], ctype[START_CSWIDTH + 3]);
	(void)printf("\t/* SUP2			     %d    |     %d         */\n",
		ctype[START_CSWIDTH + 1], ctype[START_CSWIDTH + 4]);
	(void)printf("\t/* SUP3			     %d    |     %d         */\n",
		ctype[START_CSWIDTH + 2], ctype[START_CSWIDTH + 5]);
	(void)pfmt(stdout, MM_NOSTD, ":43:\n\t/* MAXIMUM CHARACTER WIDTH        %d               */\n",
		ctype[START_CSWIDTH + 6]);
}

static	void
comment4()
{
	(void)pfmt(stdout, MM_NOSTD, ":44:\n\n\t/* The next entries point to wctype tables */\n");
	(void)pfmt(stdout, MM_NOSTD, ":45:\t/*          and have upper and lower limit */\n\n");
}

static	void
comment5()
{
	(void)pfmt(stdout, MM_NOSTD, ":46:\n\n\t/* The folowing table is used to determine\n");
	(void)pfmt(stdout, MM_NOSTD, ":47:\t * the character class for supplementary code sets */\n\n");
}

/*VARARGS*/
static	void
error(char *fmt, ...)
{
	va_list	args;

	va_start(args, fmt);
	(void)vpfmt(stderr, MM_ERROR, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	clean();
}

static	void
check_chrclass(cp)
unsigned char	*cp;
{
	if (chrclass_num != UL)
		if (*cp == '<' || *cp == '>')
			error(msg[9], input, chrclass_name, *cp);
		else
			;
	else
		if (*cp == '-')
			error(msg[9], input, chrclass_name, *cp);
		else
			;
}

static	void
clean()
{
	(void)signal(SIGINT, SIG_IGN);
	(void)unlink("wctype.c");
	(void)unlink(tablename1);
	(void)unlink(tablename2);
	exit(1);
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
		error(msg[13]);
	(void)strcpy(buf, (char *)p);
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
		if ( (ctype[START_CSWIDTH + suppl_set]) && !(ctype[START_CSWIDTH + suppl_set + 3]) )
			ctype[START_CSWIDTH + suppl_set + 3] = ctype[START_CSWIDTH + suppl_set];
		suppl_set++;
	}
	ctype[START_CSWIDTH + 6] = ctype[START_CSWIDTH + 1] > ctype[START_CSWIDTH + 2] ? ctype[START_CSWIDTH + 1] : ctype[START_CSWIDTH + 2];
	if (ctype[START_CSWIDTH + 6] > 0)
		++ctype[START_CSWIDTH + 6];
	if (ctype[START_CSWIDTH] > ctype[START_CSWIDTH + 6])
		ctype[START_CSWIDTH + 6] = ctype[START_CSWIDTH];
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

	p = clean_line(p);
	if ((len = strlen((char *)p)) == 0) 
		return(1);
	if (len > 1)
		return(0);
	ctype[START_NUMERIC + num_category - DECIMAL_POINT] = (int)p[0];
	return(1);
}

static	void
setmem(codeset)
	int	codeset;
{
	register i;
	if ((wctyp[codeset-1] = (unsigned *)(malloc(WSIZE * 4))) == NULL ||
	    (wconv[codeset-1] = (long *)(malloc(WSIZE * 4))) == NULL) 
		error(msg[13]);
	for (i=0; i<WSIZE; i++){
		wctyp[codeset-1][i] = 0;
		wconv[codeset-1][i] = 0xffff;
	}
}

/* When more than WSIZE chars are invloved, realloc the wctyp and wcconv */
/*	arrays according to tmin, tmax, cmin, and cmax from the input.   */
static	void
resetmem(cs)
	int     cs;
{                 
	register i;                          
	if((qtmax[cs] - qtmin[cs]) > WSIZE) {
	  if ((wctyp[cs-1] =      
	       (unsigned *)(realloc(wctyp[cs-1], (1 + qtmax[cs] - qtmin[cs]) * 4)))
	      == NULL)            
        	        error(msg[13]);
	  for (i=0; i<(qtmax[cs] - qtmin[cs]) ; i++) 
			wctyp[codeset-1][i] = 0;
	}
        if((qcmax[cs] - qcmin[cs]) > WSIZE) {        
	  if ((wconv[cs-1] =            
	       (long *)(realloc(wconv[cs-1], (1 + (qcmax[cs] - qcmin[cs])) * 4))) 
	      == NULL)                  
	                error(msg[13]);
	  for (i=0; i<(qcmax[cs] - qcmin[cs]) ; i++) 
			wconv[codeset-1][i] = 0xffff;
	}

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
		error(msg[14], input, chrclass_name);
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
			error(msg[14], input, chrclass_name);
		} else
			buf[j++] = *ptr;
		ptr++;
	} /* while() */
	ptr++;
	SKIPWHITE(ptr);
	if (*ptr != '\0') {
		error(msg[14], input, chrclass_name);
	}
	buf[j] = '\0';
	if ((ptr = strdup(buf)) == NULL) {
		error(msg[13]);
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

