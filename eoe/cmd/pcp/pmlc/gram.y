/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

%{
#ident "$Id: gram.y,v 2.9 1997/09/12 01:07:42 markgw Exp $"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "pmapi_dev.h"
#include "./pmlc.h"

int		mystate = GLOBAL;
int		mynumb;
int		logfreq;
int		parse_stmt;
char		emess[160];
char		*hostname;
int		state;
int		control;

static char	*name;
static int	sts;

extern int	port;
extern int	pid;

extern char	yytext[];
extern int	errno;

%}

%term	LSQB
	RSQB
	COMMA
	LBRACE
	RBRACE
	AT
	EOL

	LOG
	MANDATORY ADVISORY
	ON OFF MAYBE
	EVERY ONCE
	MSEC SECOND MINUTE HOUR

	QUERY SHOW LOGGER CONNECT PRIMARY QUIT STATUS HELP
	TIMEZONE LOCAL PORT
	NEW VOLUME

	SYNC

	NAME
	HOSTNAME
	NUMBER
	STRING
%%

stmt    : dowhat					{
		    mystate |= INSPEC;
		    if (!connected()) {
			metric_cnt = -1;
			return 0;
		    }
		    if (ConnectPMCD()) {
			yyerror("");
			metric_cnt = -1;
			return 0;
		    }
		    beginmetrics();
		}
	  somemetrics					{
		    mystate = GLOBAL;
		    endmetrics();
		}
	  EOL						{
		    parse_stmt = LOG;
		    YYACCEPT;
		}
	| SHOW loggersopt hostopt EOL			{
		    parse_stmt = SHOW;
		    YYACCEPT;
		}
	| CONNECT towhom hostopt EOL			{
		    parse_stmt = CONNECT;
		    YYACCEPT;
		}
	| HELP EOL					{
		    parse_stmt = HELP;
		    YYACCEPT;
		}
	| QUIT EOL					{
		    parse_stmt = QUIT;
		    YYACCEPT;
		}
	| STATUS EOL					{
		    parse_stmt = STATUS;
		    YYACCEPT;
		}
	| NEW VOLUME EOL				{
		    parse_stmt = NEW;
		    YYACCEPT;
		}
	| TIMEZONE tzspec EOL				{
		    parse_stmt = TIMEZONE;
		    YYACCEPT;
		}
	| SYNC EOL					{
		    parse_stmt = SYNC;
		    YYACCEPT;
		}
	| EOL						{
		    parse_stmt = 0;
		    YYACCEPT;
		}
	|						{ YYERROR; }
	;

dowhat	: action 
	;

action	: QUERY					{ state = PM_LOG_ENQUIRE; }
	| logopt cntrl ON frequency		{ state = PM_LOG_ON; }
	| logopt cntrl OFF			{ state = PM_LOG_OFF; }
	| logopt MANDATORY MAYBE		{
		    $$ = LOG;
		    control = PM_LOG_MANDATORY;
		    state = PM_LOG_MAYBE;
		}
	;

logopt	: LOG 
	| /* nothing */			{ $$ = LOG; }
	;

cntrl	: MANDATORY			{ control = PM_LOG_MANDATORY; }
	| ADVISORY			{ control = PM_LOG_ADVISORY; }
	;

frequency	: everyopt NUMBER timeunits	{ logfreq = mynumb * $3; }
		| ONCE				{ logfreq = -1; }
		;

everyopt	: EVERY
		| /* nothing */
		;

timeunits	: MSEC		{ $$ = 1; }
		| SECOND	{ $$ = 1000; }
		| MINUTE	{ $$ = 60000; }
		| HOUR		{ $$ = 3600000; }
		;

somemetrics	: LBRACE { mystate |= INSPECLIST; } metriclist RBRACE
		| metricspec
		;

metriclist	: metricspec
		| metriclist metricspec
		| metriclist COMMA metricspec
		;

metricspec	: NAME					{
			    name = strdup(yytext);
			    if (name == NULL) {
				__pmNoMem("copying metric name", strlen(yytext), PM_FATAL_ERR);
				/*NOTREACHED*/
			    }
			    beginmetgrp();
			    if ((sts = pmTraversePMNS(name, addmetric)) < 0)
				/* metric_cnt is set by addmetric but if
				 * traversePMNS fails, set it so that the bad
				 * news is visible to other routines
				 */
				metric_cnt = sts;
			    else if (metric_cnt < 0) /* addmetric failed */
				sts = metric_cnt;
			    if (sts < 0 || metric_cnt == 0) {
				sprintf(emess, "Problem with lookup for metric \"%s\" ...", name);
				yywarn(emess);
				if (sts < 0) {
				    fprintf(stderr, "Reason: ");
				    if (still_connected(sts))
					fprintf(stderr, "%s\n", pmErrStr(sts));
				}
			    }
			    free(name);
			}
		  optinst				{ endmetgrp(); }
		;

optinst		: LSQB instancelist RSQB
		| /* nothing */
		;

instancelist	: instance
		| instance instancelist
		| instance COMMA instancelist
		;

instance	: NAME			{ addinst(yytext, 0); }
		| NUMBER		{ addinst(NULL, mynumb); }
		| STRING				{
			    yytext[strlen(yytext)-1] = '\0';
			    addinst(&yytext[1], 0);
			}
		;

loggersopt	: LOGGER
		| /* nothing */
		;

hostopt		: AT NAME		{ hostname = strdup(yytext); }
		| AT HOSTNAME		{ hostname = strdup(yytext); }
		| AT NUMBER		{ hostname = strdup(yytext); }
		| AT STRING		{
			    /* strip delimiters */
			    yytext[strlen(yytext)-1] = '\0';
			    hostname = strdup(&yytext[1]);
			}
		| /* nothing */
		;

towhom		: PRIMARY		{ pid = PM_LOG_PRIMARY_PID; port = PM_LOG_NO_PORT; }
		| NUMBER		{ pid = mynumb; port = PM_LOG_NO_PORT; }
		| PORT NUMBER		{ pid = PM_LOG_NO_PID; port = mynumb; }
		;

tzspec		: LOCAL			{ tztype = TZ_LOCAL; }
		| LOGGER		{ tztype = TZ_LOGGER; }
		| STRING		{
			    tztype = TZ_OTHER;
			    /* ignore the quotes: skip the leading one and
			     * clobber the trailing one with a null to
			     * terminate the string really required.
			     */
			    yytext[strlen(yytext)-1] = '\0';
			    if (tz != NULL)
				free(tz);
			    if ((tz = strdup(&yytext[1])) == NULL) {
				__pmNoMem("setting up timezone",
					 strlen(&yytext[1]), PM_FATAL_ERR);
				/*NOTREACHED*/
			    }
			}
		;

%%
