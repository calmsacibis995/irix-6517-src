%{

/*
 *  @OSF_COPYRIGHT@
 *  COPYRIGHT NOTICE
 *  Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 *  ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 *  src directory for the full copyright text.
 */

/*
 * HISTORY
 * $Log: acf.l,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
NONERevision 1.1.5.5  1993/01/03  21:37:26  bbelch
NONE	Embedding copyright notice
NONE	[1993/01/03  14:31:40  bbelch]
NONE	Embedding copyright notice
NONE
NONERevision 1.1.12.2  1993/01/03  14:31:44  bbelch
NONE	Embedding copyright notice
NONE
NONERevision 1.1.5.4  1992/12/23  18:42:09  zeliff
NONE	Embedding copyright notice
NONE	[1992/12/23  00:58:39  zeliff]
NONE	Embedding copyright notice
NONE
NONERevision 1.1.12.2  1992/12/23  00:58:42  zeliff
NONE	Embedding copyright notice
NONE
NONERevision 1.1.5.3  1992/12/04  17:09:49  marty
NONE	OSC1.1.1 upgrade.
NONE	[1992/12/04  17:01:14  marty]
NONE
 * Revision 1.1.7.2  1992/11/17  19:59:29  marty
 * 	OSC1.1.1 upgrade.
 * 	[1992/11/17  13:20:31  marty]
 * 
 * Revision 1.1.5.2  1992/09/29  20:40:20  devsrc
 * 	Fix merge hiccup.
 * 	[1992/09/21  18:32:47  weisman]
 * 
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:33:29  weisman]
 * 
 * $EndLog$
 */

 /*
 **
 **  Copyright (c) 1989 by
 **      Hewlett-Packard Company, Palo Alto, Ca. &
 **      Digital Equipment Corporation, Maynard, Mass.
 **
 ** (c) Copyright 1991, 1992 
 ** Siemens-Nixdorf Information Systems, Burlington, MA, USA
 ** All Rights Reserved
 **
 **  NAME:
 **
 **      acf.l
 **
 **  FACILITY:
 **
 **      Interface Definition Language (IDL) Compiler
 **
 **  ABSTRACT:
 **
 ** "lex" source file for Attribute Configuration File (ACF) parser.
 **
 **  VERSION: DCE 1.0
 **
 */

#define PROCESSING_LEX          /* Define before including nidl.h */
#define PROCESSING_ACF_LEX      /* Define before including nidl.h */
#include <nidl.h>               /* IDL common defs */
#ifdef SNI_SVR4
#include <ast.h>                /* for acf_y_tab.c union definition */
#include <astp.h>               /* for acf_y_tab.c union definition */
#endif /* SNI_SVR4 */
#include <acf_y_tab.h>          /* yacc include file */
#include <nametbl.h>            /* Nametable defs */
#include <nidlmsg.h>            /* Error message IDs */

extern void commenteof(         /* Called on EOF within comment */
#ifdef PROTO
    void
#endif
);

static void acf_yymark(         /* Handles # <lineno> left by cpp */
#ifdef PROTO
    void
#endif
);

static int acf_screen(          /* Screens a token to see if it is a keyword */
#ifdef PROTO
    char *token
#endif
);

static void acf_read_comment(   /* Reads and copies comment text */
#ifdef PROTO
    void
#endif
);

static char *identifier_name;   /* Ptr to parsed identifier */

typedef struct                  /* Keyword and corresponding value */
{
    char    *keyword;
    int     token_value;
} keyword_entry;

static keyword_entry acf_keywords[] =   /* Table of keywords and values */
{
    {"auto_handle",     AUTO_HANDLE_KW},
    {"code",            CODE_KW},
    {"comm_status",     COMM_STATUS_KW},
    {"enable_allocate", ENABLE_ALLOCATE_KW},
    {"explicit_handle", EXPLICIT_HANDLE_KW},
    {"fault_status",    FAULT_STATUS_KW},
    {"handle_t",        HANDLE_T_KW},
    {"heap",            HEAP_KW},
    {"implicit_handle", IMPLICIT_HANDLE_KW},
    {"include",         INCLUDE_KW},
    {"interface",       INTERFACE_KW},
    {"in_line",         IN_LINE_KW},
    {"nocode",          NOCODE_KW},
    {"out_of_line",     OUT_OF_LINE_KW},
    {"represent_as",    REPRESENT_AS_KW},
    {"typedef",         TYPEDEF_KW},
    {0,                 0}      /* Sentinel - Do not remove */
};

%}


delim           [ \t\n]
ws              {delim}+
letter          [A-Za-z_$]
digit           [0-9]
oct_digit       [0-7]
hex_digit       [0-9A-Fa-f]
id              {letter}({letter}|{digit})*
other           .

%%


{ws}        { /* White space: No action, and no return */ }

","         {return(COMMA);}
"{"         {return(LBRACE);}
"??<"       {return(LBRACE);}
"["         {return(LBRACKET);}
"("         {return(LPAREN);}
"}"         {return(RBRACE);}
"??>"       {return(RBRACE);}
"]"         {return(RBRACKET);}
")"         {return(RPAREN);}
";"         {return(SEMI);}

^"#"{ws}*{digit}*({ws}+.*)?\n  {acf_yymark();}  /* cpp # lineno filename */

\"[^\"\n]*\" {  /* Quoted string: Strip off quotes, add to string table. */
                char    stripped_string[max_string_len];

                strcpy(stripped_string, (char *)&yytext[1]);
                stripped_string[strlen((char *)yytext)-2] = '\0';
                yylval.y_string = STRTAB_add_string(stripped_string);
                return(STRING);
            }

"/*"        {   /* Comment: Ignore through closing delimiter. */
                acf_read_comment();
            }

{id}        {
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

                /* Identifier: See if it is a valid keyword. */
                return acf_screen((char *)yytext);
            }

{other}     {return(UNKNOWN);};
%%

/*
 *  a c f _ y y w r a p
 *
 *  Function:   Called by lexical analyzer when end-of-file reached.
 *
 *  Globals:    acf_yylineno
 *
 *  Returns:    0 => continue lexical analysis
 *              1 => wrap-up lexical analysis
 */

#ifdef PROTO
int acf_yywrap(void)
#else
int acf_yywrap()
#endif

{
    /*
     * Note: Currently no support for one acf #including another.
     */
    return (1);
}

/*
**  a c f _ y y m a r k
**
**  Processes a "# lineno" line as left by cpp.
**
**  Implicit Outputs:
**      acf_yylineno is updated; If a filename was included in the source
**      line, set_name_for_errors is called to set up the source filename
**      for error messages.
*/

static void acf_yymark()
{
    char    *source ; /* Source file name in #line directive */

    source = (char *) MALLOC(yyleng) ;
    sscanf((char *)yytext, "# %d %s", &acf_yylineno, source) ;

    /* If text included a source file name, set name for error reporting. */
    if (source[0] != '\0')
    {
        char    file_name[max_string_len];

        /* Strip the quotes. */
        strcpy(file_name, &source[1]);
        file_name[strlen(file_name)-1] = '\0';

        set_name_for_errors(file_name);
    }

    FREE(source);
}

/*
 *  a c f _ s c r e e n
 *
 *  Function:   Checks to see if a token is a keyword.
 *
 *  Inputs:     token to check
 *
 *  Returns:    specific keyword ID if token is a keyword;
 *              the value IDENTIFIER otherwise
 */

#ifdef PROTO
static int acf_screen(char *token)
#else
static int acf_screen(token)
    char *token;                /* Token to "screen" */
#endif

{
    int i;

    /*
     * Scan the reserved word table.
     */
    for (i = 0; acf_keywords[i].keyword != 0; i++)
        if (strcmp(token, acf_keywords[i].keyword) == 0)
            return acf_keywords[i].token_value;     /* Return keyword ID */

    /*
     * Not a reserved word; therefore, an identifier, return nametable id.
     */
    yylval.y_id = NAMETABLE_add_id(token);
    return IDENTIFIER;                  /* Return IDENTIFER */
}

/*
 *  a c f _ r e a d _ c o m m e n t
 *
 *  Function:   Reads a comment in the source file.
 *
 */

#ifdef PROTO
static void acf_read_comment(void)
#else
static void acf_read_comment()
#endif

{
    int     c;

    while ( (c = input()) != 0 )
    {
        if (c == '*')
        {
            if ( (c = input()) == '/')
                break;
            else
                unput(c);
        }
        else if (c == 0)
            commenteof();
    }
}

/*
 *  a c f _ k e y w o r d _ l o o k u p
 *
 *  Looks up a keyword's text string given its numeric token value.
 */

char * acf_keyword_lookup
#ifdef PROTO
(
    int         token_value     /* Numeric value of keyword token */
)
#else
(token_value)
    int         token_value;    /* Numeric value of keyword token */
#endif

{
    keyword_entry * entry;

    for (entry = acf_keywords ; entry->keyword != NULL ; entry++)
        if (entry->token_value == token_value)
            return entry->keyword;

    /* Not found, just return question mark. */
    return "?";
}


 /** End of "lex" source file. **/

