/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

%{

# include "lboot.h"
# include "mkboot.h"

struct	flag {
	int	bits;
	short	sema;
};

static struct flag *cvtflag(register char *string);
static int string_table_add(offset *,char *);
static int admin_pair_add(char *,char *);		


char	any_error = FALSE;
%}


%union	{
	char			word[FILENAME_MAX+1];
	char			*string;
	struct tnode		*tptr;
	struct format		*fptr;
	unsigned long		number;
	struct flag		flag;
	}

%token	<word>		HWORD
%token	<number>	NUMBER
%token	<string>	STRING DWORD ROUTINE

%type	<word>		header prefix
%type	<string>	routine
%type	<flag>		flag
%type	<number>	master
%type	<number>	software ndevice dependencies admin data type

%left	'+' '-'
%left	'*' '/'

%%

%{
%}

master		:	header admin data
				{
				/* master.magic = MMAGIC; */
				master.nadmins = (short)$2;
				}
		|	header data
				{
				/* master.magic = MMAGIC; */
				master.nadmins = 0;
				}
		;

header		:	flag prefix software ndevice dependencies
				{
				master.flag = $1.bits;
				master.sema = $1.sema;
				strncat( strcpy(master.prefix,""), $2, sizeof(master.prefix)-1 );
				master.ndev = (unsigned char)$4;
				master.ndep = (short)$5;
				}
		|	flag prefix software ndevice
				{
				master.flag = $1.bits;
				master.sema = $1.sema;
				strncat( strcpy(master.prefix,""), $2, sizeof(master.prefix)-1 );
				master.ndev = (unsigned char)$4;
				master.ndep = 0;
				}
		|	flag prefix software
				{
				master.flag = $1.bits;
				master.sema = $1.sema;
				strncat( strcpy(master.prefix,""), $2, sizeof(master.prefix)-1 );
				master.ndev = 0;
				master.ndep = 0;
				}
		|	flag prefix
				{
				master.flag = $1.bits;
				master.sema = $1.sema;
				strncat( strcpy(master.prefix,""), $2, sizeof(master.prefix)-1 );
				master.soft[0] = 0;
				master.nsoft = 0;
				master.ndev = 0;
				master.ndep = 0;
				}
		|	flag
				{
				master.flag = $1.bits;
				master.sema = $1.sema;
				master.prefix[0] = '\0';
				master.soft[0] = 0;
				master.nsoft = 0;
				master.ndev = 0;
				master.ndep = 0;
				}
		;

flag		:	HWORD
				{
				struct flag *f;

				if ( (f=cvtflag($1)) == NULL )
					{
					yyerror ( "cvtflag returned NULL" );
					YYERROR;
					}

				$$ = *f;
				}
		|	'-'
				{
				$$ = *cvtflag("none");
				}
		;

prefix		:	'-'
				{
				strcpy( $$, "" );
				}
		|	HWORD
				{
				if ( strlen($1) >= sizeof(master.prefix) )
					yyerror( "prefix \"%s\" truncated to %d characters", $1, sizeof(master.prefix)-1 );
				}
		;

software	:	software ',' NUMBER
				{
				if (master.nsoft >= DRV_MAXMAJORS)
					{
					yyerror( "too many major numbers" );
					YYERROR;
					}

				master.soft[master.nsoft] = (short)$3;
				master.nsoft++;
				}
		|	NUMBER
				{
				master.soft[master.nsoft] = (short)$1;
				master.nsoft++;
				}
		|	'-'
				{
				master.nsoft = 0;
				}
		;

ndevice		:	'-'
				{
				$$ = 0;
				}
		|	NUMBER
		;


/*
 * <dependencies> will maintain the total number of struct depend elements
 */
dependencies	:	dependencies ',' HWORD
				{
				if ( ndepend-depend >= MAXDEP )
					{
					yyerror( "too many dependencies" );
					YYERROR;
					}

				if ( nstring-string >= MAXSTRING-strlen($3)-1 )
					{
					yyerror( "string table overflow" );
					YYERROR;
					}

				ndepend->name = Offset( strcpy(nstring,($3)), string );
				nstring += strlen(nstring) + 1;
				++ndepend;
				$$ = $1 + 1;
				}
		|	HWORD
				{
				if ( nstring-string >= MAXSTRING-strlen($1)-1 )
					{
					yyerror( "string table overflow" );
					YYERROR;
					}

				ndepend->name = Offset( strcpy(nstring,($1)), string );
				nstring += strlen(nstring) + 1;

				++ndepend;
				$$ = 1;
				}
		;
/*
 *	The following 4 rules parse the driver administration
 *	hints section. It is basically organized as follows.
 *		<parameter1>	<value1>
 *		<parameter2>	<value2>
 *			:
 *			:
 *	NOTE: There should be no leading white space before the 
 *	parameter name.	
 *	
 */
admin		:	admin '+' HWORD NUMBER
				{
				char	num_s[20];
				sprintf(num_s,"%d",(int)($4));
				if (admin_pair_add($3,num_s))
					YYERROR;
				$$ = $1 + 1;
				}
		|	admin '+' HWORD HWORD
				{
				if (admin_pair_add($3,$4))
					YYERROR;
				$$ = $1 + 1;
				}
		|	'+' HWORD NUMBER
				{
				char	num_s[20];
				sprintf(num_s,"%d",(int)($3));
				if (admin_pair_add($2,num_s))
					YYERROR;
				$$ = 1;	
				}
		|	'+' HWORD HWORD
				{
				if (admin_pair_add($2,$3))
					YYERROR;
				$$ = 1;	
				}
		;
/*
 * <data> the data section follows the FLAG PREFIX ... line and ends
 * with $$$. It includes either routine stubs, such as:
 *
 *	stub_routine(){}
 *
 * or strings, such as:
 *
 *	"pci/vendor/1234/device/5678"
 *	"pci/mydevicename"
 *
 * Strings are used to match against the hwgraph to determine whether
 * to load the driver/module or not.
 *
 * Both stubs and strings need to be preceeded by white space - either
 * a tab or space.
 *
 */
data		:	/* empty */
				{
				master.nrtn = 0;
				master.naliases = 0;
				}
		|	data STRING
				{
				if ( nalias-alias >= MAXALIASES )
					{
					yyerror( "too many aliases" );
					YYERROR;
					}
				if ( nstring-string >= MAXSTRING-strlen($2)-1 )
					{
					yyerror( "string table overflow" );
					YYERROR;
					}

				nalias->str = Offset( strcpy(nstring,($2)), string );
				nstring += strlen(nstring) + 1;
				++master.naliases;
				++nalias;
				}
		|	data routine
				{
				++master.nrtn;
				++nroutine;
				}
		;
routine		:	ROUTINE '{' type '}'
				{
				if ( nroutine-routine >= MAXRTN )
					{
					yyerror( "too many routine definitions" );
					YYERROR;
					}

				nroutine->id = (char)$3;
				nroutine->name = Offset( $1, string );
				}
		;

/*
 * <type> is the routine type: RNULL, RNOSYS, RNODEV, etc.
 */
type		:	/* empty */
				{
				$$ = RNOTHING;
				}
		|	DWORD
				{
				static struct
					{
					char	*name;
					int	flag;
					}
					table[] ={
						{ "nulldev", RNULL },
						{ "nosys", RNOSYS },
						{ "nodev", RNODEV },
						{ "true", RTRUE },
						{ "false", RFALSE },
						{ "fsnull", RFSNULL },
						{ "fsstray", RFSSTRAY },
						{ "nopkg", RNOPKG},
						{ "noreach", RNOREACH},
						0
						};
				int i;

				lcase( $1 );

				for( i=0; table[i].name; ++i )
					if ( 0 == strcmp(table[i].name,$1) )
						break;

				if ( table[i].name )
					$$ = table[i].flag;
				else
					{
					yyerror( "unknown routine type \"%s\"", $1 );
					YYERROR;
					}
				}
		;

%% /*start of programs */



/*
 * yyfatal( message, ...)
 *
 * Printf for fatal error messages; error message is:
 *
 *	mkboot: /etc/master: line 0, <message>
 */
 /*VARARGS1*/
void
yyfatal(char * message, ...)
{
	extern char master_file[];
	extern char *Argv;
	extern int yylineno;
	char buffer[BUFSIZ];
	va_list ap;

	va_start(ap, message);
	vsprintf( buffer, message, ap );
	fprintf( stderr, "%s: %s: line %d, %s\n",
			Argv, master_file, yylineno, buffer );
	va_end(ap);

	if ( jmpbuf != NULL )
		longjmp( *jmpbuf, -1 );
	else
		myexit(1);
}



/*
 * yyerror( message, ... )
 *
 * Printf for warning error messages; error message is:
 *
 *	mkboot: /etc/master: line 0, <message>
 */
 /*VARARGS1*/
 void
yyerror(char *message, ...)
{
	extern char master_file[];
	extern char *Argv;
	extern int yylineno;
	char buffer[BUFSIZ];
	va_list ap;

	any_error = TRUE;

	va_start(ap, message);
	vsprintf( buffer, message, ap );
	va_end(ap);

	fprintf( stderr, "%s: %s: line %d, %s\n",
			Argv, master_file, yylineno, buffer );
}



/*
 * warn( message, ... )
 *
 * Printf for warning error messages; error message is:
 *
 *	mkboot: <message>
 */
 /*VARARGS1*/
void
warn(char *message, ...)
{
	extern	char	*Argv;
	char buffer[256];
	va_list ap;

	va_start(ap, message);
	vsprintf( buffer, message, ap );
	va_end(ap);

	fprintf( stderr, "%s: %s\n", Argv, buffer );

}



/*
 * This routine converts the alphabetic string of flags
 * into the correct bit flags.
 */
static struct flag *
cvtflag(register char *string)
{
	register  char  	c;
	char			*f = string;
	static struct flag	flag;

	flag.bits = 0;
	flag.sema = 0;

	if (strcmp(f,"none") == 0)
		return( &flag );

	while(c = *f++)
		switch (c) {
		case 'D':
			flag.bits |= DLDREG;
			break;
		case 'N':
			flag.bits |= NOUNLD;
			break;
		case 'e':
			flag.bits |= ENET;
			break;
		case 'k':
			flag.bits |= KOBJECT;
			break;
		case 'T':
			flag.bits |= DYNSYM;
			break;
		case 'R':
			flag.bits |= DYNREG;
			break;
		case 'd':
			flag.bits |= DYNAMIC;
			break;
		case 'o':
			flag.bits |= ONCE;
			break;
		case 'O':
			flag.bits |= OLD;
			break;
		case 'r':
			flag.bits |= REQ;
			break;
		case 'b':
			flag.bits |= BLOCK;
			break;
		case 'c':
			flag.bits |= CHAR;
			break;
		case 'a':
			flag.bits |= REQADDR;
			break;
		case 's':
			flag.bits |= SOFT;
			break;
		case 't':
			flag.bits |= TTYS;
			break;
		case 'x':
			flag.bits |= NOTADRV;
			break;
		case 'f':
			flag.bits |= FUNDRV;
			break;
		case 'm':
			flag.bits |= FUNMOD;
			break;
		case 'j':
			flag.bits |= FSTYP;
			break;
		case 'w':
			flag.bits |= WBACK;
			break;
		case 'u':
			flag.bits |= STUB;
			break;
		case 'n':
			flag.sema |= S_NONE;
			break;
		case 'l':
			flag.sema |= S_MAJOR;
			yyerror( "device flag \"l\" no longer supported" );
			myexit(1);
		case 'p':
			flag.sema |= S_PROC;
			break;
#if MIN_VERSION < VERSION_NONET
		case 'q':
			if (system_version < VERSION_NONET) {
				flag.sema |= S_NET;
			} else {
				warn(
			    "Warning: obsolete network processor flag \"q\"");
			}
			break;
#endif
		default:
			yyerror( "illegal device flags \"%s\"", string );
			return( NULL );
		}

	if (!flag.sema)
		flag.sema = S_PROC;

	return( &flag );
}

/*
 * string_table_add
 *	Add a the string to the global string table and note the
 *	offset.
 *	Returns -1 on string table overflow.
 *		0  otherwise.
 */
int
string_table_add(offset *off,char *str)
{
	if ( nstring-string >= MAXSTRING-strlen(str)-1 ) {
		yyerror( "string table overflow" );
		return(-1);
	}
	*off = Offset( strcpy(nstring,str), string );
	nstring += strlen(nstring) + 1;
	return(0);
}
/*
 * admin_pair_add
 *	Given an administrator hint <name,value> pair
 *	store them in the global string table and
 *	remember their offsets in a admin pair table.
 *	Returns -1 if the admin pair table limit is 
 *		   exceeded
 *		0  otherwise
 */
int
admin_pair_add(char *name_s,char *val_s)
{
	if ( nadmin-admin >= MAXADMIN )	{
		yyerror( "too many admins" );
		return(-1);
	}
	string_table_add(&nadmin->admin_name,name_s);
	string_table_add(&nadmin->admin_val,val_s);
	++master.nadmins;
	++nadmin;
	return(0);
}

# include "xmkboot.c"


/*
 * reset lex to its initial state prior to each file
 */
void
lexinit(void)
	{
	BEGIN INITIAL;
	NLSTATE;
	yymorfg = 0;
	yysptr = yysbuf;
	}
