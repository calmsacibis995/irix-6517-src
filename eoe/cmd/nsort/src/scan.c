/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 *	$Ordinal-Id: scan.c,v 1.24 1996/10/22 00:22:38 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"

/*
** A few of keyword <-> token arrays
**	The first is an alphabetically ordered string -> toktype_t lookup table
**	It is sorted by both
**		- the toktype_t enumeration value, and
**		- the keyword string which maps to it. 
**	So the enum in parse.h has to be ordered alphabetically by keyword.
**	All non-keyword tokens (parentheses, etc) are after the keywords.
**	The second maps a letter to the 'slot' in the first array
**	that contains the first keyword starting with that letter.
*/
const keyword_t NsortKeywords[] =
{
    tokEOF,		"~eof",
/*  tokAPPEND,		"append", */
    tokASCENDING,	"ascending",
    tokBINARYINT,	"binary",	/* binary int, not binary string */
    tokBLANKconst,	"blank",
    tokBUFFERED,	"buffered",
    tokCHARACTER,	"character",
/*  tokCHECKSEQUENCE,	"checksequence", */
/*  tokCOLLATINGSEQUENCE, "collatingsequence", */
    tokCOMMAconst,	"comma",
/*  tokCONDITION,	"condition", */
    tokCOPYWRITE,	"copieswrite",
    tokCOUNT,		"count",
/*  tokDATA,		"data",	*/
    tokDECIMAL,		"decimal",
    tokDELIMITER,	"delimiter",
    tokDERIVED,		"derived",
    tokDESCENDING,	"descending",
/*  tokDIGITS,		"digits", */
    tokDIRECT,		"direct",
    tokDOUBLE,		"double",
    tokDUPLICATES,	"duplicates",
/*  tokELSE,		"else",	*/
    tokFIELD,		"field",
/*  tokFILESIZE,	"filesize", */
    tokFILESYS,		"filesystem",
/*  tokFIXED,		"fixed", */
    tokFLOAT,		"float",
/*  tokFOLD,		"fold", */
    tokFORMAT,		"format",
    tokHASH,		"hash",
/*  tokIF,		"if",	*/
/*  tokINCLUDE,		"include",	*/
    tokINFILE,		"infile",
    tokINTEGER,		"integer",
    tokKEY,		"key",
    tokLRL,		"lrl",
    tokMAPPED,		"mapped",
    tokMAXIMUM,		"maximumlength",
    tokMEMORY,		"memory",
    tokMERGE,		"merge",
    tokMETHOD,		"method",
    tokMINIMUM,		"minimumlength",
/*  tokMODIFICATION,	"modification", */
    tokNAME,		"name",
    tokNEWLINEconst,	"newline",
    tokNLconst,		"nl",
/*  tokNOCHECKSEQUENCE,	"nochecksequence", */
    tokNODUPLICATES,	"noduplicates",
    tokNOHASH,		"nohash",
#if defined(tokPREHASH)
    tokNOPREHASH,	"noprehash",
#endif
    tokNOSTABLE,	"nostable",
    tokNOSTATISTICS,	"nostatistics",
    tokNOTOUCHMEM,	"notouchmemory",
    tokNULLconst,	"null",
    tokNUMBER,		"number",
    tokOFFSET,		"offset",
/*  tokOMIT,		"omit",	*/
    tokOUTFILE,		"outfile",
/*  tokOVERLAY,		"overlay",	*/
/*  tokPACKED,		"packed", */
    tokPAD,		"pad",
/*  tokPADKEY,		"padkey", */
/*  tokPADRECORD,	"padrecord", */
    tokPOINTER,		"pointer",
    tokPOSITION,	"position",
#if defined(tokPREHASH)
    tokPREHASH,		"prehash",
#endif
    tokPROCESSES,	"processes",
    tokRADIX,		"radix",
    tokRECORD,		"record",
    tokRECORDCOUNT,	"recordcount",
    tokRECDELIM,	"recorddelimiter",
    tokRECSIZE,		"record_size",
    tokSFLOAT,		"s_floating",
/*  tokSEQUENCE,	"sequence", */
    tokSIZE,		"size",
    tokSPACEconst,	"space",
    tokSPECIFICATION,	"specification",
    tokSTABLE,		"stable",
    tokSTATISTICS,	"statistics",
    tokSUMMARIZE,	"summarize",
    tokSYNC,		"sync",
    tokTFLOAT,		"t_floating",
    tokTABconst,	"tab",
    tokTEMPFILE,	"tempfile",
/*  tokTHEN,		"then", */
    tokTOUCHMEM,	"touchmemory",
    tokTRANSFERSIZE,	"transfersize",
    tokTWOPASS,		"twopass",
    tokUNSIGNED,	"unsigned",
    tokVALUE,		"value",
    tokVARIABLE,	"variable",
    tokWIZARD,		"wizard",
    tokINTEGERVALUE,	"<integer>",
    tokDOUBLEVALUE,	"<float>",
    tokSTRINGVALUE,	"<string>",
    tokCHARVALUE,	"<character>",
    tokIDENTIFIER,      "<field name>",
    tokFILENAME,	"<filename>",
    tokAMBIGUOUSKEYWORD,"<ambiguous keyword prefix>",
    tokSLASH,		"/",
    tokDASH,		"-",
    tokCOMMA,		",",
    tokASSIGNVALUE,     "either ':' or '/'",
    tokLEFTPAREN,	"(",
    tokRIGHTPAREN,	")",
    tokUNKNOWN,         "<unrecognized input>"
};
short KeywordStarts['z' - 'a' + 1] = { 0 };	/* 0 == tokEOF, not a keyword */

/*
 * get_token
 *
 */
toktype_t get_token(sort_t *sort, const char **charp, token_t *token, const toktype_t *expecting)
{
    const char	*p;
    int		i;
    i8		ll;
    const keyword_t *kw;
    char	ch;

    p = *charp;
    while (isspace(*p))
    {
	if (*p == '\n')
	{
newline:			/* jumped to at end of a // or -- comment */
	    PARSE.lineno++;
	    if (p[1] != '\0')
	    {
		PARSE.charno = 0;
		PARSE.line = &p[1];
	    }
	}
	p++;
    }
    token->lineno = PARSE.lineno;
    token->charno = (int) (p - PARSE.line);
    token->line = PARSE.line;

    ch = *p;
    if (isalpha(ch))
    {
	if (expecting != NULL && (kw = keyword_lookup(sort, token, p, expecting)) != NULL)
	{
	    /*
	    ** Some keywords are actually names for common characters
	    */
	    switch (kw->value)
	    {
	      case tokNEWLINEconst:
	      case tokNLconst:
		token->toktype = tokCHARVALUE;
		token->value.strval[0] = '\n';
		break;

	      case tokBLANKconst:
	      case tokSPACEconst:
		token->toktype = tokCHARVALUE;
		token->value.strval[0] = ' ';
		break;

	      case tokNULLconst:
		token->toktype = tokCHARVALUE;
		token->value.strval[0] = '\0';
		break;

	      case tokTABconst:
		token->toktype = tokCHARVALUE;
		token->value.strval[0] = '\t';
		break;

	      case tokCOMMAconst:
		token->toktype = tokCHARVALUE;
		token->value.strval[0] = ',';
		break;

	      default:
		token->toktype = kw->value;
		break;
	    }
#if 0
	    else if (kw->value == tokAMBIGUOUSKEYWORD)
		     kw->value > tokLASTKEYWORD)
	    {
		fprintf(stderr, "At %s, lookup got %s", p, (kw == NULL) ? "nothing" : kw->name);
		token->toktype = tokIDENTIFIER;
		/*
		parser_error(sort, token, NSORT_UNKNOWN_KEYWORD);
		*/
		token->toktype = tokUNKNOWN;
	    }
	    else
	    {
		fprintf(stderr, "At %s, lookup found kw %s", p, kw->name);
		parser_error(sort, token, NSORT_UNEXPECTED_KEYWORD);
		token->toktype = tokUNKNOWN;
	    }
#endif

	    /*
	    ** Skip to the end of the (possibly abbreviated) keyword 
	    */
	    while (IsIdentifier(*p))
		p++;
	    *charp = p;
	    return (token->toktype);	/* early return here */
	}
	
	/*
	 * It isn't a keyword, treat it as an identifier (field name)
	 * or a file name if we are expecting a filename here
	 */
	if (expecting != NULL && expecting[0] == tokFILENAME)
		goto scan_filename;
	for (i = 0; i < sizeof(token->value.strval) - 1 && IsIdentifier(*p); i++, p++)
	{
	    token->value.strval[i] = *p;
	}
	token->value.strval[i] = '\0';
	token->toktype = tokIDENTIFIER;
    }
    else if (expecting != NULL && expecting[0] == tokFILENAME)
    {
	/*
	 */
scan_filename:
	for (i = 0; ; p++)
	{
	    /* End the filename at a null or an unescaped filename delimiter
	     * Gobble up '\'s used as escape characters.
	     */
	    if (*p == '\\')
	    {
		continue;
	    }
	    if (*p == '\0')
		break;
	    if (strchr(",()\n", *p) != NULL)
		break;
	    token->value.strval[i] = *p;
	    i++;
	}
	token->value.strval[i] = '\0';
	token->toktype = tokFILENAME;
    }
    else if (isdigit(ch))
    {
	ll = strtoll(p, (char **) &p, 10);
	token->value.i8val = ll;
	token->toktype = tokINTEGERVALUE;
    }
    else if ((ch == '!' || ch == '#') /* && p == PARSE.line */ )
    {
	do				/* a ! or # comment until eol */
	{
	    p++;
	    if (*p == '\n')
		goto newline;
	} while (*p != '\0');
	token->toktype = tokEOF;
    }
    else if (ch == '/')
    {
	token->toktype = tokSLASH;
	p++;
    }
    else if (ch == ',')
    {
	token->toktype = tokCOMMA;
	p++;
    }
    else if (ch == '-')
    {
	token->toktype = tokDASH;
	p++;
    }
    else if (ch == '(')
    {
	token->toktype = tokLEFTPAREN;
	p++;
    }
    else if (ch == ')')
    {
	token->toktype = tokRIGHTPAREN;
	p++;
    }
    else if (ch == ':' || ch == '=')
    {
	token->toktype = tokASSIGNVALUE;
	p++;
    }
    else if (ch == '"')
    {
	for (i = 0; i < sizeof(token->value.strval) - 1; i++)
	{
	    ch = *++p;
	    if (ch == '"')
		break;
	    if (ch == '\\')
	    {
		ch = *++p;
		if (ch == '\0')		/* a backslash right before EOF */
		{
		    token->toktype = tokEOF;
		    break;
		}
		else if (ch == '0')
		{
		    long val;
		    val = strtol(p, (char **) charp, 0);
		    if (val > UCHAR_MAX)
			parser_error(sort, token, NSORT_CHARACTER_TOO_LARGE);
		    ch = i;
		    p = *charp;
		}
		else if (ch == 't')
		{
		    ch = '\t';
		}
		else if (ch == 'n')
		{
		    ch = '\n';
		}
	    }
	    token->value.strval[i] = ch;
	}
	if (*p != '"')
	    parser_error(sort, token, NSORT_MISSING_QUOTE);
	else
	    token->toktype = tokCHARVALUE;
	p++;
    }
    else if (ch == '\'')
    {
	ch = *++p;
	if (ch == '\\')
	{
	    ch = *++p;
	    if (ch == '0')
	    {
		ll = strtol(p - 1, (char **) charp, 0);
		if (ll > UCHAR_MAX)
		    parser_error(sort, token, NSORT_CHARACTER_TOO_LARGE);
		ch = ll;
		p = *charp;
		if (*p != '\'')
		    parser_error(sort, token, NSORT_BAD_CHARACTER_SPEC);
	    }
	    else if (ch == 't')
		ch = '\t';
	    else if (ch == 'n')
		ch = '\n';

	}
	if (*++p != '\'')
	    parser_error(sort, token, NSORT_MISSING_QUOTE);
	p++;
	token->value.strval[0] = ch;
	token->toktype = tokCHARVALUE;
    }
    else if (ch == '\0')
	token->toktype = tokEOF;
    else
	token->toktype = tokUNKNOWN;

    *charp = p;
    return (token->toktype);
}

const char *unget_token(sort_t *sort, const token_t *token)
{
    PARSE.lineno = token->lineno;
    PARSE.line = token->line;
    return (PARSE.line + token->charno);
}

/*
 * skipcomma - see if there is another comma-separated qualifier in this stmt
 */
int skipcomma(sort_t *sort, const char **strp, int prior_errs)
{
    token_t	token;

    /* don't continue processing a comma-list past the first error
     */
    if (prior_errs != sort->n_errors)
	return (FALSE);
    if (get_token(sort, strp, &token, NULL) == tokCOMMA)
	return (TRUE);

    if (token.toktype != tokEOF && token.toktype != tokRIGHTPAREN &&
	prior_errs == sort->n_errors)
	parser_error(sort, &token, NSORT_EXTRA_INSIDE_STATEMENT);

    *strp = unget_token(sort, &token);
    return (FALSE);
}

void print_token_list(const toktype_t *expecting, int margin, int all_tokens)
{
    toktype_t	toktype;
    int		col = margin;
    int		len;

    while ((toktype = *expecting) != tokEOF)
    {
	if (all_tokens || isalpha(NsortKeywords[toktype].name[0]))
	{
	    len = (int) strlen(NsortKeywords[toktype].name) + 1;/* 1 for blank*/
	    col += len;
	    if (col >= 80)
	    {
		fprintf(stderr, "\n%*s", margin, "");
		col = len + margin;
	    }
	    fprintf(stderr, "%s ", NsortKeywords[toktype].name);
	}
	expecting++;
    }
}

/*
 * keyword_lookup - see if an identifier is a member of
 *		    the set of 'expected' keywords.
 *
 *	This permits minimal disambiguous prefixes of a keyword to
 *	to be used, e.g.:
 *	    keyword_lookup("as", {tokAPPEND, tokCHARACTER, tokASCENDING})
 *		-> tokASCENDING
 *	    keyword_lookup("a", {tokAPPEND, tokCHARACTER, tokASCENDING})
 *		-> raise error, ambiguous
 *	    keyword_lookup("ascent", {tokAPPEND, tokCHARACTER, tokASCENDING})
 *		-> NULL, raise error or not?
 *
 *	If non-keyword identifier-like string are ok
 *	(either filenames or field names but not both)
 *	then the tokIDENTIFIER or the tokFILENAME must be the first element
 *	in the expecting set in order to be recognized correctly.
 */
const keyword_t *keyword_lookup(sort_t *sort,
				token_t *token,
				const char *text,
				const toktype_t *expecting)
{
    const keyword_t *kw;
    char	ch;
    int		i;
    int		j;
    int		expect_count;
    int		match;
    int		matchcount;
    unsigned	keywords_only;
    u1		possible[MAX_EXPECTING_SET + 1];

    /*
    ** Initialize the array which maps a lower-case character to the
    ** first keyword which starts with it.
    */
    if (KeywordStarts[0] == 0)
    {
	for (ch = 'a', kw = &NsortKeywords[1]; ch <= 'z'; ch++)
	{
	    KeywordStarts[ch - 'a'] = (short) (kw - NsortKeywords);
	    while (kw->name[0] == ch)
		kw++;
	}
    }

    keywords_only = expecting[0] != tokIDENTIFIER &&
		    expecting[0] != tokFILENAME;
    for (expect_count = 0; expecting[expect_count] != tokEOF; expect_count++)
    {
	possible[expect_count] = TRUE;
    }
    possible[expect_count] = FALSE;
    if (expect_count > MAX_EXPECTING_SET)
	die("keyword_lookup:missing token terminator");

    /*
     * Compare each successive character in the string 
     * to the corresponding character in each of the expected keywords
     * which is still a possible match.
     */
    for (i = 0; IsIdentifier(ch = tolower(text[i])); i++)
    {
	for (j = 0, matchcount = 0; j < expect_count; j++)
	{
	    if (possible[j])
	    {
		if (ch == NsortKeywords[expecting[j]].name[i])
		{
		    matchcount++;
		    match = j;
		}
		else
		{
		    possible[j] = FALSE;
		}
	    }
	}

	if (matchcount == 0)		/* none left in possible[] */
	{
#if 0
	    if (expecting == NsortKeywordTokens)
		return (NULL);
#endif
	    if (keywords_only && sort != NULL)
	    {
		parser_error(sort, token, NSORT_UNEXPECTED_KEYWORD);
		print_token_list(expecting, 0, FALSE);
		putc('\n', stderr);
	    }
	    return (NULL);
#if 0
	    if ((kw = keyword_lookup(NULL, token, text, NsortKeywordTokens)) == NULL ||
	        kw->value > tokLASTKEYWORD)
	    {
		fprintf(stderr, "kw_look:At %s, recurse lookup got %s\n",
				text, (kw == NULL) ? "kw-nothing" : kw->name);
		if (keywords_only)
		    return (/* &NsortKeywords[tokUNKNOWN]*/ NULL);
		else
		    return (&NsortKeywords[expecting[0]]);
	    }
	    else
	    {
		fprintf(stderr, "kw_At %s, lookup found kw %s", text, kw->name);
		return (kw);
	    }
	    if (keywords_only)
	    else
		return (&NsortKeywords[tokUNKNOWN]);
#endif
	}
    }

    if (matchcount == 1)
	return (&NsortKeywords[expecting[match]]);
    else	/* matchcount > 1 ==> ambiguous */
    {
	if (!keywords_only)
	    return (NULL);
	else if (sort != NULL)
	{
	    parser_error(sort, token, NSORT_AMBIGUOUS_IDENTIFIER);
	    for (j = 0; j < expect_count; j++)
		if (possible[j])
		    fprintf(stderr, "%s ", NsortKeywords[expecting[j]].name);
	    fputs("\n\n", stderr);
	}
	return (&NsortKeywords[tokAMBIGUOUSKEYWORD]);
    }
}

/*
 * matching_paren	- find a right parentheses to match the left paren
 *			  previously seen
 *	
 */
void matching_paren(sort_t *sort, const char **strp, token_t *token)
{
    if (get_token(sort, strp, token, NULL) != tokRIGHTPAREN)
    {
	parser_error(sort, token, NSORT_PAREN_EXPECTED_AFTER);
	*strp = unget_token(sort, token);
    }
}

/*
 * expect_token	- see if an expected token is the next one in the input
 *		  stream.  Optionally raise an error if it isn't found
 *
 *	This is frequently used for both required language characters
 *	(e.g. ':' or '=', the 'assignment' token) and optional tokens
 *	such as the parentheses with are permitted around many
 *	commands' qualification lists.
 */
int expect_token(sort_t		*sort,
		 const char	**strp,
		 token_t	*token,
		 toktype_t	expecting,
		 nsort_msg_t	err)
{
    token_t	localtoken;

    if (get_token(sort, strp, &localtoken, NULL) == expecting)
	return (TRUE);

    *strp = unget_token(sort, &localtoken);
    if (err != NSORT_NOERROR)
	parser_error(sort, token, err);
    return (FALSE);
}
