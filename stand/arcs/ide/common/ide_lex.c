
/*
 * ide - lexical scanner
 *
 */

#include "libsc.h"
#include "uif.h"
#include "ide.h"
#include "ide_gram.h"

#ident "$Revision: 1.16 $"

#ifdef YYDEBUG
extern int yydebug;
#endif

/* yyinbuff is the incoming stream to parse */
char	yyinbuff[MAXLINELEN];
int	yyinbp  = 0;
int	yysave  = 0;
int	yysflag = 0;
size_t	yyinlen = 0;
int	ide_currline = 0; /* current line number */

#define yygetc()	(yysflag? yysflag=0, yysave: \
			yyinbp < yyinlen ? yyinbuff[yyinbp++] : ide_refill())
#define yyungetc(c)	(yysflag = yysave = c)

extern int yychar; /* current parser token */
extern int goteof; /* user type ctrl-d */
extern int badflag;/* after parsing is finished, don't execute the result */
extern int inrawcmd;/* parsing a diag command that doesn't want parsed args */

static int findkey(char *);

/* keyword table must be in lexical order */
#define NKEYWORDS 16
struct keyword_s {
	char	*name;
	int	type;
	} keytable[] = {
		"break",	KWBREAK,
		"case",		KWCASE,
		"continue",	KWCONTINUE,
		"default",	KWDEFAULT,
		"do",		KWDO,
		"else",		KWELSE,
		"fi",		KWFI,
		"for",		KWFOR,
		"forever",	KWFOREVER,
		"if",		KWIF,
		"print",	KWPRINT,
		"repeat",	KWREPEAT,
		"return",	KWRETURN,
		"switch",	KWSWITCH,
		"while",	KWWHILE,
		"",		0
		};

/*
 * called by parser whenever an error occurs
 *
 */
void
yyerror(char *s)
{
	extern char *ide_sname;

	if ( ! strcmp(s, "(lex: yyerror()) syntax error") ) {
		/*
		 * Let's see if we can't get a better error message than this.
		 */
		if ( yychar == 0 )
			printf("unexpected end-of-file");
		else	{
			printf("lex: yyerror() ELSE syntax error");
			if ( yychar >= ' ' && yychar < 127 )
				printf(" near '%c'", yychar );
			}
		if ( ide_sname )
			printf(" at line %d of %s", ide_currline+1, ide_sname);
		putchar('\n');
		}
	else
		printf("%s\n",s);

	badflag = 1; /* don't attempt to execute this statement */
	return;
}

/*
 * yylex - return next token from the input stream
 *
 * This routine gathers up the next token and returns it via the
 * global variable "yylval" and the return code.
 *
 */
int
yylex(void)
{
	int c;
	sym_t *psym;

#ifdef YYDEBUG
	yydebug = 1;
#endif

	/* skip white space */
	while( (c = yygetc()) == ' ' || c == '\t' )
		;
	
	if ( c == '\n' ) {
		ide_currline++;
		return c;
		}

	if ( c == EOF ) {
		goteof = 1;
		return EOF;
		}
	
	if ( inrawcmd && c != ';' && c != '"' && c != ')' && c != '`') {
		char buf[MAXIDENT+1];
		char *bp = buf;
		int bc = 0;
		while ( c != ' ' && c != '\t' && c != '\n' && c != ';' ) {
			*bp++ = c;
			c = yygetc();
			if ( ++bc == MAXIDENT )
				break;
			}
		*bp = '\0';
		yyungetc(c);
		yylval.strval = makestr(buf);
		return STRCONST;
		}

	/* skip shell-style comment */
	if ( c == '#' ) {
		while ( (c = yygetc()) != '\n' && c != EOF )
			;
		ide_currline++;
		return '\n';
		}
	
	/* skip C-style comment */
	if ( c == '/' )
		if ( (c = yygetc()) == '*' ) {
			while ( (c = yygetc()) != EOF )
				if ( c == '\n' )
					ide_currline++;
				else
				if ( c == '*' && (c = yygetc()) == '/' )
					break;
			return '\n';
			}
		else	{
			yyungetc(c);
			c = '/';
			}
	
	/* unquoted word */
	if ( isalpha(c) ) {
		char buf[MAXIDENT+1];
		char *bp = buf;
		int bc = 0;
		while ( (isalnum(c) || c == '_') && bc < MAXIDENT ) {
			*bp++ = c;
			bc++;
			c = yygetc();
			}
		if ( ! c )
			printf("unexpected null in input\n");
		yyungetc(c);
		*bp = '\0';
		if ( bc = findkey(buf) )
			return bc;
		else
		if ( ! (psym = findsym(buf)) ) {
			yylval.strval = makestr(buf);
			return IDENT;
			}
		yylval.psym = psym;
		switch( psym->sym_type ) {
			case SYM_GLOBAL:
			case SYM_VAR:
				return VAR;
			case SYM_CMD:
				return CMD;
			default:
	printf("internal error, lex: bad sym type (%d)\n", psym->sym_type);
	yyerror("");
				return EOF;
			}
		}
	
	if ( isdigit(c) || c == '-' ) {
		int val = 0;
		int base = 10;
		int sign;
		if ( c == '-' ) {
			c = yygetc();
			if ( ! isdigit(c) ) {
				yyungetc(c);
				c = '-';
				goto notanumber;
				}
			sign = -1;
			}
		else
			sign =  1;
		if ( c == '0' ) {
			base = 8;
			c = yygetc();
			if ( c == 'x' || c == 'X' ) {
				base = 16;
				c = yygetc();
				}
			else
			if ( c == 'b' || c == 'B' ) {
				base = 2;
				c = yygetc();
				}
			}
		while ( isxdigit(c) ) {
			if (base == 16 && c >= 'a' && c <= 'f' )
				c = (c - 'a')+10;
			else
			if (base == 16 && c >= 'A' && c <= 'F' )
				c = (c - 'A')+10;
			else
			if ( (c - '0') < base )
				c -= '0';
			else
				break;
			val = (val * base) + c;
			c = yygetc();
			}
		yyungetc(c);
		yylval.intval = val * sign;
		return INTCONST;
		}
	notanumber:
	
	/* normally quoted string */
	if ( c == '"' ) {
		char buf[MAXSTRLEN+1];
		char *bp = buf;
		char bc = 0;
		c = yygetc();
		while ( c != '"' && bc < MAXSTRLEN ) {
			if ( c == '\\' )
				switch( c = yygetc() ) {
					case 'n':
						c = '\n';
						break;
					case 'r':
						c = '\r';
						break;
					case 'b':
						c = '\b';
						break;
					case '0': case '1': case '2':
					case '3': case '4': case '5':
					case '6': case '7':
						{
						int v;
						c -= '0';
						v = yygetc();
						if ( v >= '0' && v <= '7' ) {
							c = (c * 8)+(v-'0');
							v = yygetc();
							}
						else	{
							yyungetc(v);
							break;
							}
						if ( v >= '0' && v <= '7' )
							c = (c * 8)+(v-'0');
						else
							yyungetc(v);
						}
					}
			*bp++ = c;
			bc++;
			c = yygetc();
			}
		*bp = '\0';
		yylval.strval = makestr(buf);
		return STRCONST;
		}
	
	/* now a whole slew of special two character tokens */
	if ( c == '!' ) {
		if ( (c = yygetc()) == '=' )
			return NE;
		else
			yyungetc(c);
		return '!';
		}
	else
	if ( c == '>' ) {
		if ( (c = yygetc()) == '=' )
			return GE;
		else
		if ( c == '>' ) {
			if ( (c = yygetc()) == '=' )
				return SHRASS;
			else
				yyungetc(c);
			return SHR;
			}
		else
			yyungetc(c);
		return '>';
		}
	else
	if ( c == '<' ) {
		if ( (c = yygetc()) == '=' )
			return LE;
		else
		if ( c == '<' ) {
			if ( (c = yygetc()) == '=' )
				return SHLASS;
			else
				yyungetc(c);
			return SHL;
			}
		else
			yyungetc(c);
		return '<';
		}
	else
	if ( c == '=' ) {
		if ( (c = yygetc()) == '=' )
			return EQ;
		else
			yyungetc(c);
		return '=';
		}
	else
	if ( c == '|' ) {
		if ( (c = yygetc()) == '|' )
			return LO;
		else
		if ( c == '=' )
			return ORASS;
		else
			yyungetc(c);
		return '|';
		}
	else
	if ( c == '&' ) {
		if ( (c = yygetc()) == '&' )
			return LA;
		else
		if ( c == '=' )
			return ANDASS;
		else
			yyungetc(c);
		return '&';
		}
	else
	if ( c == '^' ) {
		if ( ( c = yygetc()) == '=' )
			return XORASS;
		else
			yyungetc(c);
		return '^';
		}
	else
	if ( c == '+' ) {
		if ( (c = yygetc()) == '+' )
			return INCR;
		else
		if ( c == '=' )
			return ADDASS;
		else
			yyungetc(c);
		return '+';
		}
	else
	if ( c == '-' ) {
		if ( (c = yygetc()) == '-' )
			return DECR;
		else
		if ( c == '=' )
			return SUBASS;
		else
			yyungetc(c);
		return '-';
		}
	else
	if ( c == '*' ) {
		if ( (c = yygetc()) == '=' )
			return MULASS;
		else
			yyungetc(c);
		return '*';
		}
	else
	if ( c == '/' ) {
		if ( (c = yygetc()) == '=' )
			return DIVASS;
		else
			yyungetc(c);
		return '/';
		}
	else
	if ( c == '%' ) {
		if ( (c = yygetc()) == '=' )
			return MODASS;
		else
			yyungetc(c);
		return '%';
		}

	return c;
}
/*
 * Binary search for keyword s
 *
 */
int
findkey(char *s)
{
	int high, low, test, i;
	struct keyword_s *pkey;

	low  = 0;
	high = NKEYWORDS + 1;

	do	{
		test = ((high - low) / 2) + low;
		pkey = &keytable[test - 1];

		if ( ! (i = (int)*pkey->name - (int)*s) )
			i = strcmp( pkey->name, s );
		
		if ( i == 0 )
			return pkey->type;
		
		if ( i < 0 )
			low = test;
		else
			high= test;

		} while ( low+1 < high );
	
	return 0;
}
