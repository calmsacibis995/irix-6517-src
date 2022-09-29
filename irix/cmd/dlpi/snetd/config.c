/*
 *  SpiderTCP Network Daemon
 *
 *      Configuration File Parser
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.config.c
 *	@(#)config.c	1.8
 *
 *	Last delta created	17:06:00 2/4/91
 *	This file extracted	14:52:19 11/13/92
 *
 */

#ident "@(#)config.c	1.8	11/13/92"

#include <stdio.h>
#include "debug.h"
#include "utils.h"

extern char	*copy();
extern list_t	mklist();

extern char	*prog, *config;

extern	void insert();
extern	void graph_link();

/*
 *	<configuration>  ::=  <modules> <streams> eof
 *
 *	<modules>        ::=  <modline> <modules> | null
 *
 *	<modline>        ::=  <moddef> | <empty>
 *
 *	<moddef>         ::=  STRING STRING STRING <comment> \n
 *
 *	<empty>          ::=  <comment> \n
 *
 *	<comment>        ::=  COMMENT | null
 *
 *	<streams>        ::=  <delim> <nodes> | null
 *
 *	<delim>          ::=  DELIM \n
 *
 *	<nodes>          ::=  <nodeline> <nodes> | null
 *
 *	<nodeline>       ::=  <nodedef> | <empty>
 *
 *	<nodedef>        ::=  STRING STRING <info> <comment> \n
 *
 *	<info>           ::=  <message> <info> | null
 *
 *	<message>        ::=  <function> | <function> "=" <value>
 *
 *	<function>       ::=  STRING
 *
 *	<value>          ::=  <arg> | "{" <list> "}"
 *
 *	<list>           ::=  <arg> <rest>
 *
 *	<arg>            ::=  STRING
 *
 *	<rest>           ::=  "," <list> | null
 */

static void lexstate();
static void match();

/*
 *  Lexical Tokens.
 */

#define STRING	256
#define COMMENT	'#'
#define NL	'\n'
#define DELIM	'%'

static int	lexan();		/* returns next input token */
static void	modules();
static void	modline();
static void	streams();
static void	comment();
static void	nodes();
static void	nodeline();

/*
 *  Local globals used by parser and lexical analyser.
 */

static int	token;			/* current input token */
static char	*word;			/* value of token */


/*
 *  configure()	--	build the configuration specified in a file
 */

void
configure()
{
	/*
	 *  Initially, equals signs are lexically special.
	 */

	lexstate('=');

	token = lexan();

	XPRINT2("TOKEN %d %s\n", token, token==STRING? word:"");

	/*
	 *  Section 1 - Module/Driver Information.
	 *
	 *  This section contains lines with three fields each,
	 *  i.e. id, flags and name.  Any additional tokens
	 *  should be comments.  Blank lines and whole-line
	 *  comments contain zero fields.  Any other number
	 *  of fields is an error.
	 */

	modules();

	/*
	 *  Section 2 - STREAMS Architecture.
	 *
	 *  The start of this section is delimited from the
	 *  previous section by a line containing "%%".
	 *  This section consists of lists of link dependencies
	 *  of the form
	 *
	 *	up_stream: down_stream
	 *
	 *  followed by a list of strings representing control
	 *  information to be sent to the upstream half of the
	 *  link when the link is actually completed.
	 */

	streams();

	match(EOF);
}


static void
modules()
{
	for (;;) switch (token)
	{
	case STRING:	modline(); break;
	case COMMENT:	comment(); break;
	case NL:	match(NL); break;
	case EOF:	return;
	case DELIM:	return;
	}
}

static void
modline()
{
	char	*id, *flags, *name;

	if (token == STRING) id = copy(word); match(STRING);
	if (token == STRING) flags = copy(word); match(STRING);
	if (token == STRING) name = copy(word); match(STRING);
	comment();
	insert(id, flags, name);
	match(NL);
}

static void
comment()
{
	switch (token)
	{
	case COMMENT:	match(COMMENT); break;
	default:	return;
	}
}

static void
streams()
{
	switch (token)
	{
	case DELIM:	match(DELIM); match(NL); nodes(); break;
	default:	return;
	}
}

static void
nodes()
{
	for (;;) switch (token)
	{
	case STRING:	nodeline(); break;
	case COMMENT:	comment(); break;
	case NL:	match(NL); break;
	case EOF:	return;
	case DELIM:	exit(lerror("unexpected delimiter"));
	}
}

static void
nodeline()
{
	char	*upper, *lower;
	list_t	info = L_NULL, tail;

	if (token == STRING) upper = copy(word); match(STRING);
	if (token == STRING) lower = copy(word); match(STRING);

	/*
	 *  Process control message list...
	 */

	while (token == STRING)
	{
		list_t	message;

		/*
		 *  Control message identifier...
		 *
		 *  Store this at the head of a list, with arguments
		 *  (each of which is also a list) tagged afterwards.
		 */

		message = mklist(copy(word));

		if (info)	/* append to end of list */
		{
			tail->next = mklist((char *) message);
			tail = tail->next;
		}
		else		/* first in list */
		{
			info = mklist((char *) message);
			tail = info;
		}

		match(STRING);

		/*
		 *  Argument list is optional...
		 *
		 *  After the '=' is either:
		 *
		 *   -	an empty list
		 *   -  a single argument
		 *   -  a brace-enclosed, comma-separated list
		 *      of possibly null arguments
		 */

		if (token == '=')
		{
			match('=');

			switch (token)
			{
			case STRING:		/* single argument */

				message->next = mklist(copy(word));
				match(STRING);
				break;

			case '{':		/* brace-enclosed list */

				/*
				 *  Inside the braces in a control message
				 *  specification commas are special, equals
				 *  signs are not.
				 */

				lexstate(',');

				match('{');

				for (;;)
				{
					if (token == STRING)
					{
						message->next =
							mklist(copy(word));
						match(STRING);
					}
					else
						message->next = mklist(NULL);

					if (token == '}')
						break;

					message = message->next;

					match(',');
				}

				/*
				 *  Restore lexical state.
				 */

				lexstate('=');

				match('}');
				break;

			default:		/* empty list */
				break;
			}
		}
	}

	comment();
	graph_link(upper, lower, info);

	match(NL);
}

static void
match(t)
int t;
{
	if (token == t)
	{
		if (token == EOF)
			return;

		token = lexan();
	}
	else
#ifdef DEBUG
		exit(lerror("bad format (token %d, expected %d)", token, t));
#else /*~DEBUG*/
		exit(lerror("bad format"));
#endif /*DEBUG*/

	XPRINT2("TOKEN %d %s\n", token, token==STRING? word:"");
}

/**********************************************************************/

static char	state;		/* lexical state */

/*
 *  lexstate()		--	set lexical state
 */

static void
lexstate(c)
char	c;
{
	state = c;
}

#define MAXLEN	200		/* max. length of a token */

static int lineno = 1;		/* input line number */

/*
 *  lexan()	--	get the next input token
 */

static int
lexan()
{
	static char	buff[MAXLEN+1];
	int		len = 0;
	int		c;

again:
	/* skip whitespace */
	while ((c = getc(stdin)) == ' ' || c == '\t')
		;

	if (c == EOF)		/* end of file */
		return EOF;

	if (c == '\n')		/* newline token */
	{
		++lineno;
		return NL;
	}

	if (c == '#')		/* comment - strip to end of line */
	{
		while ((c = getc(stdin)) != EOF && c != '\n')
			;

		if (c != EOF)
			ungetc(c, stdin);

		return COMMENT;
	}

	/* check for escaped newline */

	if (c == '\\')
	{
		if ((c = getc(stdin)) == '\n')	/* got one */
			goto again;
		else if (c != EOF)
			ungetc(c, stdin);
	}

	/*
	 *  special characters...
	 *
	 *  NB:  state is either '=' (normally) or ',' (inside braces).
	 *
	 *  State is set by the parser.
	 */

	if (c == state || c == '{' || c == '}')
		return c;

	/*  quoted string - gather till matching quote or end of line */

	if (c == '"' || c == '\'')
	{
		char	q = c;		/* quote character */

		while ((c = getc(stdin)) != q && c != EOF && c != '\n')
			if (len < MAXLEN)
				buff[len++] = c;

		buff[len] = '\0';

		/* silently terminate quoted string at end of line... */

		if (c == '\n')
			ungetc(c, stdin);

		word = buff;
		return STRING;
	}

	/* unquoted string - gather till next special character or whitespace */

	while (c != state && c != '{' && c != '}'
	&&     c != EOF && c != ' ' && c != '\t' && c != '\n')
	{
		if (len < MAXLEN)
			buff[len++] = c;

		c = getc(stdin);
	}
	buff[len] = '\0';

	if (c != EOF)
		ungetc(c, stdin);

	/* check for delimiter */

	if (streq(buff, "%%"))
		return DELIM;

	word = buff;
	return STRING;
}

lerror(format, arg1, arg2)
char	*format;
char	*arg1, *arg2;
{
	fprintf(stderr, "%s: \"%s\", line %d: ", prog, config, lineno);
	fprintf(stderr, format, arg1, arg2);
	fprintf(stderr, "\n");
	return(1);
}
