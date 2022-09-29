/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* Copyright (c) 1981 Regents of the University of California */
#ident	"@(#)vi:port/ex_data.c	1.11.1.2"

#include "ex.h"
#include "ex_tty.h"

/*
 * Initialization of option values.
 * The option #defines in ex_vars.h are made
 * from this file by the script makeoptions.
 *
 * These initializations are done char by char instead of as strings
 * to confuse xstr so it will leave them alone.
 */
unsigned char	direct[ONMSZ] =
	{'/', 't', 'm', 'p'}; 
unsigned char	paragraphs[ONMSZ] = {
	'I', 'P', 'L', 'P', 'P', 'P', 'Q', 'P',		/* -ms macros */
	'P', ' ', 'L', 'I',				/* -mm macros */
	'p', 'p', 'l', 'p', 'i', 'p', 'n', 'p',		/* -me macros */
	'b', 'p'					/* bare nroff */
};
unsigned char	sections[ONMSZ] = {
	'N', 'H', 'S', 'H',				/* -ms macros */
	'H', ' ', 'H', 'U',				/* -mm macros */
	'u', 'h', 's', 'h', '+', 'c'			/* -me macros */
};
unsigned char	shell[ONMSZ] =
	{ '/', 'b', 'i', 'n', '/', 's', 'h' };
unsigned char	tags[ONMSZ] = {
	't', 'a', 'g', 's', ' ',
	'/', 'u', 's', 'r', '/', 'l', 'i', 'b', '/', 't', 'a', 'g', 's'
};
unsigned char termtype[ONMSZ];

struct	option options[vi_NOPTS + 1] = {
	(unsigned char *)"autoindent",	(unsigned char *)"ai",	ONOFF,		0,	0,	0,
	(unsigned char *)"autoprint",	(unsigned char *)"ap",	ONOFF,		1,	1,	0,
	(unsigned char *)"autowrite",	(unsigned char *)"aw",	ONOFF,		0,	0,	0,
	(unsigned char *)"beautify",	(unsigned char *)"bf",	ONOFF,		0,	0,	0,
	(unsigned char *)"directory",	(unsigned char *)"dir",	STRING,		0,	0,	direct,
	(unsigned char *)"edcompatible",	(unsigned char *)"ed",	ONOFF,		0,	0,	0,
	(unsigned char *)"errorbells",	(unsigned char *)"eb",	ONOFF,		0,	0,	0,
	(unsigned char *)"exrc",	(unsigned char *)"ex",	ONOFF,		0,	0,	0,
	(unsigned char *)"flash",	(unsigned char *)"fl",	ONOFF,		1,	1,	0,
	(unsigned char *)"hardtabs",	(unsigned char *)"ht",	NUMERIC,	8,	8,	0,
	(unsigned char *)"ignorecase",	(unsigned char *)"ic",	ONOFF,		0,	0,	0,
	(unsigned char *)"lisp",		0,	ONOFF,		0,	0,	0,
	(unsigned char *)"list",		0,	ONOFF,		0,	0,	0,
	(unsigned char *)"magic",	0,	ONOFF,		1,	1,	0,
	(unsigned char *)"mesg",		0,	ONOFF,		1,	1,	0,
	(unsigned char *)"modelines",	(unsigned char *)"ml",	ONOFF,		0,	0,	0,
	(unsigned char *)"number",	(unsigned char *)"nu",	ONOFF,		0,	0,	0,
	(unsigned char *)"novice",	0,	ONOFF,		0,	0,	0,
	(unsigned char *)"oldparagraphs",	(unsigned char *)"oldpara",	ONOFF,		0,	0,	0,
	(unsigned char *)"optimize",	(unsigned char *)"opt",	ONOFF,		0,	0,	0,
	(unsigned char *)"paragraphs",	(unsigned char *)"para",	STRING,		0,	0,	paragraphs,
	(unsigned char *)"prompt",	0,	ONOFF,		1,	1,	0,
	(unsigned char *)"readonly",	(unsigned char *)"ro",	ONOFF,		0,	0,	0,
	(unsigned char *)"redraw",	0,	ONOFF,		0,	0,	0,
	(unsigned char *)"remap",	0,	ONOFF,		1,	1,	0,
	(unsigned char *)"report",	0,	NUMERIC,	5,	5,	0,
	(unsigned char *)"scroll",	(unsigned char *)"scr",	NUMERIC,	12,	12,	0,
	(unsigned char *)"sections",	(unsigned char *)"sect",	STRING,		0,	0,	sections,
	(unsigned char *)"shell",	(unsigned char *)"sh",	STRING,		0,	0,	shell,
	(unsigned char *)"shiftwidth",	(unsigned char *)"sw",	NUMERIC,	TABS,	TABS,	0,
	(unsigned char *)"showmatch",	(unsigned char *)"sm",	ONOFF,		0,	0,	0,
	(unsigned char *)"showmode",	(unsigned char *)"smd",	ONOFF,		0,	0,	0,
	(unsigned char *)"slowopen",	(unsigned char *)"slow",	ONOFF,		0,	0,	0,
	(unsigned char *)"tabstop",	(unsigned char *)"ts",	NUMERIC,	TABS,	TABS,	0,
	(unsigned char *)"taglength",	(unsigned char *)"tl",	NUMERIC,	0,	0,	0,
	(unsigned char *)"tags",		(unsigned char *)"tag",	STRING,		0,	0,	tags,
	(unsigned char *)"term",		0,	OTERM,		0,	0,	termtype,
	(unsigned char *)"terse",	0,	ONOFF,		0,	0,	0,
#ifndef sgi
	(unsigned char *)"timeout",	(unsigned char *)"to",	ONOFF,		1,	1,	0,
#else
	(unsigned char *)"timeout",     (unsigned char *)"to",  NUMERIC,	200,	200,	0,
#endif
	(unsigned char *)"ttytype",	(unsigned char *)"tty",	OTERM,		0,	0,	termtype,
	(unsigned char *)"warn",		0,	ONOFF,		1,	1,	0,
	(unsigned char *)"window",	(unsigned char *)"wi",	NUMERIC,	23,	23,	0,
	(unsigned char *)"wrapscan",	(unsigned char *)"ws",	ONOFF,		1,	1,	0,
	(unsigned char *)"wrapmargin",	(unsigned char *)"wm",	NUMERIC,	0,	0,	0,
	(unsigned char *)"writeany",	(unsigned char *)"wa",	ONOFF,		0,	0,	0,
	0,		0,	0,		0,	0,	0,
};

/*
 *	Internationalisation definitions (referenced in ex.h).  These are the
 *	common messages:
 */
const	char	EXmronly[] = " File is read only";
const	char	EXmronlyid[] = ":65";
const	char	EXmtmptoobig[] = " Tmp file too large";
const	char	EXmtmptoobigid[] = ":66";
const	char	EXmnewfile[] = " [New file]";
const	char	EXmnewfileid[] = ":67";
const	char	EXmbreadonlyb[] = " [Read only]";
const	char	EXmbreadonlybid[] = ":68";
const	char	EXmequalsm[] = "========  ========\n";
const	char	EXmequalsmid[] = ":69";
const	char	EXmappmode[] = "APPEND MODE";
const	char	EXmappmodeid[] = ":70";
const	char	EXmargtoolong[] = "Arg list too long";
const	char	EXmargtoolongid[] = "uxsyserr:10";
const	char	EXmchamod[] = "CHANGE MODE";
const	char	EXmchamodid[] = ":71";
const	char	EXmcommtoo[] = "Command too long";
const	char	EXmcommtooid[] = ":72";
const	char	EXminpmode[] = "INPUT MODE";
const	char	EXminpmodeid[] = ":73";
const	char	EXminsmode[] = "INSERT MODE";
const	char	EXminsmodeid[] = ":74";
const	char	EXmlitoolong[] = "Line too long";
const	char	EXmlitoolongid[] = ":75";
const	char	EXmmactoolong[] = "Macro too long|Macro too long - maybe recursive?";
const	char	EXmmactoolongid[] = ":76";
const	char	EXmmisslhs[] = "Missing lhs";
const	char	EXmmisslhsid[] = ":77";
const	char	EXmmissrhs[] = "Missing rhs";
const	char	EXmmissrhsid[] = ":78";
const	char	EXmnofile[] = "No file|No current filename";
const	char	EXmnofileid[] = ":79";
const	char	EXmnoproc[] = "No more processes";
const	char	EXmnoprocid[] = "uxsyserr:14";
const	char	EXmnobang[] = "No previous command|No previous command to substitute for !";
const	char	EXmnobangid[] = ":80";
const	char	EXmnopre[] = "No previous re|No previous regular expression";
const	char	EXmnopreid[] = ":81";
const	char	EXmnotail[] = "No tail recursion";
const	char	EXmnotailid[] = ":82";
const	char	EXmemptyreg[] = "Nothing in register %c";
const	char	EXmemptyregid[] = ":83";
const	char	EXmopnmode[] = "OPEN MODE";
const	char	EXmopnmodeid[] = ":84";
const	char	EXmrepmode[] = "REPLACE MODE";
const	char	EXmrepmodeid[] = ":85";
const	char	EXmreppat[] = "Replacement pattern too long|Replacement pattern too long - limit 256 characters";
const	char	EXmreppatid[] = ":86";
const	char	EXmsubmode[] = "SUBSTITUTE MODE";
const	char	EXmsubmodeid[] = ":87";
const	char	EXmregcont[] = "\nRegister  Contents\n";
const	char	EXmregcontid[] = ":88";
const	char	EXmemptyregtwo[] = "\t\tNothing in register.\n";
const	char	EXmemptyregtwoid[] = ":89";
const	char	EXmenterkey[] = "Enter encryption key: ";
const	char	EXmenterkeyid[] = ":90";
const	char	EXmnocrypt[] = "Encryption facility not available\n";
const	char	EXmnocryptid[] = ":91";
const	char	EXmnoaltfile[] = "No alternate filename|No alternate filename to substitute for #";
const	char	EXmnoaltfileid[] = ":92";
const	char	EXmnocopykey[] = " Cannot copy key to environment";
const	char	EXmnocopykeyid[] = ":93";
const	char	EXmmacroloop[] = "Infinite macro loop";
const	char	EXmmacroloopid[] = ":94";
