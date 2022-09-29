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
 * parse.h	- nsort-specific structures used in scanning and parsing
 * 		  ordinal's sort definition language
 *
 *	$Ordinal-Id: parse.h,v 1.27 1996/10/22 00:23:56 charles Exp $
 *	$Revision: 1.1 $
 */
#ifndef _PARSE_H_
#define _PARSE_H_

#include <stdlib.h>
#include <string.h>

#include "ordinal.h"

/*
 *	Token type enumeration
 *      Keyword tokens are alphabetically sorted by the keyword string
 *	which is the token's external 'name'
 *      All non-keyword tokens (parentheses, etc) are after the keywords,
 *	except for the end-of-parser-input-file, which is first.
 *
 *	This must match the initialization of NsortKeywords[] in scan.c
 */
typedef enum toktype
{
    tokEOF /* = 0 */,
 /* tokAPPEND,	*/
    tokASCENDING,
    tokBINARYINT,
    tokBLANKconst,
    tokBUFFERED,
    tokCHARACTER,
/*  tokCHECKSEQUENCE, */
/*  tokCOLLATINGSEQUENCE, */
    tokCOMMAconst,
/*  tokCONDITION, */
    tokCOPYWRITE,
    tokCOUNT,
/*  tokDATA, */
    tokDECIMAL,
    tokDELIMITER,
    tokDERIVED,
    tokDESCENDING,
/*  tokDIGITS, */
    tokDIRECT,
    tokDOUBLE,
    tokDUPLICATES,
/*  tokELSE, */
    tokFIELD,
/*  tokFILESIZE, */
    tokFILESYS,
/*  tokFIXED, */
    tokFLOAT,
/*  tokFOLD, */
    tokFORMAT,
    tokHASH,
/*  tokIF, */
/*  tokINCLUDE, */
    tokINFILE,
    tokINTEGER,
    tokKEY,
    tokLRL,
    tokMAPPED,
    tokMAXIMUM,
    tokMEMORY,
    tokMERGE,
    tokMETHOD,
    tokMINIMUM,
/*  tokMODIFICATION, */
    tokNAME,
    tokNEWLINEconst,
    tokNLconst,
/*  tokNOCHECKSEQUENCE, */
    tokNODUPLICATES,
    tokNOHASH,
/*  tokNOPREHASH, */
    tokNOSTABLE,
    tokNOSTATISTICS,
    tokNOTOUCHMEM,
    tokNULLconst,
    tokNUMBER,
    tokOFFSET,
/*  tokOMIT, */
    tokOUTFILE,
/*  tokOVERLAY, */
/*  tokPACKED, */
    tokPAD,
/*  tokPADKEY, */ 
/*  tokPADRECORD, */
    tokPOINTER,
    tokPOSITION,
/*  tokPREHASH, */
    tokPROCESSES,
    tokRADIX,
    tokRECORD,
    tokRECORDCOUNT,
    tokRECDELIM,
    tokRECSIZE,
    tokSFLOAT,
/*  tokSEQUENCE, */
    tokSIZE,
    tokSPACEconst,
    tokSPECIFICATION,
    tokSTABLE,
    tokSTATISTICS,
    tokSUMMARIZE,
    tokSYNC,
    tokTFLOAT,
    tokTABconst,
    tokTEMPFILE,
/*  tokTHEN, */
    tokTOUCHMEM,
    tokTRANSFERSIZE,
    tokTWOPASS,
    tokUNSIGNED,
    tokVALUE,
    tokVARIABLE,
    tokWIZARD,		/* last keyword (alphabetic token) */
    tokLASTKEYWORD = tokWIZARD,

    /* The rest of the tokens are not ordered alphabetically, they are not
     * keywords. They don't start with a letter anyway.
     */
    tokINTEGERVALUE,
    tokDOUBLEVALUE,
    tokSTRINGVALUE,
    tokCHARVALUE,
    tokIDENTIFIER,	/* an identifier that is not a keyword */
    tokFILENAME,	/* a filename not containing ',', '(', ')' */
    tokAMBIGUOUSKEYWORD,/* an insufficiently distinguishing keyword prefix */
    tokSLASH,
    tokDASH,
    tokCOMMA,
    tokASSIGNVALUE,	/* either ':' or '/' */
    tokLEFTPAREN,
    tokRIGHTPAREN,
    tokUNKNOWN,		/* control chars, other punctuation, ... */
    tokLASTTOKEN = tokUNKNOWN
} toktype_t;

#define IsIdentifier(ch)	(isalnum(ch) || (ch) == '_')

#define	MAX_EXPECTING_SET	80

#define MAXSTRSIZE	128

typedef union anyval
{
	char	strval[MAXSTRSIZE + 1];	/* IDENTIFIER, STRING, CHAR, FILENAME */
	i8	i8val;
	i4	i4val;
	i2	i2val;
	i1	i1val;
	u8	u8val;
	u4	u4val;
	u2	u2val;
	u1	u1val;
	float	fval;
	double	dval;
} anyval_t;

typedef struct token
{
	toktype_t	toktype;
	short		lineno;
	short		charno;
	const char	*line;
	anyval_t	value;
} token_t;

typedef struct keyword
{
	toktype_t	value;
	char		*name;
} keyword_t;

/*
** You may index into this array with a toktype_t subscript
** to get the string version of the token
**	NsortKeywords[tokDECIMAL].name --> "decimal"
**	NsortKeywords[tokLEFTPAREN].name --> "("
*/
extern const keyword_t NsortKeywords[];	/* defined in scan.c */

/*
 * Parser error numbers.  The order of items in this define
 * must match the order of the ErrorText[] initialization in error.c.
 *
 * Get the error numbers from the public header nsorterrno.h
 */
typedef nsort_msg_t parse_error_t;

#include "nsorterrno.h"

typedef struct parse_field_flags
{
	unsigned	has_position:1;
	unsigned	has_size:1;
	unsigned	has_delimiter:1;
	unsigned	has_pad:1;
	unsigned	has_type:1;
	unsigned	has_unsigned:1;
	unsigned	has_binary:1;
	unsigned	has_ordering:1;
} parse_field_flags_t;

/*
#define REQUIRE(tok, sort, strp, token, err) \
	if (get_token((sort), (strp), (token), NULL) != (tok)) \
		parser_error((sort), (token), (err))
*/
#define REQUIRE(tok, sort, strp, token, err) expect_token(sort, strp, token, tok, err)
#define optional_paren(sort, strp, token) expect_token(sort, strp, token, tokLEFTPAREN, NSORT_NOERROR)

extern const toktype_t	Statements[];	/* defined in parse.c */
extern const toktype_t	FileSpecs[];	/* defined in parse.c */
extern const toktype_t	InfileSpecs[];	/* defined in parse.c */
extern const toktype_t	OutfileSpecs[];	/* defined in parse.c */

#endif /* _PARSE_H_ */
