//____________________________________________________________________________
//
// Copyright (C) 1995 Silicon Graphics, Inc
//
// All Rights Reserved Worldwide.
//
//____________________________________________________________________________
//
// FILE NAME :          loc.l
//
// DESCRIPTION :        source for scanner generator used to tokenize
//                      POSIX-format locale definition input file.
//
//____________________________________________________________________________

%{
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include "tables.h"
#include "collate.h"
#include "y.tab.h"
#include "localedef.h"

static int rval = 0;
static struct keyword {
    char *str;
    int  val;
} keyword_table [] =
{
    "...",                  ELLIPSIS,
    "copy",                 COPY,
    "END",                  END,
    "grouping",             GROUPING,
    "escape_char",          ESCAPE_CHAR,
    "comment_char",         COMMENT_CHAR,
    "upper",                CHCLASS_KWD,
    "lower",                CHCLASS_KWD,
    "alpha",                CHCLASS_KWD,
    "digit",                CHCLASS_KWD,
    "punct",                CHCLASS_KWD,
    "xdigit",               CHCLASS_KWD,
    "space",                CHCLASS_KWD,
    "print",                CHCLASS_KWD,
    "graph",                CHCLASS_KWD,
    "blank",                CHCLASS_KWD,
    "cntrl",                CHCLASS_KWD,
    "isideogram",           CHCLASS_KWD,        // AT&T EXTENSION
    "isphonogram",          CHCLASS_KWD,        // AT&T EXTENSION
    "isenglish",            CHCLASS_KWD,        // AT&T EXTENSION
    "isspecial",            CHCLASS_KWD,        // AT&T EXTENSION
    "charclass",            CHCLASS,
    "toupper",              CHCONV_KWD,
    "tolower",              CHCONV_KWD,
    "collating-symbol",     COLL_SYM,
    "collating-element",    COLL_ELMT,
    "from",                 FROM,
    "order_start",          ORDER_START,
    "order_end",            ORDER_END,
    "forward",              FORWARD,
    "backward",             BACKWARD,
    "position",             POSITION,
    "UNDEFINED",            UNDEFINED,
    "IGNORE",               IGNORE,
    "yesexpr",              YESEXPR,
    "noexpr",               NOEXPR,
    "yesstr",               YESSTR,
    "nostr",                NOSTR,
    "int_curr_symbol",      MONETARY_KWD_STR,
    "currency_symbol",      MONETARY_KWD_STR,
    "mon_decimal_point",    MONETARY_KWD_STR,
    "mon_thousands_sep",    MONETARY_KWD_STR,
    "positive_sign"  ,      MONETARY_KWD_STR,
    "negative_sign",        MONETARY_KWD_STR,
    "int_frac_digits",      MONETARY_KWD_CHAR,
    "frac_digits",          MONETARY_KWD_CHAR,
    "p_cs_precedes",        MONETARY_KWD_CHAR,
    "p_sep_by_space",       MONETARY_KWD_CHAR,
    "n_cs_precedes",        MONETARY_KWD_CHAR,
    "n_sep_by_space",       MONETARY_KWD_CHAR,
    "p_sign_posn",          MONETARY_KWD_CHAR,
    "n_sign_posn",          MONETARY_KWD_CHAR,
    "mon_grouping",         MONETARY_GRP,
    "-1",                   NEG_ONE,
    "decimal_point",        NUM_KWD_STR,
    "thousands_sep",        NUM_KWD_STR,
    "abday",                TIME_KWD_NAME,
    "day",                  TIME_KWD_NAME,
    "abmon",                TIME_KWD_NAME,
    "mon",                  TIME_KWD_NAME,
    "d_t_fmt",              TIME_KWD_FMT,
    "d_fmt",                TIME_KWD_FMT,
    "t_fmt",                TIME_KWD_FMT,
    "am_pm",                TIME_KWD_NAME,
    "t_fmt_ampm",           TIME_KWD_FMT,
    "att_d_t_fmt",          TIME_KWD_FMT,
    "era",                  TIME_KWD_OPT,
    "era_d_fmt",            TIME_KWD_OPT,
    "era_t_fmt",            TIME_KWD_OPT,
    "era_d_t_fmt",          TIME_KWD_OPT,
    "alt_digits",           TIME_KWD_OPT,
    "",                     0,
};

#ifdef ECHO
#undef ECHO
#define ECHO { fprintf(stderr, "token %s not recognized (line %d)\n", yytext, yylineno); exit(4); }
#endif

#undef YYLMAX
#define YYLMAX 2500
%}

%s  NORMAL  QUOTE
%{
    static int first_time = 1;
%}

ws       [ \t]+ 
/* comment  #.*\n */
alpha    [a-zA-Z]
dec      [0-9]
oct      [0-7]
hex      [0-9a-fA-F]
alphanum [0-9a-zA-Z]

%e    1000
%k    2000
%a    16000
%o    30000
%n    1000
%p    20000

%%
%{
    if (first_time) {
        BEGIN NORMAL;
        first_time = 0;
    }
%}

<NORMAL>\"	{
    {
	BEGIN QUOTE;
	return '"';
    } }

<QUOTE>\\{oct}{oct}+    { LDBG_PRINTF(("lex: returning OCTAL_CHAR\n"));
                          yylval.str = (char *) strdup(yytext);
                          return(OCTAL_CHAR);
                        } 

<QUOTE>\\x{hex}{hex}+   { LDBG_PRINTF(("lex: returning HEX_CHAR\n")); 
                          yylval.str = (char *) strdup(yytext);
        	          return(HEX_CHAR); 
                        }

<QUOTE>\\d{dec}{dec}+   { LDBG_PRINTF(("lex: returning DECIMAL_CHAR\n")); 
                          yylval.str = (char *) strdup(yytext);
        		  return(DECIMAL_CHAR); 
			}

<QUOTE>\<[^\"\>]+\>	{
    {
	if (search_list(collsym_list, yytext)) {
	    LDBG_PRINTF(("lex: returning COLLSYMBOL\n"));
	    yylval.str = (char *) strdup(yytext);
	    return(COLLSYMBOL);
	} 
	if (search_list(collelt_list, yytext)) {
	    LDBG_PRINTF(("lex: returning COLLELEMENT\n"));
	    yylval.str = (char *) strdup(yytext);
	    return(COLLELEMENT);
	} 
	if (search_list(charmap_list, yytext)) {
	    LDBG_PRINTF(("lex: returning CHARSYMBOL\n"));
	    yylval.str = (char *) strdup(yytext);
	    return(CHARSYMBOL);
	}
	
	LDBG_PRINTF(("lex: returning EXTENDED_REG_EXP %s\n", yytext));
	yylval.str = (char *) strdup(yytext);
	return(EXTENDED_REG_EXP);
    } }

<QUOTE>[^\"\<]*		{
    {
	LDBG_PRINTF(("lex: returning CHAR1\n")); /* {1} not * */
	yylval.str = (char *) strdup(yytext);
	return(CHAR); 
    } }

<QUOTE>\"		{
    {
	BEGIN NORMAL;
	return '"';
    } }

^[ \t]*\n       { LDBG_PRINTF(("lex: ignoring blank line\n")); }

<NORMAL>{ws}    { LDBG_PRINTF(("lex: ignoring whitespace\n")); }

^#.*\n          { LDBG_PRINTF(("lex: ignoring comment\n")); }

escape_char.*\n { LDBG_PRINTF(("lex: ignoring escape_char line\n")); }

comment_char.*\n { LDBG_PRINTF(("lex: ignoring comment_char line\n")); }

-?{dec}+        { LDBG_PRINTF(("lex: returning NUMBER\n")); 
                  yylval.ival = atoi(yytext);
                  return(NUMBER); 
                }

\\{oct}{oct}+   { LDBG_PRINTF(("lex: returning OCTAL_CHAR\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(OCTAL_CHAR); 
                }

\\x{hex}{hex}+  { LDBG_PRINTF(("lex: returning HEX_CHAR\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(HEX_CHAR); 
                }

\\d{dec}{dec}+  { LDBG_PRINTF(("lex: returning DECIMAL_CHAR\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(DECIMAL_CHAR); 
                }

\n     	        { LDBG_PRINTF(("lex: returning EOL\n")); 
                  return(EOL); 
                }

\,              { LDBG_PRINTF(("lex: returning COMMA\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(COMMA); 
                }

;               { LDBG_PRINTF(("lex: returning SEMI_COLON\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(SEMI_COLON); 
                }

\"\"            { LDBG_PRINTF(("lex: returning EMPTY_STRING\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(EMPTY_STRING); 
                }

\"              { LDBG_PRINTF(("lex: returning DOUBLE_QUOTE\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(DOUBLE_QUOTE); 
                }

\(     	        { LDBG_PRINTF(("lex: returning OPEN_PAREN\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(OPEN_PAREN); 
                }

\)     	        { LDBG_PRINTF(("lex: returning CLOSE_PAREN\n")); 
                  yylval.str = (char *) strdup(yytext);
                  return(CLOSE_PAREN); 
                }

LC_CTYPE        { LDBG_PRINTF(("lex: returning LC_CTYPE\n"));
                 yylval.str = (char *) strdup(yytext);
                 return(LC_CTYPE_TOK);
                }

LC_COLLATE     { LDBG_PRINTF(("lex: returning LC_COLLATE\n"));
                 yylval.str = (char *) strdup(yytext);
                 return(LC_COLLATE_TOK);
                }

LC_MESSAGES    { LDBG_PRINTF(("lex: returning LC_MESSAGES\n"));
                 yylval.str = (char *) strdup(yytext);
                 return(LC_MESSAGES_TOK);
                }

LC_MONETARY    { LDBG_PRINTF(("lex: returning LC_MONETARY\n"));
                 yylval.str = (char *) strdup(yytext);
                 return(LC_MONETARY_TOK);
                }

LC_NUMERIC     { LDBG_PRINTF(("lex: returning LC_NUMERIC\n"));
                 yylval.str = (char *) strdup(yytext);
                 return(LC_NUMERIC_TOK);
                }

LC_TIME        { LDBG_PRINTF(("lex: returning LC_TIME\n"));
                 yylval.str = (char *) strdup(yytext);
                 return(LC_TIME_TOK);
                }

EOF             { LDBG_PRINTF(("lex: got EOF\n"));  
                  return(EOF); 
                }

<NORMAL>{alpha}+{alphanum}{0,10} { if ((rval = keyword_match(yytext)) != 0) {
                               LDBG_PRINTF(("lex: matched keyword %s\n", yytext));
                               yylval.str = (char *) strdup(yytext);
                               return(rval) ;
                            }
                            yylval.str = (char *) strdup(yytext);
                            if (strlen(yytext) == 1) {
                                LDBG_PRINTF(("lex: returning CHAR2\n"));
                                yylval.str = (char *) strdup(yytext);
                                return(CHAR);
                            } else {
                                LDBG_PRINTF(("lex: returning CHARCLASS\n"));
                                yylval.str = (char *) strdup(yytext);
                                return(CHARCLASS);
                            }
                         }

<NORMAL>\<[^\t\>]+\>	{
    {
	if (search_list(collsym_list, yytext)) {
	    LDBG_PRINTF(("lex: returning COLLSYMBOL\n"));
	    yylval.str = (char *) strdup(yytext);
	    return(COLLSYMBOL);
	} 
	if (search_list(collelt_list, yytext)) {
	    LDBG_PRINTF(("lex: returning COLLELEMENT\n"));
	    yylval.str = (char *) strdup(yytext);
	    return(COLLELEMENT);
	}
	if (search_list(charmap_list, yytext)) {
	    LDBG_PRINTF(("lex: returning CHARSYMBOL\n"));
	    yylval.str = (char *) strdup(yytext);
	    return(CHARSYMBOL);
	} 

	yylval.str = (char *) strdup ( yytext );
	return ( SYMBOLIC_NAME );

    } }

<NORMAL>[^ \t\n,;<\"\(\)\\]{2,}  { if ((rval = keyword_match(yytext)) != 0) {
                               LDBG_PRINTF(("lex: in NORMAL mode, matched keyword %s\n", yytext));
                               yylval.str = (char *) strdup(yytext);
                               return(rval) ;
                           }
                           LDBG_PRINTF(("lex: in NORMAL mode (2)\n"));
                           ECHO;
                         }                   

\\\n          { LDBG_PRINTF(("lex: got line continuation char\n")); }

<NORMAL>[^ \t\n,;<\"\(\)\\0-9]{1}   { LDBG_PRINTF(("lex: returning CHAR3\n"));
                               yylval.str = (char *) strdup(yytext);
                               return(CHAR); 
                             }
%%

int keyword_match(char *s)
{
    struct keyword *ptr = keyword_table;

    while ((ptr->str != NULL) && (*(ptr->str) != '\0')) 
        if (strcmp(ptr->str, s) == 0)
            return ptr->val;
        else
            ptr++;

    return 0;
}

yyerror(char *s)
{
    fprintf ( stderr, GETMSG ( MSGID_SYNTAX ), yylineno, yytext );
    exit(4);
}  
