%{
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "globals.h"
#include "parser.h"

static char *strndup(const char *str, int maxlen);
static int yyinput(char *buf, int max_size);

// Input control.  We redefine the input routine to one that reads the input
// line by line and then parcels the characters within that line one at a time
// (see yyinput below in the ``routines'' section.  This is done so we can
// report errors reasonably by printing out the line number, the line itself,
// and a pointer beneather the line pointing at the offending token.
//
#define YY_INPUT(buf,result,max_size) (result = yyinput(buf, max_size))

char file[PATH_MAX];			// name of file we're processing
char line[1024];			// buffer to hold input
int linelen = 0;			// length of buffer
int lineindex = 0;			// current position in buffer
int lineno = 0;				// current line number


// Global variables (see descriptions in "globals.h"):
//
int compilation_errors = 0;
%}

identifier	[A-Za-z_][A-Za-z_0-9]*
integer		(0[xX][0-9a-fA-F]+)|(0[0-7]+)|([+-]?[0-9]+)
comment		\/\/.*
whitespace	([ \t]+)|{comment}|\n
any		.|\n
quote_string	\"([^"]|\\\")*\"
squote_string	\'([^']|\\\')*\'
angle_string	\<[^>]*\>

%x in_nested_string

%%

    // counter for paren and brace nesting
    int nesting;

    // Gross Hack Alert!  Sometimes we want to interpret ``(...)'' as a
    // parenthetically quoted string and sometimes we want to treat it as
    // an open parenthsis, followed by the inside goo, followed by a closing
    // parenthsis.  The real problem is that it's actually harder in some
    // sense *not* to fully parse a language than it is to parse it.  But we
    // really don't want to put the parsing rules for C into the parser, so
    // we try to treat things like ``if (...)'', ``epilogue {...}'', and
    // ``prologue {...}'' as being simple quoted strings.  Unfortunately
    // we *do* want to parse ``name(parameters)'' in the interface definition.
    // (sigh) So we use this gross hack to keep track of whether the last
    // token we returned was IF.  Then, if we see an open parenthsis and the
    // last token was *not* an IF, we return it as an open parenthsis instead
    // of lexing out a ``parenthsis quoted string.''  I told you it was a
    // Gross Hack ...
    //
    static int saw_if = 0;	// ``one shot'' that is reset on each entry
    int last_was_if;		// recorded value of saw_if on entry

    last_was_if = saw_if;
    saw_if = 0;

"subsystem"		return SUBSYSTEM;
"import"		return IMPORT;
"opcode"		return OPCODE;
"callback"		return CALLBACK;
"synchronous"		return SYNCHRONOUS;
"asynchronous"		return ASYNCHRONOUS;
"if"			{ saw_if = 1; return IF; }
"in"			return IN;
"out"			return OUT;
"inout"			return INOUT;
"pointerto"		return POINTERTO;
"valueof"		return VALUEOF;
"bufferof"		return BUFFEROF;
"vectorof"		return VECTOROF;
"debug_prologue"	return PROLOGUE;
"debug_epilogue"	return EPILOGUE;

"MEMBERSHIP" |
"MAIN"			{
				yylval.strVal = strdup(yytext);
				assert(yylval.strVal);
				return CHANNEL;
			}

"void" |
"char" |
"short" |
"int" |
"long" |
"float" |
"double" 		{
				yylval.strVal = strdup(yytext);
				assert(yylval.strVal);
				return CTYPE;
			}

{identifier}		{
			    yylval.strVal = strdup(yytext);
			    assert(yylval.strVal);
			    return IDENTIFIER;
			}

{integer}		{
			    yylval.intVal = strtol(yytext, NULL, 0);
			    return INTEGER;
			}

{quote_string}		{
			    yylval.strVal = strdup(yytext);
			    assert(yylval.strVal);
			    return QUOTE_STRING;
			}

{angle_string}		{
			    yylval.strVal = strdup(yytext);
			    assert(yylval.strVal);
			    return ANGLE_STRING;
			}

[({]			{
			    if (!last_was_if && yytext[0] == '(')
				return '(';

			    // Start processing a balanced () and {} string
			    // and absorb the entire string into yytext.
			    // Don't make any attempt to match '('s with ')'s
			    // or '{'s with '}'s.  Just count absolute nesting
			    // depth and stop counting when back to 0.  We
			    // do handle scanning past quoted strings and
			    // C-style comments and ignoring ()'s and {}'s
			    // within them.
			    //
			    BEGIN in_nested_string;
			    nesting = 1;
			    yymore();
			}

<in_nested_string>[({]	{
			    nesting++;
			    yymore();
			}

<in_nested_string>[)}]	{
			    nesting--;
			    if (nesting == 0) {
				BEGIN INITIAL;
				yylval.strVal = strdup(yytext);
				assert(yylval.strVal);
				return yytext[0] == '('
					? PAREN_STRING : BRACE_STRING;
			    }
			    yymore();
			}

<in_nested_string>{quote_string} |
<in_nested_string>{squote_string} |
<in_nested_string>{any} yymore();

{whitespace}		;
.			return yytext[0];

^#[ \t]*(line)?[ \t]*[0-9]+[ \t]*\"[^"]*\"[ \t]*$ {
			    // Stumbled across a CPP #line directive.  Scan the
			    // line number and filename out of the beast and
			    // continue.
			    //
			    char *p1, *p2;
			    int len;

			    // scan out file name
			    for (p2 = yytext+yyleng-1; *p2 != '"'; p2--);
			    for (p1 = p2-1; *p1 != '"'; p1--);
			    len = p2-p1-1;
			    if (len > 0) {
				memcpy(file, p1+1, len);
				file[len] = '\0';
			    } else
				memcpy(file, "stdin", 6);

			    // scan out line number
			    while (!isdigit(*--p1));
			    while (isdigit(*--p1));
			    lineno = strtol(p1+1, NULL, 10) - 1;
			}

%%

static char *
strndup(const char *str, int maxlen)
    //
    // Copy at most maxlen characters into new()'ed storage and return a
    // pointer to the NULL terminated, new()'ed string.
    //
{
    char *ep = (char *)str;
    while (maxlen > 0 && *ep) {
	maxlen--;
	ep++;
    }
    int len = ep-str;
    char *newstr = new char[len+1];
    assert(newstr);
    memcpy(newstr, str, len);
    newstr[len] = '\0';
    return newstr;
}

static int
yyinput(char *buf, int /*max_len*/)
    //
    // Return the next character in the input.  Read the input a line at a
    // time and parcel the characters out one at a time.  This allows the
    // error routine to print nice error messages with the line and token
    // that the error corresponded to.
    //
{
    if (lineindex >= linelen) {
	// advance line number if last "line" ended in newline
	if (linelen > 0 && line[linelen-1] == '\n') lineno++;

	if (fgets(line, sizeof line, yyin) == NULL) return 0;
	linelen = strlen(line);
	lineindex = 0;
    }
    *buf = line[lineindex++];
    return 1;
}

int
yywrap()
    //
    // Always return 1 indicating that there's no more input to read.  We let
    // CPP handle things like #include's, etc.
    //
{
    return 1;
}


int
yyerror(char *msg, ...)
    //
    // Print an error message, the offending line, and a line with a caret (^)
    // underneath the offending line pointing [hopefully] at the offending
    // token.
    //
{
    // Construct a buffer of spaces and tabs with a trailing caret to print
    // underneath the offending line.  Use tabs in the line positions in the
    // original line that contain tabs, and spaces everywhere else.  This
    // should end up lining the caret with the token in the input line that
    // offended the parser.  Note that the token's position is the next read
    // position in the line minus the length of the last token minus 1 for
    // the read ahead character that stoped the lexical parse for the token.
    //
    int errpos = lineindex-strlen(yytext)-1;
    if (errpos < 0) errpos = 0;
    char posbuf[sizeof line];
    for (int i = 0; i < errpos; i++)
	posbuf[i] = line[i] == '\t' ? '\t' : ' ';
    posbuf[errpos] = '^';
    posbuf[errpos+1] = '\0';

    // Print the error message.
    //
    fprintf(stderr, "\"%s\", line %d: ", file, lineno);

    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    fprintf(stderr, (line[linelen-1] == '\n' ? "\n%s%s\n" : "\n%s\n%s\n"),
	    line, posbuf);

    // Increment the number of compilation errors and return.
    //
    compilation_errors++;
    return 0;
}
