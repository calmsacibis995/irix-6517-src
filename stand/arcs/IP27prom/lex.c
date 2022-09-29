#include <sys/types.h>
#include <stdlib.h>

#include <hwreg.h>

#include "cmd.h"
#include "libc.h"
#include "lex.h"

#define NEXTC()		( *ip ? *ip++ : 0)
#define DONE(c)		{ yylval->x = t = (c); goto done; }
#define DONESTR(c)	{ t = (c); goto done; }
#define DONEVAL(c, v)	{ t = (c); yylval->x = (v); goto done; }
#define DONEREG(r)	{ yylval->gpr.reg = (r); goto donereg; }

int parse_yylex(parse_YYSTYPE *yylval, void *lex_data)
{
    lex_data_t	       *ld	= lex_data;
    char		text[LEX_STRING_MAX];
    char	       *ip, *s;
    int			t, c, peekc, i;
    int			s0, s1, s2, s3;

    ip = ld->ptr;

    if (ld->pushback < 0)
	DONE(0);			/* Already returned parse_EOL */

    if (ld->pushback) {
	c = ld->pushback;
	ld->pushback = 0;
    } else
	c = NEXTC();

    while (c == ' ' || c == '\t' || c == '\n')
	c = NEXTC();

    if (c == 0 || c == '#') {		/* EOL or comment */
	ld->pushback = -1;
	DONE(parse_EOL);
    }

    ld->last = ip - 1;

    if (c == '"' || c == '\'') {	/* Quoted string or char const */
	s0 = c;
	for (i = 0; i < LEX_STRING_MAX; i++) {
	    if ((c = NEXTC()) == 0 || c == s0)
		break;
	    if (c == '\\' && (c = NEXTC()) != 0) {
		switch (c) {
		case 'b': c = 8;  break;
		case 't': c = 9;  break;
		case 'n': c = 10; break;
		case 'f': c = 12; break;
		case 'r': c = 13; break;
		default:
		    if (isodigit(c)) {
			c -= '0';
			if ((t = NEXTC()) != 0) {
			    if (isodigit(t)) {
				c = c * 8 + (t - '0');
				if ((t = NEXTC()) != 0) {
				    if (isodigit(t))
					c = c * 8 + (t - '0');
				    else
					ip--;
				}
			    } else
				ip--;
			}
		    }
		}
	    }

	    text[i] = c;
	}

	if (s0 == '\'') {
	    __uint64_t		val	= 0;

	    if (i > 8)
		DONE(parse_BAD);

	    for (t = 0; t < i; t++)
		val = val << 8 | (__uint64_t) text[t];

	    DONEVAL(parse_CONSTANT, val);
	} else {
	    if (i == LEX_STRING_MAX)
		DONE(parse_BAD);
	    
	    text[i] = 0;
	    
	    strcpy(yylval->s, text);
	    
	    DONESTR(parse_STRING);
	}
    }

    peekc = NEXTC();

    if (c == '<' && peekc == '<') DONE(parse_LSHIFT);
    if (c == '<' && peekc == '=') DONE(parse_LTEQUAL);
    if (c == '>' && peekc == '>') DONE(parse_RSHIFT);
    if (c == '>' && peekc == '=') DONE(parse_GTEQUAL);
    if (c == '=' && peekc == '=') DONE(parse_ISEQUAL);
    if (c == '!' && peekc == '=') DONE(parse_NOTEQUAL);
    if (c == '+' && peekc == '+') DONE(parse_INCR);
    if (c == '-' && peekc == '-') DONE(parse_DECR);
    if (c == '&' && peekc == '&') DONE(parse_ANDAND);
    if (c == '|' && peekc == '|') DONE(parse_OROR);
    if (c == '$' && peekc == 'f') DONE(parse_FPPREF);

    if (! isalnum(c) && c != '_') {
	ld->pushback = peekc;
	DONE(c);
    }

    text[0] = c;
    c = peekc;

    for (i = 1; i < LEX_STRING_MAX; ) {
	if (! isalnum(c) && c != '_')
	    break;
	text[i++] = c;
	c = NEXTC();
    }

    if (i == LEX_STRING_MAX) {
	ld->word_too_long = 1;
	ld->pushback = -1;		/* Pretend EOL */
	DONE(parse_BAD);
    }

    text[i] = 0;

    if (i == 1 && c == ':') {
	switch (text[0]) {
	case 'h':
	    DONE(parse_MOD_HSPEC);
	case 'i':
	    DONE(parse_MOD_IO);
	case 'm':
	    DONE(parse_MOD_MSPEC);
	case 'u':
	    DONE(parse_MOD_UNCAC);
	case 'c':
	    DONE(parse_MOD_CAC);
	case 'p':
	    DONE(parse_MOD_PHYS);
	case 's':
	    DONE(parse_MOD_SIGNEXT);
	case 'n':
	    DONE(parse_MOD_NODE);
	case 'w':
	    DONE(parse_MOD_WIDGET);
	case 'b':
	    DONE(parse_MOD_BANK);
	case 'L':
	    DONE(parse_MOD_DIRL);
	case 'H':
	    DONE(parse_MOD_DIRH);
	case 'P':
	    DONE(parse_MOD_PROT);
	case 'E':
	    DONE(parse_MOD_ECC);
	default:
	    DONE(parse_BAD);
	}
    }

    ld->pushback = c;

    if (isdigit(text[0])) {
	char		*ends;
	__uint64_t	val	= strtoull(text, &ends, 0);

	switch (*ends) {
	case 'G':
	case 'g':
	    val <<= 10;
	case 'M':
	case 'm':
	    val <<= 10;
	case 'K':
	case 'k':
	    val <<= 10;
	    ends++;
	}

	if (*ends == 0) {
	    DONEVAL(parse_CONSTANT, val);
	} else {
	    DONE(parse_BAD);
	}
    }

    s0 = text[0];
    if ((s1 = text[1]) != 0)
	if ((s2 = text[2]) != 0)
	    s3 = text[3];

    /* Decode n64 ABI register names */

    switch (s0) {
    case 'A':
	if (s1 == 'T' && s2 == 0)
	    DONEREG(1);
	break;
    case 'v':
	if (s1 >= '0' && s1 <= '1' && s2 == 0)
	    DONEREG(s1 - '0' + 2);
	break;
    case 'a':
	if (s1 >= '0' && s1 <= '7' && s2 == 0)
	    DONEREG(s1 - '0' + 4);
	if (s1 == 't' && s2 == 0)
	    DONEREG(1);
	break;
    case 't':
	if (s1 == 'a' && s2 >= '0' && s2 <= '3' && s3 == 0)
	    DONEREG(s2 - '0' + 8);
	if (s1 >= '0' && s1 <= '3' && s2 == 0)
	    DONEREG(s1 - '0' + 12);
	if (s1 >= '8' && s1 <= '9' && s2 == 0)
	    DONEREG(s1 - '8' + 24);
	break;
    case 's':
	if (s1 >= '0' && s1 <= '7' && s2 == 0)
	    DONEREG(s1 - '0' + 16);
	if (s1 == 'p' && s2 == 0)
	    DONEREG(29);
	if (s1 == '8' && s2 == 0)
	    DONEREG(30);
	break;
    case 'j':
	if (s1 == 'p' && s2 == 0)
	    DONEREG(25);
	break;
    case 'k':
	if (s1 >= '0' && s1 <= '1' && s2 == 0)
	    DONEREG(s1 - '0' + 26);
	break;
    case 'g':
	if (s1 == 'p' && s2 == 0)
	    DONEREG(28);
	break;
    case 'f':
	if (s1 == 'p' && s2 == 0)
	    DONEREG(30);
	break;
    case 'r':
	if (s1 == 'a' && s2 == 0)
	    DONEREG(31);
	if (s1 >= '0' && s1 <= '9') {
	    if (s2 == 0)
		DONEREG(s1 - '0');
	    if (s2 >= '0' && s2 <= '9' && s3 == 0 &&
		(i = (s1 - '0') * 10 + (s2 - '0')) < 32)
		DONEREG(i);
	}
	break;
    case 'p':
	if (s1 == 'c' && s2 == 0)
	    DONEREG(32);
	break;
    }

    strncpy(yylval->s, text, sizeof (yylval->s) - 1);

    if ((i = cmd_name2token(text)) != 0) {
	DONE(i);
    } else {
	DONESTR(parse_STRING);
    }

    /* NOTREACHED */

 donereg:

    strcpy(yylval->gpr.name, text);
    t = parse_GPR;

 done:

#ifdef LEXDEBUG
    switch (t) {
    case 0:
	printf("End of input\n");
	break;
    case parse_EOL:
	printf("End of line\n");
	break;
    case parse_CONSTANT:
	printf("Constant: 0x%016llx\n", yylval->x);
	break;
    case parse_GPR:
	printf("GPR: %lld\n", yylval->x);
	break;
    case parse_STRING:
	printf("String: %s\n", yylval->s);
	break;
    default:
	printf("Token: %d\n", t);
	break;
    }
#endif

    ld->ptr = ip;

    return t;
}

int parse_yylex_init(lex_data_t *ld, char *s)
{
    ld->ptr = s;
    ld->buf = s;
    ld->last = s;
    ld->pushback = 0;
    ld->word_too_long = 0;

    return 0;
}
