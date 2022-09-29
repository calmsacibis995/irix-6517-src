/*
 * FILE: eoe/cmd/miser/lib/libmiser/src/parse_gram.c
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
#include <sys/types.h>
#include <sys/miser_public.h>
#include "libmiser.h"


static miser_seg_t *	yyjseg;
static miser_qseg_t *	yyqseg;
static miser_qhdr_t *	yyqhdr;



void
yyerror(const char *msg)
{
	parse_error("%s", msg);
}
%}

%union {
	uint64_t	v_int;
	char *		v_str;
}

%start	parse
%token			SEL_JSUB SEL_QMOV SEL_QDEF
%token			KWD_SEGMENT KWD_MULTIPLE KWD_PRIORITY KWD_STATIC
%token			KWD_EXCEPT KWD_KILL KWD_WTLESS
%token			KWD_TIME KWD_CTIME KWD_START KWD_END
%token			KWD_MEMORY KWD_NCPUS
%token			KWD_POLICY KWD_QUANTUM KWD_NSEG KWD_QUEUE
%token			TOK_ERR
%token <v_int>		TOK_INT
%token <v_str>		TOK_STR
%%

parse:		SEL_JSUB jsegs		{ }
	|	SEL_QMOV qsegs		{ }
	|	SEL_QDEF qdefs		{ }
	;

jsegs:		jseg jsegs		{ }
	|	jseg			{ }
	;

jseg:		KWD_SEGMENT		{
			if (!(yyjseg = parse_jseg_start())) {
				YYERROR;
			}
		}

		jattrs			{ 
			if (!parse_jseg_stop()) {
				YYERROR; 
			}
		}
	;

jattrs:		jattr jattrs		{ }
	|	jattr			{ }
	;

jattr:		KWD_STATIC		{
			yyjseg->ms_flags &= ~MISER_SEG_DESCR_FLAG_NORMAL;
			yyjseg->ms_flags |= MISER_SEG_DESCR_FLAG_STATIC;
		}

	|	KWD_MULTIPLE TOK_INT	{ yyjseg->ms_multiple = $2; }

	|	KWD_PRIORITY TOK_INT	{ yyjseg->ms_priority = $2; }

	|	KWD_EXCEPT KWD_KILL	{
			yyjseg->ms_flags |= MISER_EXCEPTION_TERMINATE;
		}

	|	KWD_EXCEPT KWD_WTLESS	{
			yyjseg->ms_flags |= MISER_EXCEPTION_WEIGHTLESS;
		}

	|	KWD_TIME TOK_STR	{
			if (!fmt_str2time($2, &yyjseg->ms_rtime)) {
				parse_error("invalid time");
				YYERROR;
			}
		}

	|	KWD_CTIME TOK_STR	{
			if (!fmt_str2time($2, &yyjseg->ms_etime)) {
				parse_error("invalid ctime");
				YYERROR;
			}
		}

	|	KWD_MEMORY TOK_STR	{
			if (!fmt_str2bcnt($2, &yyjseg->ms_resources.mr_memory)){
				parse_error("invalid memory size");
				YYERROR;
			}
		}

	|	KWD_NCPUS TOK_INT	{ yyjseg->ms_resources.mr_cpus = $2; }
	;

qdefs:		qdef qdefs		{ }
	|	qdef			{ }
	;

qdef:		KWD_QUEUE TOK_INT TOK_STR	{
			if (!(yyqhdr = parse_qdef_start())) {
				YYERROR;
			}

			yyqhdr->name = $2;
			yyqhdr->fname = $3;
		}

		qhdrs			{ 
			if (!parse_qdef_hdr()) {
				YYERROR; 
			}
		}
	
		qsegs			{ 
			if (!parse_qdef_stop()) {
				YYERROR; 
			}
		}
	;

qhdrs:		qhdr qhdrs		{ }
	|	qhdr			{ }
	;

qhdr:		KWD_POLICY TOK_STR	{ yyqhdr->policy = miser_qid($2); }
	|	KWD_QUANTUM TOK_INT	{ yyqhdr->quantum = $2; }
	|	KWD_NSEG TOK_INT	{ yyqhdr->nseg = $2; }
	;

qsegs:		qseg qsegs		{ }
	|	qseg			{ }
	;

qseg:		KWD_SEGMENT		{
			if (!(yyqseg = parse_qseg_start())) {
				YYERROR;
			}
		}

		qattrs			{ 
			if (!parse_qseg_stop()) {
				YYERROR; 
			}
		}
	;

qattrs:		qattr qattrs		{ }
	|	qattr			{ }
	;

qattr:		KWD_START TOK_STR	{
			if (!fmt_str2time($2, &yyqseg->mq_stime)) {
				parse_error("invalid start time");
				YYERROR;
			}
		}

	|	KWD_END TOK_STR		{
			if (!fmt_str2time($2, &yyqseg->mq_etime)) {
				parse_error("invalid end time");
				YYERROR;
			}
		}

	|	KWD_MEMORY TOK_STR	{
			if (!fmt_str2bcnt($2, &yyqseg->mq_resources.mr_memory)){
				parse_error("invalid memory size");
				YYERROR;
			}
		}

	|	KWD_NCPUS TOK_INT	{ yyqseg->mq_resources.mr_cpus = $2; }
	;
