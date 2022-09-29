%{
 /*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/* 
 * (c) Copyright 1991, 1992 
 *     Siemens-Nixdorf Information Systems, Burlington, MA, USA
 *     All Rights Reserved
 */
/*
 * HISTORY
 * $Log: nidl.l,v $
 * Revision 1.2  1993/09/08 20:13:23  jwag
 * first ksgen hack
 *
 * Revision 1.1  1993/08/31  06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.5.5  1993/01/03  21:40:46  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:09  bbelch]
 *
 * Revision 1.1.5.4  1992/12/23  18:49:52  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:26  zeliff]
 * 
 * Revision 1.1.5.3  1992/12/04  17:09:42  marty
 * 	OSC1.1.1 upgrade.
 * 	[1992/12/04  17:01:55  marty]
 * 
 * Revision 1.1.7.2  1992/11/17  19:59:52  marty
 * 	OSC1.1.1 upgrades.
 * 	[1992/11/17  13:19:58  marty]
 * 
 * Revision 1.1.5.2  1992/09/29  20:41:14  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:35:11  weisman]
 * 
 * Revision 1.1.2.3  1992/07/24  21:08:41  jim
 * 	Do not use the declarations for yyunput or yyoutput on AIX 3.2 with all
 * 	updates.
 * 	[1992/07/24  21:08:05  jim]
 * 
 * Revision 1.1.2.2  1992/05/05  15:17:05  harrow
 * 	*** empty log message ***
 * 
 * Revision 1.1.1.2  1992/05/04  19:50:21  harrow
 * 	Truncate identifiers at MAX_ID length
 * 
 * Revision 1.1  1992/01/19  03:02:00  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */

/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      IDL.L
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      This file defines the tokenizing rules for lexical analysis.
**
**  VERSION: DCE 1.0
**
*/



/*  Get definitions for token values    */
#define PROCESSING_LEX          /* Define before including nidl.h */
#define PROCESSING_NIDL_LEX     /* Define before including nidl.h */
#include <nidl.h>
#include <ctype.h>
#include <errors.h>
#include <nametbl.h>
#include <ast.h>
#include <astp.h>
#include <y_tab.h>
#include <driver.h>

boolean search_attributes_table = false ;

#ifndef MAX_INCLUSION_DEPTH
#define MAX_INCLUSION_DEPTH 10
#endif

static  FILE *inclusion_stack[MAX_INCLUSION_DEPTH] ;
static  int  stack_depth = 0 ;


void commenteof (
#ifdef PROTO
    void
#endif
);

int yylex (
#ifdef PROTO
    void
#endif
);

int yylook (
#ifdef PROTO
    void
#endif
);

static void read_c_comment (
#ifdef PROTO
    void
#endif
);

static void yymark (
#ifdef PROTO
    void
#endif
);

static int yywrap (
#ifdef PROTO
    void
#endif
);

static int yyuuid (
#ifdef PROTO
    char *s,
    nidl_uuid_t *uuid
#endif
);

static int yyolduuid (
#ifdef PROTO
    char *s,
    nidl_uuid_t *uuid
#endif
);



int KEYWORDS_screen (
#ifdef PROTO
    char *token,
    NAMETABLE_id_t *id
#endif
);

char *KEYWORDS_lookup_text (
#ifdef PROTO
    long token
#endif
);

int yyinput (
#ifdef PROTO
    void
#endif
);

#ifndef _AIX
#if defined(SNI_SVR4) || defined(SGI)
void yyoutput (
#else
int yyoutput (
#endif
#ifdef PROTO
    int c
#endif
);

#if defined(SNI_SVR4) || defined(SGI)
void yyunput (
#else
int yyunput (
#endif
#ifdef PROTO
    int c
#endif
);
#endif

int yyback (
#ifdef PROTO
    int *p,

    int m
#endif
);


%}


/* regular definitions  */

delim           [ \t\n]
opt_ws          {delim}*
ws              {delim}
letter          [A-Za-z_$]
digit           [0-9]
hex_digit       [0-9A-Fa-f]
id              {letter}({letter}|{digit})*
l_suffix        (l|L)
u_suffix        (u|U)
f_suffix        (u|U)
integer         -?{digit}+(({u_suffix}{l_suffix})|({l_suffix}{u_suffix})|{l_suffix}|{u_suffix})?
c_hex_integer   (0(x|X){hex_digit}*)(({l_suffix}{u_suffix}?)|({u_suffix}{l_suffix}?))?
float           {digit}+\.{digit}+({f_suffix}|{l_suffix})?
octet           {hex_digit}{hex_digit}
octet2          {octet}{octet}
octet_dot       {octet}\.
octet2_dash     {octet2}\-
olduuid         \({opt_ws}{octet2}{octet2}{octet}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet}{opt_ws}\)
uuid            \({opt_ws}{octet2}{octet2_dash}{octet2_dash}{octet2_dash}{octet2_dash}{octet2}{octet2}{octet2}{opt_ws}\)
other           .

%%
{ws}            { /* No action, and no return */ }

":"             {return(COLON);}
","             {return(COMMA);}
".."            {return(DOTDOT);}
"="             {return(EQUAL);}
\[              {return(LBRACKET);}
"("             {return(LPAREN);}
\]              {return(RBRACKET);}
")"             {return(RPAREN);}
";"             {return(SEMI);}
"*"             {return(STAR);}
"{"             {return(LBRACE);}
"??<"           {return(LBRACE);}
"}"             {return(RBRACE);}
"??>"           {return(RBRACE);}
"?"             {return(QUESTION);}
"|"             {return(BAR);}
"||"            {return(BARBAR);}
"<"             {return(LANGLE);}
"<<"            {return(LANGLEANGLE);}
">"             {return(RANGLE);}
">>"            {return(RANGLEANGLE);}
"&"             {return(AMP);}
"&&"            {return(AMPAMP);}
"<="            {return(LESSEQUAL);}
">="            {return(GREATEREQUAL);}
"=="            {return(EQUALEQUAL);}
"^"             {return(CARET);}
"+"             {return(PLUS);}
"-"             {return(MINUS);}
"!"             {return(NOT);}
"!="            {return(NOTEQUAL);}
"/"             {return(SLASH);}
"%"             {return(PERCENT);}
"~"             {return(TILDE);}

^"#"[ \t]*{digit}*([ \t]+.*)?\n  { yymark() ;}
^"%"(c|C){ws}\n     { log_error(yylineno-1,NIDL_USETRANS); }
^"%pascal"{ws}\n    { log_error(yylineno-1,NIDL_USETRANS); }
^"%PASCAL"{ws}\n    { log_error(yylineno-1,NIDL_USETRANS); }

'\\''   {
    /* Simple escaped single quote character literal */
    yylval.y_char = '\'';
    return(CHAR);
}

'[^'\n\\]'  {
    /* Simple character constants */
    yylval.y_char = yytext [1];
    return(CHAR);
}

'\\[^'\n]*' {
    /* Character constants with an escape */
    if ((yyleng > 6) || (yyleng < 4)) {
          log_error(yylineno, NIDL_INVCHARLIT);
          return(UNKNOWN);
    }
    switch (yytext[2]) {
        case 'n':   yylval.y_char = '\n'; break;
        case 't':   yylval.y_char = '\t'; break;
        case 'v':   yylval.y_char = '\v'; break;
        case 'b':   yylval.y_char = '\b'; break;
        case 'r':   yylval.y_char = '\r'; break;
        case 'f':   yylval.y_char = '\f'; break;
        case 'a':   yylval.y_char = '\a'; break;
        case '\\':  yylval.y_char = '\\'; break;
        case '?':   yylval.y_char = '\?'; break;
        case '\'':  yylval.y_char = '\''; break;
        case '\"':  yylval.y_char = '\"'; break;
        case 'x' :  {
            /* Hex literal value */
            int char_value;
            sscanf((char *)&yytext[3],"%x",&char_value);
            yylval.y_char = (char )char_value;
            log_warning(yylineno, NIDL_NONPORTCHAR);
            break;
        }
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '7':   {
            /* Octal literal value */
            int char_value;
            sscanf((char *)&yytext[2],"%o",&char_value);
            yylval.y_char = (char )char_value;
            log_warning(yylineno, NIDL_NONPORTCHAR);
            break;
        }
        default:
            /* all others are illegal */
            log_error(yylineno, NIDL_INVCHARLIT);
            return(UNKNOWN);
    }

    return (CHAR);
}

\"[^\"\n]*  {
    char stripped_string[max_string_len] ;
    if (yytext[yyleng-1] == '\\')
        yymore(); /* Allow \" within strings, look for next " */
    else {
    yylval.y_string = STRTAB_add_string((char *)&yytext[1]);
        if (input() == '\n')
            log_error(yylineno, NIDL_STRUNTERM);
        return(STRING);
    }
}

"/*"    { read_c_comment(); }

{id}    {
    int token;
    NAMETABLE_id_t id;
    /* If id is too long, truncate it and issue a warning */
    if (yyleng > MAX_ID) {
        NAMETABLE_id_t id;
        char *identifier;
        id = NAMETABLE_add_id((char *)yytext);
        NAMETABLE_id_to_string(id, &identifier);
        log_warning(yylineno, NIDL_IDTOOLONG, identifier, MAX_ID);
        /* Truncate the string */
        yytext[MAX_ID] = '\0';
        id = NAMETABLE_add_id((char *)yytext);
    }
    if ((token = KEYWORDS_screen((char *)yytext, &id))==IDENTIFIER) {
        yylval.y_id = id;
    }
    else {
        yylval.y_id = NAMETABLE_NIL_ID;
    }
    return token;
}


{integer}   {
    int unsigned_int = false;
#if defined(vax) && defined(ultrix)
    float fval;
    char *fval_fmt = "%f";
#define FLOAT float
#else
    double fval;
    char *fval_fmt = "%lf";
#define FLOAT double
#endif

    /*
    ** Remove suffix for long and/or unsigned, if present
    */
    if ((yytext[yyleng-1] == 'L') ||
        (yytext[yyleng-1] == 'l') ||
        (yytext[yyleng-1] == 'U') ||
        (yytext[yyleng-1] == 'u'))
    {
        if ((yytext[yyleng-1] == 'U') ||
            (yytext[yyleng-1] == 'u'))
                unsigned_int = true;
        yytext[yyleng-1] = '\0';
        if ((yytext[yyleng-2] == 'L') ||
            (yytext[yyleng-2] == 'l') ||
            (yytext[yyleng-2] == 'U') ||
            (yytext[yyleng-2] == 'u'))
        {
            if ((yytext[yyleng-2] == 'U') ||
                (yytext[yyleng-2] == 'u'))
                    unsigned_int = true;
            yytext[yyleng-2] = '\0';
        }
    }

    /*
    ** Convert to a float to get overflow detection.
    */
    sscanf((char *)yytext, fval_fmt, &fval);
    yylval.y_ival = 0;

    /*
    ** Throw out integers that are out of range.
    */
    if (unsigned_int &&
        ((strlen((char *)yytext) > 10)
        || (fval > (FLOAT)ASTP_C_ULONG_MAX)
        || (fval < (FLOAT)ASTP_C_ULONG_MIN)
        ))
        log_error(yylineno, NIDL_INTOVERFLOW, KEYWORDS_lookup_text(LONG_KW));
    else if ((strlen((char *)yytext) > 11) ||
        (fval > (FLOAT)ASTP_C_LONG_MAX) ||
        (fval < (FLOAT)ASTP_C_LONG_MIN)
        )
        log_error(yylineno, NIDL_INTOVERFLOW, KEYWORDS_lookup_text(LONG_KW));
    else {
        sscanf((char *)yytext, "%i", &yylval.y_ival);
    }


    if ((yytext[0] == '0') && (strlen((char *)yytext) != strspn((char *)yytext,"01234567"))) {
        char    *int_text;              /* Text of integer */
        STRTAB_str_t string_id;         /* Entry in string table of integer */
        string_id = STRTAB_add_string(yytext_p);
        STRTAB_str_to_string(string_id, &int_text);
        log_error(yylineno, NIDL_INVOCTDIGIT, int_text);
    }

    return(INTEGER_NUMERIC);
}

{c_hex_integer} {
    /*
    ** Remove suffix for long and/or unsigned, if present
    */
    if ((yytext[yyleng-1] == 'L') ||
        (yytext[yyleng-1] == 'l') ||
        (yytext[yyleng-1] == 'U') ||
        (yytext[yyleng-1] == 'u'))
    {
        yytext[yyleng-1] = '\0';
        if ((yytext[yyleng-2] == 'L') ||
            (yytext[yyleng-2] == 'l') ||
            (yytext[yyleng-2] == 'U') ||
            (yytext[yyleng-2] == 'u'))
        {
            yytext[yyleng-2] = '\0';
        }
    }

    /*
    ** Scan the hex integer and return the value as an integer
    */
    sscanf((char *)&yytext[2],"%lx", &yylval.y_ival);
    return(INTEGER_NUMERIC);
}

{float} {
    yylval.y_float = STRTAB_add_string((char *)yytext);
    return(FLOAT_NUMERIC);
}

{uuid}      {   return (yyuuid((char *)&yytext[1], &yylval.y_uuid)); }
{olduuid}   {   return (yyolduuid((char *)&yytext[1], &yylval.y_uuid)); }
{other}     {   return (UNKNOWN); }

%%

void commenteof()
{
    log_error (yylineno, NIDL_COMMENTEOF);
    nidl_terminate();
}

static int yywrap ()
{
    int    c ;
    if ((c = input()) != 0)
    {
        unput(c) ;
        return (0) ;
    }
    yylineno = 1 ;
        return (1);
}

static void yymark()
{
    char    *source ; /* Source file name in #line directive */

    source = (char *) MALLOC(yyleng) ;
    source[0] ='\0';
    sscanf((char *)yytext, "# %d %s", &yylineno, source) ;

    /* If text included a source file name, set name for error reporting. */
    if (source[0] != '\0'
        &&  !(source[0] == '"' && source[1] == '"'))
    {
        char    file_name[max_string_len];

        /* Strip the quotes. */
        strcpy(file_name, &source[1]);
        file_name[strlen(file_name)-1] = '\0';

        set_name_for_errors(file_name);
    }

    FREE(source);
}

static void read_c_comment()
{
    register int c;

    /* While not EOF look for end of comment */
    while ((c = input()))
    {
        if (c == '*')
        {
            if ((c = input()) == '/')
                break ;
            else
                unput(c) ;
        }
    }

    /* Didn't find end comment before EOF, issue error */
    if (c == 0) commenteof();
}


static int yyolduuid(s, uuid)
char *s;
nidl_uuid_t *uuid;
{
    unsigned32 v1_time_high;
    unsigned32 v1_time_low;
    unsigned32 v1_reserved = 0;  /* v1 UUID always zero here */
    unsigned32 v1_family;
    unsigned32 v1_host[7];
    int        i;
    char *uuid_str;

    if (sscanf(s, "%8X%4X.%2X.%2X.%2X.%2X.%2X.%2X.%2X.%2X",
              &v1_time_high, &v1_time_low, &v1_family,
              &v1_host[0], &v1_host[1], &v1_host[2], &v1_host[3],
              &v1_host[4], &v1_host[5], &v1_host[6]) != 10)
        log_error(yylineno, NIDL_SYNTAXUUID);

    /* scanf only returns ints, so scan into ints and copy into smaller types */
    uuid->time_low                  = v1_time_high;
    uuid->time_mid                  = v1_time_low;
    uuid->time_hi_and_version       = v1_reserved;
    uuid->clock_seq_hi_and_reserved = v1_family;
    uuid->clock_seq_low             = v1_host[0];

    for (i=0; i < 6; i++)
        uuid->node[i] = v1_host[i+1];

    uuid_str = (char *)malloc(sizeof("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"));

    sprintf(uuid_str, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
              uuid->time_low, uuid->time_mid, uuid->time_hi_and_version,
              uuid->clock_seq_hi_and_reserved, uuid->clock_seq_low,
              uuid->node[0], uuid->node[1], uuid->node[2],
              uuid->node[3], uuid->node[4], uuid->node[5]);

    log_warning(yylineno, NIDL_OLDUUID);
    log_warning(yylineno, NIDL_NEWUUID, uuid_str);
    return (UUID_REP) ;
}

static int yyuuid(s, uuid)
char *s;
nidl_uuid_t *uuid;
{
    int i;
    unsigned32 time_low, time_mid, time_hi_and_version,
              clock_seq_hi_and_reserved, clock_seq_low,
              node[6];

    if (sscanf(s, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
              &time_low, &time_mid, &time_hi_and_version,
              &clock_seq_hi_and_reserved, &clock_seq_low,
              &node[0], &node[1], &node[2],
              &node[3], &node[4], &node[5]) != 11)
        log_error(yylineno, NIDL_SYNTAXUUID);

    /* scanf only returns ints, so scan into ints and copy into smaller types */
    uuid->time_low = time_low;
    uuid->time_mid = time_mid;
    uuid->time_hi_and_version = time_hi_and_version;
    uuid->clock_seq_hi_and_reserved = clock_seq_hi_and_reserved;
    uuid->clock_seq_low = clock_seq_low;
    for (i=0; i < 6; i++)
        uuid->node[i] = node[i];
    return (UUID_REP);
}
