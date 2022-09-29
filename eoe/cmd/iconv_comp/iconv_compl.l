
/**************************************************************************
 *                                                                        *
 * Copyright (C) 1995 Silicon Graphics, Inc.                         *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

%{

/*
 * iconv_compl.l
 *	lex spec for iconv compiler
 */

#include "in_proc.h"
#include "y.tab.h"
#include "iconv_comp.h"
#undef getc
#define getc	inx_getc

%}

%%

[\011\n ]				;
(((#)([^\n]*))(\n))			;
(((\/\/)([^\n]*))(\n))			;
(;)					{ return ICC_END; }
(\,)					{ return ','; }
(\*)					{ return '*'; }
(=)					{ return '='; }
(\})					{ return '}'; }
(\{)					{ return '{'; }
((-)?)(0x)[0-9A-Fa-f]([0-9A-Fa-f]*)	{ icc_fixident(); return ICC_INTEGER; }
((-)?)[0-9]([0-9]*)			{ icc_fixident(); return ICC_INTEGER; }
((-)|(none)|(NONE))			{ return ICC_NONE; }
((file)|(FILE))				{ return ICC_FILE; }
((clone)|(CLONE))			{ return ICC_CLONE; }
((join)|(JOIN))				{ return ICC_JOIN; }
((use)|(USE))				{ return ICC_USE; }
((mbwc)|(MBWC))				{ return ICC_MBWC; }
((wcmb)|(WCMB))				{ return ICC_WCMB; }
((locale_alias)|(LOCALE_ALIAS))		{ return ICC_LOCALE_ALIAS; }
((locale_codeset)|(LOCALE_CODESET))	{ return ICC_LOCALE_CODESET; }
((resource)|(RESOURCE))			{ return ICC_RESOURCE; }
((\")((((((((\\)?)[^\n\"\\])|(\\\"))|(\\\n))|(\\\\))*)(\")))	{ icc_fixstr(); return ICC_STRING; }
([\$A-Z_a-z]([\$0-9A-Z_a-z]*))		{ icc_fixident(); return ICC_IDENTIFIER; }
(.)					{ lexerror(); }
%%
