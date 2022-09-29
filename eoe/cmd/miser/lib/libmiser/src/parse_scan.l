/*
 * FILE: eoe/cmd/miser/lib/libmiser/src/parse_scan.l
 *
 * DESCRIPTION:
 *	Implements miser command line option parsing.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

%{
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/miser_public.h>
#include "libmiser.h"
#include "parse_gram.h"
#include <syslog.h>

static char *		_yyfname;
static uint64_t		_yylnum;
static int16_t		_yytype;
static int		_yysel;
static int		_yysym;
static int		_save;
static FILE *		_save_yyin;
static YY_BUFFER_STATE	_save_yybuf;
static char *		_save_yyfname;
static uint64_t		_save_yylnum;


int		yywrap		(void)		{ return 1; }
uint64_t	parse_lnum	(void)		{ return _yylnum; }
void		parse_select	(int16_t type)	{ _yytype = type; _yysel = 1; }
int16_t		parse_type	(void)		{ return _yytype; }


void
parse_error(const char *fmt, ...)
{
	va_list		args;
	char *		nlp;
	char 		buf[1024];

	if (_yyfname) {
		va_start(args, fmt);
		sprintf(buf, "Failed to parse  [%s, %d]: %s",
				 _yyfname, _yylnum, fmt);
		merror_v(buf, args);
		va_end(args);

		if (G_syslog) {
			return;
		}

		merror_hdr("[%s, %d] ", _yyfname, _yylnum);

		if (yytext) {   /* cheezy attempt at better error output */
			int     hdrsz;
			int     hdrln;

			if (nlp = strchr(yytext, '\n')) {
				*nlp = '\0';
			}

			merror("\"... %s\"", yytext);

			hdrsz = strlen(_yyfname) + strlen("[, ] \"... ") + 1;

			for (hdrln = _yylnum; hdrln /= 10; hdrsz++);

			while (hdrsz--) {
				merror_hdr(" ");
			}

			merror_hdr("^ ");

		} /* if */

	} /* if */

	va_start(args, fmt);
	merror_v(fmt, args);
	va_end(args);

} /* parse_error */


int
parse_open(const char *fname)
{
	if (!(yyin = fopen(fname, "r"))) {
		parse_error("failed to open '%s'", fname);
		return 0;	/* error */
	}

	_yyfname = strdup(fname);

	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));

	return 1;	/* success */

} /* parse_open */


void
parse_close(void)
{
	if (_yyfname) {
		free(_yyfname);
		_yyfname = 0;
		yy_delete_buffer(YY_CURRENT_BUFFER);

		if (yyin) {
			fclose(yyin);
		}
	} 

} /* parse_close */


void
parse_ctxt_save(void)
{
	ASSERT( _save == 0 );

	_save_yyin = yyin;
	_save_yybuf = YY_CURRENT_BUFFER;
	_save_yyfname = _yyfname;
	_save_yylnum = _yylnum;
	_save = 1;

} /* parse_ctxt_save */


void
parse_ctxt_restore(void)
{
	ASSERT( _save == 1 );

	yyin = _save_yyin;
	yy_switch_to_buffer(_save_yybuf);
	_yyfname = _save_yyfname;
	_yylnum = _save_yylnum;
	_save = 0;

} /* parse_ctxt_restore */


#define	NBEGIN(_sym)	_yysym = _sym;
%}

%x	SELECT STRING QDEFIN QDEFSW
%%

	if (_yysel) {
		BEGIN(SELECT);
	} else {
		BEGIN(_yysym);
	}


<SELECT>.		{
			_yysel = 0;
			unput(yytext[0]);
			NBEGIN(INITIAL);

			switch(_yytype) {

			case PARSE_JSUB:
				return SEL_JSUB;

			case PARSE_QMOV:
				return SEL_QMOV;

			case PARSE_QDEF:
				return SEL_QDEF;

			default:
				break;
			}

			return TOK_ERR;
		}

<*>[ \t]+			{ }
<*>#[^\n]*			{ }
<*>[\n]				{ _yylnum++; }


[sS][eE][gG][mM][eE][nN][tT]			{ return KWD_SEGMENT;	}
[mM][uU][lL][tT][iI][pP][lL][eE]		{ return KWD_MULTIPLE;	}
[pP][rR][iI][oO][rR][iI][tT][yY]		{ return KWD_PRIORITY;	}
[sS][tT][aA][tT][iI][cC]			{ return KWD_STATIC;	}
[eE][xX][cC][eE][pP][tT][iI][oO][nN]		{ return KWD_EXCEPT;	}
[kK][iI][lL][lL]				{ return KWD_KILL;	}
[wW][eE][iI][gG][hH][tT][lL][eE][sS][sS]	{ return KWD_WTLESS;	}
[tT][iI][mM][eE]		{ NBEGIN(STRING); return KWD_TIME;	}
[cC][tT][iI][mM][eE]		{ NBEGIN(STRING); return KWD_CTIME;	}
[sS][tT][aA][rR][tT]		{ NBEGIN(STRING); return KWD_START;	}
[eE][nN][dD]			{ NBEGIN(STRING); return KWD_END;	}
[mM][eE][mM][oO][rR][yY]	{ NBEGIN(STRING); return KWD_MEMORY;	}
[nN][cC][pP][uU][sS]				{ return KWD_NCPUS;	}
[pP][oO][lL][iI][cC][yY]	{ NBEGIN(STRING); return KWD_POLICY;	}
[qQ][uU][aA][nN][tT][uU][mM]			{ return KWD_QUANTUM;	}
[nN][sS][eE][gG]				{ return KWD_NSEG;	}
[qQ][uU][eE][uU][eE]		{ NBEGIN(QDEFIN); return KWD_QUEUE;	}

<STRING>[^ \t\n]*	{
			yylval.v_str = yytext;
			NBEGIN(INITIAL);
			return TOK_STR;
		}

<QDEFIN>[^ \t\n]*	{
			yylval.v_int = miser_qid(yytext);
			NBEGIN(QDEFSW);
			return TOK_INT;
		}

<QDEFSW>[^ \t\n]*	{
			yylval.v_str = strdup(yytext);
			parse_ctxt_save();

			if (!parse_open(yylval.v_str)) {
				parse_ctxt_restore();
				return TOK_ERR;
			}

			NBEGIN(INITIAL);

			return TOK_STR;
		}

[0-9]+		{ yylval.v_int = atol(yytext); return TOK_INT;		}

<*><<EOF>>		{
			if (_save) {
				yyin = 0; /* XXX we keep open for now */
				parse_close();
				parse_ctxt_restore();
			}
			else {
				yyterminate();
			}
		}

<*>.		{ unput(yytext[0]); return TOK_ERR;			}
