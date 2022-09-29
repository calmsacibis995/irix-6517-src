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
 *	$Ordinal-Id: parse.c,v 1.35 1996/11/06 18:55:35 charles Exp $
 */
#ident	"$Revision: 1.2 $"

#include	<sys/types.h>
#include	<sys/resource.h>
#include	<sys/sysmp.h>
#include	<errno.h>
#include	<string.h>
#include	<fcntl.h>
#include	"otcsort.h"

static char	Parse_id[] = "$Ordinal-Revision: 1.35 $";

const toktype_t	Statements[] ={ tokFIELD,	tokKEY,
				tokDERIVED,	/* tokDATA, */
				/*tokINCLUDE,	tokOMIT,*/
				tokINFILE,	tokOUTFILE,
				tokSUMMARIZE,	/* tokCONDITION, */
				tokMEMORY,	tokTWOPASS,
				tokMETHOD,	tokSPECIFICATION,
				tokFORMAT,	tokTEMPFILE,
				tokFILESYS,
				tokPROCESSES,	tokRECORDCOUNT,
				tokTOUCHMEM,	tokNOTOUCHMEM,
				tokSTATISTICS,	tokNOSTATISTICS,
				tokDUPLICATES,	tokNODUPLICATES,
				tokSTABLE,	tokNOSTABLE,
				/* tokCHECKSEQUENCE,tokNOCHECKSEQUENCE, */
				tokEOF };
const toktype_t	CharSpecs[] = { tokBLANKconst,	tokSPACEconst,
				tokCOMMAconst,	tokNEWLINEconst,
				tokNLconst,	tokNULLconst,
				tokTABconst,
				tokEOF };
const toktype_t	FileSpecs[] = {	tokFILENAME,		tokEOF };
const toktype_t FieldSpecs[]	= { tokNAME,		tokPOSITION,
				    tokOFFSET,		tokSIZE,
				    tokUNSIGNED,	tokBINARYINT,
				    tokINTEGER,		tokCHARACTER,
				    tokDECIMAL,		/* tokPACKED, */
				    tokFLOAT,		tokSFLOAT,
				    tokDOUBLE,		tokTFLOAT,
				    tokDELIMITER,	tokPAD,
				    tokEOF };
const toktype_t KeySpecs[]	= { tokIDENTIFIER,
				    tokASCENDING,	tokDESCENDING,
				    tokNUMBER,		tokPOSITION,
				    tokOFFSET,		tokSIZE,
				    tokUNSIGNED,	tokBINARYINT,
				    tokINTEGER,		tokCHARACTER,
				    tokDECIMAL,		/* tokPACKED, */
				    tokFLOAT,		tokSFLOAT,
				    tokDOUBLE,		tokTFLOAT,
				    tokDELIMITER,	tokPAD,
				    tokEOF };
const toktype_t DerivedSpecs[]	= { tokNAME,		tokVALUE,
				    tokPOSITION,	tokOFFSET,
				    tokSIZE,	tokDELIMITER,	tokPAD,
				    tokUNSIGNED,	tokBINARYINT,
				    tokINTEGER,		tokCHARACTER,
				    tokDECIMAL, 	/* tokPACKED, */
				    tokFLOAT,		tokSFLOAT,
				    tokDOUBLE,		tokTFLOAT,
				    tokEOF };
/*const toktype_t DataSpecs[]	= { tokIDENTIFIER, tokEOF };*/
const toktype_t	MethodSpecs[]	= { tokRECORD,	tokPOINTER,
				    tokRADIX,	tokMERGE,
				    tokHASH,	tokNOHASH,
#if defined(tokPREHASH)
				    tokPREHASH,	tokNOPREHASH,
#endif
				    tokEOF };
const toktype_t	StatSpecs[]	= { 
#if defined(tokPREHASH)	
				    tokPREHASH,
#endif
				    tokWIZARD, tokEOF };
const toktype_t	FormatSpecs[]	= { tokSIZE,	tokRECSIZE,	
				    tokDELIMITER, tokRECDELIM,
				    tokMAXIMUM,	tokLRL,
				    tokMINIMUM,	tokEOF };
const toktype_t	RecsizeSpecs[]	= { tokVARIABLE,	tokEOF };
const toktype_t	IntSpecs[]	= { tokINTEGERVALUE,	tokEOF };
const toktype_t UnsignedSpecs[]	= { tokBINARYINT, tokINTEGER, tokEOF};
const toktype_t IdentSpecs[]	= { tokIDENTIFIER, tokEOF};
const toktype_t	InfileSpecs[]	= { tokTRANSFERSIZE, tokCOUNT,
				    tokBUFFERED, tokDIRECT, tokMAPPED, tokEOF };
const toktype_t	OutfileSpecs[]	= { tokTRANSFERSIZE, tokCOUNT,
				    tokCOPYWRITE, tokBUFFERED, tokSYNC,
				    tokDIRECT, tokMAPPED, tokEOF };
const toktype_t	TempSpecs[]	= { tokFILENAME,
				    tokTRANSFERSIZE, tokCOUNT, tokEOF };

typedef struct tokenqual
{
	toktype_t	token;
	const toktype_t	*quals;
} tokenqual_t;
const tokenqual_t StatementQualifiers[] =
{
    tokFIELD,		FieldSpecs,
    tokKEY,		KeySpecs,
    tokDERIVED,		DerivedSpecs,
    tokINFILE,		FileSpecs,
    tokOUTFILE,		FileSpecs,
    tokMEMORY,		IntSpecs,
    tokMETHOD,		MethodSpecs,
    tokSPECIFICATION,	FileSpecs,
    tokFORMAT,		FormatSpecs,
    tokTEMPFILE,	TempSpecs,
    tokFILESYS,		FileSpecs,
    tokPROCESSES,	IntSpecs,
    tokRECORDCOUNT,	IntSpecs,
    tokSUMMARIZE,	IdentSpecs,
    tokTWOPASS,		NULL,
    tokTOUCHMEM,	NULL,
    tokNOTOUCHMEM,	NULL,
    tokSTATISTICS,	NULL,
    tokNOSTATISTICS,	NULL,
    tokDUPLICATES,	NULL,
    tokNODUPLICATES,	NULL,
    tokSTABLE,		NULL,
    tokNOSTABLE,	NULL,
/*  tokCHECKSEQUENCE,	NULL, */
/*  tokNOCHECKSEQUENCE,	NULL, */
    tokEOF,		NULL
};
const tokenqual_t QualQalifiers[] =
{
    tokFILENAME,	OutfileSpecs,
    tokDELIMITER,	CharSpecs,
    tokPAD,		CharSpecs,
    tokEOF,		NULL
};

/*
** parse_type_def	- see if a type definition is here
**
**	Returns:
**		TRUE if and only if the next 'piece' of a
**		/field or /key statement was dealt with here
**		FALSE if the caller has to deal with it.
*/
int parse_type_def(sort_t *sort, const char **def, field_t *field, token_t *token, parse_field_flags_t *pf)
{
    const char	*p = *def;
    unsigned	incomplete_type;

    incomplete_type = !pf->has_type || !(pf->has_size || pf->has_delimiter);
    switch (token->toktype)
    {
      case tokPOSITION:		/* position:N	- first byte has N=1 */
	if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_COLON_EXPECTED))
	    return (TRUE);
	if (get_token(sort, &p, token, NULL) != tokINTEGERVALUE)
	    parser_error(sort, token, NSORT_POSITION_NEEDED);
	else if (token->value.i8val < 1)
	    parser_error(sort, token, NSORT_POSITION_POSITIVE);
	else
	    field->position = token->value.i8val - 1;
	pf->has_position = TRUE;
	if (pf->has_size)
	{
	    /*
	     * Make sure that the field does not extend beyond the end
	     * of the record, if it is fixed length
	     */
check_extent:
	    if ((RECORD.flags & RECORD_FIXED) &&
		field->position + field->length > RECORD.length)
		parser_error(sort, token, NSORT_EXTENDS_PAST_END);
	}
	break;

      case tokOFFSET:		/* offset:N	- first bytes has N=0 */
	if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_COLON_EXPECTED))
	    return (TRUE);
	if (get_token(sort, &p, token, NULL) != tokINTEGERVALUE)
	    parser_error(sort, token, NSORT_POSITION_NEEDED);
	else
	    field->position = token->value.i8val;
	pf->has_position = TRUE;
	if (pf->has_size)
	    goto check_extent;
	break;

      case tokSIZE:		/* [,(size:N | delimiter:C)] */
	if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_COLON_EXPECTED))
	    return (TRUE);
	if (get_token(sort, &p, token, NULL) != tokINTEGERVALUE)
	    parser_error(sort, token, NSORT_FIELD_SIZE_NEEDED);
	else if (pf->has_delimiter)
	    parser_error(sort, token, NSORT_FIELD_SIZE_DELIM);
	else if (pf->has_size)
	    parser_error(sort, token, NSORT_FIELD_ALREADY_SIZED);
	else
	{
	    field->length = token->value.i8val;
	    pf->has_size = TRUE;
	    if (pf->has_position)
	    goto check_extent;
	}
	break;

      case tokDELIMITER:	/* [,(size:N | delimiter:C)] */
	if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_COLON_EXPECTED))
	    return (TRUE);
	if (get_token(sort, &p, token, CharSpecs) != tokCHARVALUE)
	    parser_error(sort, token, NSORT_DELIM_NEEDED);
	else if (pf->has_size)
	    parser_error(sort, token, NSORT_FIELD_SIZE_DELIM);
	else if (pf->has_delimiter)
	    parser_error(sort, token, NSORT_FIELD_ALREADY_DELIMITED);
	else if (pf->has_binary || (pf->has_type && !IsStringType(field->type)))
	    parser_error(sort, token, NSORT_TYPE_NOT_DELIMITABLE);
	if (field->type == typeFixedDecimalString)
#if 0	    /* XXX delimited decimal types aren't fully supported yet
	     * Need to write the initial ovc code for insert_sort()
	     */
	    field->type = typeDelimDecimalString;
#else
	    parser_error(sort, token, NSORT_TYPE_NOT_DELIMITABLE);
#endif
	field->delimiter = token->value.strval[0];
	pf->has_delimiter = TRUE;
	break;

      case tokPAD:		/* pad:C */
	if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_COLON_EXPECTED))
	    return (TRUE);
	if (get_token(sort, &p, token, CharSpecs) != tokCHARVALUE)
	    parser_error(sort, token, NSORT_PAD_NEEDED);
	else
	    field->pad = token->value.strval[0];
	pf->has_pad = TRUE;
	break;

      case tokUNSIGNED:		/* ,unsigned binary, or just ,unsigned, */
	if (pf->has_unsigned)
	    break;		/* XXX raise error on redundant quals */
	pf->has_unsigned = TRUE;
	if (pf->has_type)	/* type decl has been seen already */
	{
	    if (!pf->has_binary)
		parser_error(sort, token, NSORT_UNSIGNED_WITHOUT_TYPE);
	}
	else
	{
	    *def = p;
	    switch (get_token(sort, &p, token, UnsignedSpecs))
	    {
	      case tokBINARYINT:	/* "unsigned binary" both together */
	      case tokINTEGER:		/* "unsigned integer" both together */
		pf->has_binary = TRUE;
		pf->has_type = TRUE;
		break;

	      case tokCOMMA:
	      case tokRIGHTPAREN:
		return (TRUE);

	      default:
		/* The next token was not binary, so this is an
		** unattached 'unsigned' which is not modifying a signed
		** integral type. Return early here.
		parser_error(sort, token, NSORT_UNSIGNED_WITHOUT_TYPE);
		*/
		return (TRUE);
	    }
	}
	break;

      case tokBINARYINT:
      case tokINTEGER:
	if (pf->has_type)
	{
	    parser_error(sort, token, NSORT_FIELD_ALREADY_TYPED);
	    return (TRUE);
	}
	pf->has_type = TRUE;
	pf->has_binary = TRUE;
	break;

      case tokCHARACTER:
	if (pf->has_type)
	{
	    parser_error(sort, token, NSORT_FIELD_ALREADY_TYPED);
	    return (TRUE);
	}
	pf->has_type = TRUE;
	field->type = pf->has_delimiter ? typeDelimString : typeFixedString;
	break;

#if 0
      case tokPACKED:
	if (pf->has_type)
	{
	    parser_error(sort, token, NSORT_FIELD_ALREADY_TYPED);
	    return (TRUE);
	}
	parser_error(sort, token, NSORT_PackedUnimplemented);
	field->type = typePacked;
	pf->has_type = TRUE;
	break;
#endif

      case tokDECIMAL:
	if (pf->has_type)
	{
	    parser_error(sort, token, NSORT_FIELD_ALREADY_TYPED);
	    return (TRUE);
	}
	field->type = pf->has_delimiter ? typeDelimDecimalString
					: typeFixedDecimalString;
	pf->has_type = TRUE;
	break;
	
      case tokFLOAT:
      case tokSFLOAT:
	if (pf->has_type)
	{
	    parser_error(sort, token, NSORT_FIELD_ALREADY_TYPED);
	    return (TRUE);
	}
	if (field->length != 0 && field->length != 4)
	    parser_error(sort, token, NSORT_FLOAT_SIZE);
	field->type = typeFloat;
	field->length = 4;
	pf->has_type = TRUE;
	pf->has_size = TRUE;
	break;

      case tokDOUBLE:
      case tokTFLOAT:
	if (pf->has_type)
	{
	    parser_error(sort, token, NSORT_FIELD_ALREADY_TYPED);
	    return (TRUE);
	}
	if (field->length != 0 && field->length != sizeof(double))
	    parser_error(sort, token, NSORT_DOUBLE_SIZE);
	field->type = typeDouble;
	field->length = sizeof(double);
	pf->has_type = TRUE;
	pf->has_size = TRUE;
	break;

      default:		/* innapropriate token for a type definition */
	return (FALSE);
    }

    /*
    ** If the type was incompleted specified before this qualifier
    ** and now it is (nearly) completely specified
    ** then clean up the info a little
    */
    if (incomplete_type && pf->has_binary && (pf->has_size || pf->has_delimiter))
    {
	switch (field->length)
	{
	  case 1: field->type = pf->has_unsigned ? typeU1 : typeI1; break;
	  case 2: field->type = pf->has_unsigned ? typeU2 : typeI2; break;
	  case 4: field->type = pf->has_unsigned ? typeU4 : typeI4; break;
	  case 8: field->type = pf->has_unsigned ? typeU8 : typeI8; break;

	  default:
	    if (pf->has_unsigned)
		field->type = typeFixedString;
	    else
		parser_error(sort, token, NSORT_INT_SIZE);
	    field->pad = '\0';
	    break;
	}
    }

    *def = p;
    return (TRUE);
}

/*
** parse_field
**
**	/field=[(]
**		name=field_name,
**		position:N
**		[,(size:N | delimiter:C)]
**		[,datatype]
**		[,pad:C]
**	       [)]
*/
void parse_field(sort_t *sort, const char **statement, token_t *token)
{
    const char	*p = *statement;
    int		errs = sort->n_errors;
    field_t	*field;
    int		found_paren;
    parse_field_flags_t pf;

    if ((RECORD.fieldcount % 8) == 0)	/* allocate in chunks of 8 */
	RECORD.fields = (field_t *)
			realloc(RECORD.fields,
				(RECORD.fieldcount + 8) * sizeof(*field));
    field = RECORD.fields + RECORD.fieldcount;
    field->name = "<unnamed>";
    field->position = POS_INVALID;
    field->flags = 0;
    field->length = 0;
    field->delimiter = ' ';
    field->pad = ' ';
    field->derivation = NULL;
    memset(&pf, '\0', sizeof(pf));

    p = *statement;

    if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	return;
    found_paren = expect_token(sort, &p, token, tokLEFTPAREN, NSORT_NOERROR);
    do
    {
	switch (get_token(sort, &p, token, FieldSpecs))
	{
	  case tokNAME:		/* name=field_name */
	    if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_FIELD_NEEDS_EQ))
		return;
	    if (get_token(sort, &p, token, NULL) != tokIDENTIFIER)
		parser_error(sort, token, NSORT_BAD_FIELD_NAME);
	    else if (FIELD_NAMED(field->name))
		parser_error(sort, token, NSORT_FIELD_ALREADY_NAMED);
	    else if (get_field_desc(&sort->record, token->value.strval) != NULL)
		parser_error(sort, token, NSORT_DUPLICATE_FIELDNAME);
	    else
	        field->name = strdup(token->value.strval);
	    break;

	  default:
	    if (!parse_type_def(sort, &p, field, token, &pf))
		parser_error(sort, token, NSORT_FIELD_SYNTAX);
	    break;
	}
    } while (skipcomma(sort, &p, errs));

    if (found_paren)
	matching_paren(sort, &p, token);

    if (!pf.has_type)
	field->type = pf.has_delimiter ? typeDelimString : typeFixedString;
    if (!pf.has_size && !pf.has_delimiter && sort->n_errors == 0)
	parser_error(sort, token, NSORT_TYPE_WITHOUT_LENGTH);

    RECORD.fieldcount++;	/* now 'accept' this field as well-defined */
    *statement = p;
}

/*
** parse_key
**
**	/key=field_name
**		[,{ascending|descending}]
**		[,number:N]
**	/key=   [{position|offset}:N]
**		[,(size:N | delimiter:C)]
**		[,datatype]
**		[,pad:C]
**	        [,{ascending|descending}]
**	        [,number:N]
**	     
**	There is a parsing ambiguity in determining whether an 
**	identifier is really an identifer or a short, not-quite-uniquely
**	distinguishing keyword prefix:
**		e.g. 'p' could be position,packed, or the identifier 'p'
**	When we get an identifier which is followed by tokASSIGNVALUE
**	
*/
void parse_key(sort_t *sort, const char **statement, token_t *token)
{
    const char	*p = *statement;
    keydesc_t	*key;
    int		type_seen = FALSE;
    int		found_paren;
    field_t	*field;
    int		errs = sort->n_errors;
    parse_field_flags_t pf;

    key = (keydesc_t *) realloc(sort->keys, (sort->keycount + 1) *sizeof(*key));
    sort->keys = key;
    key += sort->keycount;
    key->position = POS_INVALID;
    key->flags = 0;
    key->length = 0;
    key->number = 0;
    key->delimiter = ' ';
    key->pad = ' ';
    key->name = "<unnamed>";
    memset(&pf, '\0', sizeof(pf));

    if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	return;

    found_paren = optional_paren(sort, &p, token);
    do
    {
	switch (get_token(sort, &p, token, KeySpecs))
	{
	  case tokIDENTIFIER:		/* field_name */
	    if (expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_NOERROR))
	    {
		p = unget_token(sort, token);
		parser_error(sort, token, NSORT_KEY_SYNTAX);
	    }
	    else if (pf.has_type)
		parser_error(sort, token, NSORT_KEY_ALREADY_TYPED);
	    else
	    {
		field = get_field_desc(&sort->record, token->value.strval);
		if (field == NULL)
		    parser_error(sort, token, NSORT_KEY_FIELD_MISSING);
		else
		{
		    key->type = field->type;
		    key->flags = field->flags;
		    key->delimiter = field->delimiter;
		    key->pad = field->pad;
		    key->length = field->length;
		    key->position = field->position;
		    key->name = field->name;
		    pf.has_type = TRUE;
		    pf.has_size = TRUE;
		    type_seen = TRUE;
		}
	    }
	    break;

	  case tokASCENDING:
	  case tokDESCENDING:
	    if (pf.has_ordering)
		parser_error(sort, token, NSORT_REDUNDANT_ORDERING);
	    pf.has_ordering = TRUE;
	    if (token->toktype == tokDESCENDING)
		key->flags |= FIELD_DESCENDING;
	    break;

	  case tokNUMBER:
	    if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_COLON_EXPECTED))
		return;
	    if (get_token(sort, &p, token, NULL) != tokINTEGERVALUE)
		parser_error(sort, token, NSORT_KEY_NUMBER);
	    else if (token->value.i8val < 1 || token->value.i8val > 255)
		parser_error(sort, token, NSORT_KEY_NUMBER_INVALID);
	    else
	    {
		keydesc_t	*k2;

		for (k2 = sort->keys; k2 < key; k2++)
		    if (k2->number == token->value.i8val)
			parser_error(sort, token, NSORT_KEY_NUMBER_DUPLICATE);
		key->number = token->value.i8val;
	    }
	    break;

	  case tokAMBIGUOUSKEYWORD:
	    break;
	  
	  default:
	    if (type_seen || !parse_type_def(sort, &p, (field_t *) key, token, &pf))
		parser_error(sort, token, NSORT_KEY_SYNTAX);
	    break;
	}
    } while (token->toktype != tokAMBIGUOUSKEYWORD && skipcomma(sort, &p, errs));

    if (found_paren && sort->n_errors == errs)
	matching_paren(sort, &p, token);

    if (!pf.has_type)
	key->type = pf.has_delimiter ? typeDelimString : typeFixedString;
    if (!pf.has_size && !pf.has_delimiter && sort->n_errors == errs)
	parser_error(sort, token, NSORT_TYPE_WITHOUT_LENGTH);

    sort->keycount++;		/* now 'accept' this key as well-defined */
    *statement = p;
}

void add_edit(sort_t *sort, field_t *field)
{
    edit_t	*edit;

    sort->n_edits++;
    edit = (edit_t *) realloc(sort->edits, sort->n_edits * sizeof(*edit));
    sort->edits = edit;
    edit += sort->n_edits - 1;
    edit->is_field = !(field->flags & FIELD_DERIVED);
    edit->is_variable = !(field->flags & FIELD_VARIABLE);
    edit->length = field->length;
    if (edit->is_field)
    {
	edit->source.field_num = field - RECORD.fields;
	ASSERT(edit->source.field_num < RECORD.fieldcount);
	edit->position = field->position;
    }
    else
    {
	edit->source.constant = (byte *) field->derivation->value;
	edit->position = 0;
    }
}

/*
** parse_derived -	generate new constant field for the sorted output
**
**		/derived=[(] <<as /field supports>>
**			  [,value=constant] [)]
**			Create a new field to be placed in nsort's output.
**			A derived field may contain a numeric or
**			string constant.
**
*/
void parse_derived(sort_t *sort, const char **statement, token_t *token)
{
    const char	*p;
    field_t	*field;
    int		errs;
    int		found_paren;
    parse_field_flags_t pf;

    if ((RECORD.fieldcount % 8) == 0)	/* allocate in chunks of 8 */
	RECORD.fields = (field_t *) realloc(RECORD.fields, (RECORD.fieldcount + 8) * sizeof(*field));
    field = RECORD.fields + RECORD.fieldcount;
    field->name = "<unnamed>";
    field->type = typeFixedString;	/* default, probably will be user-set */
    field->position = ~0;
    field->flags = 0;
    field->length = 0;
    field->delimiter = ' ';
    field->pad = ' ';
    memset(&pf, '\0', sizeof(pf));

    p = *statement;
    errs = sort->n_errors;

    if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	return;
    found_paren = optional_paren(sort, &p, token);
    do
    {
	switch (get_token(sort, &p, token, DerivedSpecs))
	{
	  case tokNAME:		/* name=field_name (of this new field) */
	    if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_FIELD_NEEDS_EQ))
		return;
	    if (get_token(sort, &p, token, NULL) != tokIDENTIFIER)
		parser_error(sort, token, NSORT_BAD_FIELD_NAME);
	    else if (FIELD_NAMED(field->name))
		parser_error(sort, token, NSORT_FIELD_ALREADY_NAMED);
	    else if (get_field_desc(&sort->record, token->value.strval) != NULL)
		parser_error(sort, token, NSORT_DUPLICATE_FIELDNAME);
	    else
		field->name = strdup(token->value.strval);
	    break;

	  case tokVALUE:
	    if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_FIELD_NEEDS_EQ))
		return;
	    field->flags |= FIELD_DERIVED;
	    field->derivation = (expr_t *) malloc(sizeof(expr_t));
	    field->derivation->value = (anyval_t *) malloc(field->length);
	    switch (get_token(sort, &p, token, CharSpecs))
	    {
	      case tokINTEGERVALUE:
		switch (field->type)
		{
		  case typeI1:
		  case typeU1:
		    field->derivation->value->i1val = token->value.i8val; 
		    break;

		  case typeI2:
		  case typeU2:
		    field->derivation->value->i2val = token->value.i8val; 
		    break;

		  case typeI4:
		  case typeU4:
		    field->derivation->value->i4val = token->value.i8val; 
		    break;

		  case typeI8:
		    field->derivation->value->i8val = token->value.i8val; 
		    break;

		  case typeU8:	/* initially <= 32 bits, can sum to be larger */
		    field->derivation->value->u8val = token->value.u8val; 
		    break;

		  default:
		    if (errs == sort->n_errors)
			parser_error(sort, token, NSORT_NON_NUMERIC_DERIVED);
		}
		break;

	      case tokCHARVALUE:
		if (!IsStringType(field->type))
		    parser_error(sort, token, NSORT_NON_STRING_DERIVED);
		memset(field->derivation->value->strval, field->pad, field->length);
		strncpy(field->derivation->value->strval,
			token->value.strval,
			field->length);
		break;

#if 0	/* value=field_name (a copy of another field) */
	      case tokIDENTIFIER:
		field_t	*existing;
		if ((existing = get_field_desc(&sort->record,
					       token->value.strval)) == NULL)
		    parser_error(sort, token, NSORT_DERIVED_FIELD_MISSING);
		else if (existing->type != field->type)
		    parser_error(sort, token, NSORT_TYPE_MISMATCH);
#endif

	      default:
		if (errs == sort->n_errors)
		    parser_error(sort, token, NSORT_DERIVED_VALUE_UNSUPPORTED);
	    }
	    break;

	  default:
	    if (!parse_type_def(sort, &p, field, token, &pf))
		/* parser_error(sort, token, NSORT_FIELD_SYNTAX) */ ;
	    break;
	}
    } while (skipcomma(sort, &p, errs));

    if (sort->n_errors == errs)	/* no errors in this statement so far */
    {
	if (found_paren)
	    matching_paren(sort, &p, token);
	if (!pf.has_type)
	    field->type = pf.has_delimiter ? typeDelimString : typeFixedString;
	if (!(field->flags & FIELD_DERIVED))
	    parser_error(sort, token, NSORT_DERIVED_NEEDS_VALUE);
	else if (!pf.has_size && !pf.has_delimiter)
	    parser_error(sort, token, NSORT_TYPE_WITHOUT_LENGTH);
	else
	    sort->n_derived++;
    }

    RECORD.fieldcount++;	/* accept this derived field as well-defined */
    *statement = p;
}

/*
** parse_summarize -	specify a list of fields to be summed in groups
**			of records which have the same keys
**
**		/summarize [{= | :}] [(] field_name [,...] [)]
**
*/
void parse_summarize(sort_t *sort, const char **statement, token_t *token)
{
    const char	*p = *statement;
    int		errs = sort->n_errors;
    int		found_paren;
    field_t	*existing;

    expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_NOERROR);
    found_paren = optional_paren(sort, &p, token);
    do
    {
	if (get_token(sort, &p, token, NULL) != tokIDENTIFIER)
	    parser_error(sort, token, NSORT_BAD_FIELD_NAME);
	else if ((existing = get_field_desc(&sort->record,
					    token->value.strval)) == NULL)
	    parser_error(sort, token, NSORT_SUMMARIZE_FIELD_MISSING);
	else if (!IsIntegerType(existing->type) ||
		 !(existing->length == 4 || existing->length == 8))
	    parser_error(sort, token, NSORT_SUMMARY_NEEDS_INT);
	else
	{
	    sort->n_summarized++;
	    sort->summarize = (short *) realloc(sort->summarize,
						sort->n_summarized *
						sizeof(*sort->summarize));
	    sort->summarize[sort->n_summarized - 1] = (short) (existing - RECORD.fields);
	}
    } while (skipcomma(sort, &p, errs));

    if (found_paren && errs == sort->n_errors)
	matching_paren(sort, &p, token);

    *statement = p;
}

/*
** parse_data -	limit the sort output fields to a subset of input fields
**
**		/data=field_name[,...]
**			Specify fields to be placed in nsort's output.
**			There may be many /data statements. Fields not
**			mentioned in /data statements are not placed
**			in the output. If there are no /data statements
**			then all fields of the input
**			will be placed in the output.
**			Fields named in /data statements must have been
**			previously defined (by /field or /derived statements).
**
*/
void parse_data(sort_t *sort, const char **statement, token_t *token)
{
    const char	*p = *statement;
    int		errs = sort->n_errors;
    field_t	*field;

    p = *statement;

    if (!expect_token(sort, &p, token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	return;
    do
    {
	if (get_token(sort, &p, token, NULL) != tokIDENTIFIER)
	    parser_error(sort, token, NSORT_BAD_FIELD_NAME);
	else
	{
	    field = get_field_desc(&sort->record, token->value.strval);
	    if (field == NULL)
		parser_error(sort, token, NSORT_DATA_FIELD_MISSING);
	    else
		add_edit(sort, field);
	}
    } while (skipcomma(sort, &p, errs));
    *statement = p;
}

/*
 * parse_transfersize	- we've seen 'transfersize', look for ':N[km]'
 */
int parse_transfersize(sort_t *sort, const char **p, token_t *token)
{
    if (!expect_token(sort, p, token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	return (0);
    else if (get_token(sort, p, token, NULL) != tokINTEGERVALUE ||
	token->value.u8val == 0)
    {
	parser_error(sort, token, NSORT_NEEDS_POSITIVE_INT);
	return (0);
    }
    else
    {
	return (token->value.u8val * get_scale(p, "km"));
    }
}

/*
 * parse_aiocount	- we've seen 'count', look for ':N'
 */
int parse_aiocount(sort_t *sort, const char **p, token_t *token)
{
    if (!expect_token(sort, p, token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	return (0);
    else if (get_token(sort, p, token, NULL) != tokINTEGERVALUE ||
	token->value.u8val == 0)
    {
	parser_error(sort, token, NSORT_NEEDS_POSITIVE_INT);
	return (0);
    }
    else
    {
	return (token->value.u8val);
    }
}

/*
 * parse_file_options	handle a in/out/filesys's [,tran=N[km]][,count=N] 
 *
 *		[,transfersize:N][,count:N][,{direct|buffered[,sync]}]
 *
 *	The file has already been opened, and we have obtained the stbuf
 */
void parse_file_options(sort_t *sort,
		   fileinfo_t *file,
		   const char **p,
		   int errs,
		   const toktype_t *expecting)
{
    token_t	token;

    while (skipcomma(sort, p, errs))
    {
	switch (get_token(sort, p, &token, expecting))
	{
	  case tokTRANSFERSIZE:
	    file->io_size = parse_transfersize(sort, p, &token);
	    break;

	  case tokCOUNT:
	    file->aio_count = parse_aiocount(sort, p, &token);
	    /* XXX need to complain if the explicitly requested aio_counts over
	     * XXX all input, output, and temp files is larger than SYS.aio_max
	    if (file->aio_count != 0 && file->aio_count > SYS.aio_max)
		parser_error(sort, &token, NSORT_AIOCOUNT_TOO_LARGE, SYS.aio_max);
	     */
	    break;

	  case tokDIRECT:
	    if (!CAN_SEEK(file->stbuf.st_mode) && !S_ISDIR(file->stbuf.st_mode))
		parser_error(sort, &token, NSORT_CANT_SEEK,
			     file->name, "direct");
	    file->io_mode = IOMODE_DIRECT;
	    break;

	  case tokBUFFERED:	/* do non-direct i/o from/to the disk cache */
	  case tokMAPPED:	/* do mmap; read only or copy on write */
	  case tokCOPYWRITE:
	    if (!CAN_SEEK(file->stbuf.st_mode) && !S_ISDIR(file->stbuf.st_mode))
		parser_error(sort, &token, NSORT_CANT_SEEK,
			     file->name,
			     token.toktype == tokMAPPED ? "mapped" :"buffered");

	    file->io_mode = (token.toktype == tokMAPPED) ? IOMODE_MAPPED
							 : IOMODE_BUFFERED;
	    if (token.toktype == tokCOPYWRITE)
		file->copies_write = TRUE;
	    break;

	  case tokSYNC:
	    file->sync = TRUE;
	    break;

	  default:
	    if (errs == sort->n_errors)
		parser_error(sort, &token, NSORT_UNEXPECTED_IN_LIST);
	    return;
	}
    }
}


/*
** parse_statement	- process one statement in the sort definition language
**
**	/field
**	/key
**	/condition
**	options
*/
nsort_msg_t parse_statement(sort_t *sort, const char *source, const char *title)
{
    const char	*p;
    token_t	token;
    int		errs;
    int		found_paren;

    /* ignore comment lines
     */
    if (*source == '#' || *source == '!')
	return (NSORT_NOERROR);

    PARSE.line = source;
    PARSE.title = title;
    p = source;
    if (get_token(sort, &p, &token, NULL) == tokEOF)
	return (NSORT_NOERROR);

    if (token.toktype != tokSLASH && token.toktype != tokDASH)
    {
	parser_error(sort, &token, NSORT_STATEMENT_START);
	return (NSORT_STATEMENT_START);
    }

    errs = sort->n_errors;
    switch (get_token(sort, &p, &token, Statements))
    {
      case tokFIELD:
	parse_field(sort, &p, &token);
	break;

#if 0
      case tokCONDITION:
      case tokINCLUDE:
      case tokOMIT:
      case tokOVERLAY:
	break;
		
      case tokDATA:	/* project only certain fields to the sorted output */
	parse_data(sort, &p,  &token);
	break;
#endif

      case tokDERIVED:	/* /derived=(<<as /field>> [,value={constant}]) */
	parse_derived(sort, &p,  &token);
	break;

      case tokKEY:
	parse_key(sort, &p,  &token);
	break;

      case tokMETHOD:	/* method=[(][{record|pointer}]
				     [,{radix|merge}]
				     [,{prehash|noprehash}]
				     [)] */
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	found_paren = optional_paren(sort, &p, &token);
	do
	{
	    switch (get_token(sort, &p, &token, MethodSpecs))
	    {
	      case tokRECORD:
		sort->method = SortMethRecord;
		break;

	      case tokPOINTER:
		sort->method = SortMethPointer;
		break;

	      case tokMERGE:
		sort->radix = FALSE;
		break;

	      case tokRADIX:
		sort->radix = TRUE;
		break;

	      case tokHASH:
		sort->compute_hash = TRUE;
		break;

	      case tokNOHASH:
		sort->compute_hash = FALSE;
		break;

#if defined(tokPREHASH)
	      case tokPREHASH:
		sort->prehashing = SortPreHashRequired;
		break;

	      case tokNOPREHASH:
		sort->prehashing = SortPreHashForbidden;
		break;
#endif

	      default:
		if (errs == sort->n_errors)
		    parser_error(sort, &token, NSORT_UNEXPECTED_IN_LIST);
		break;
	    }
	} while (skipcomma(sort, &p, errs));
	if (found_paren && errs == sort->n_errors)
	    matching_paren(sort, &p, &token);
	break;

      case tokMEMORY:
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	if (get_token(sort, &p, &token, NULL) != tokINTEGERVALUE)
	    parser_error(sort, &token, NSORT_MEMORY_NEEDED);
	else
	{
	    int scale = get_scale(&p, "kmg");

	    if (token.value.u8val > LONGLONG_MAX / scale)
		parser_error(sort, &token, NSORT_SCALE_OVERFLOW);
	    else
		sort->memory = (token.value.u8val * scale) / 1024;
	}
	break;

      case tokINFILE:	 /* /infile=[(] filename[{,transfer=N[km]}
						 {,count=N}] [)] */
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	found_paren = optional_paren(sort, &p, &token);
	p = another_infile(sort, p);
	if (found_paren && errs == sort->n_errors)
	    matching_paren(sort, &p, &token);
	break;

      case tokOUTFILE:		/* /outfile=[(] fspec[,transfer=N]
					             [,count=N] [)] */
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	found_paren = optional_paren(sort, &p, &token);
	p = assign_outfile(sort, p);
	if (found_paren && errs == sort->n_errors)
	    matching_paren(sort, &p, &token);
	break;

      case tokTEMPFILE:
	/*
	 * Define a new set of temporary file directories, ingoring the old
	 * /tempfile= [(] {fspec | transfer=N[km] | count=N }+ [)]
	 */
	rm_temps(sort);
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	found_paren = optional_paren(sort, &p, &token);
	do
	{
	    switch (get_token(sort, &p, &token, TempSpecs))
	    {
	      case tokTRANSFERSIZE:
		TEMP.blk_size = parse_transfersize(sort, &p, &token);
		break;

	      case tokCOUNT:
		TEMP.aio_cnt = parse_aiocount(sort, &p, &token);
		break;

	      case tokFILENAME:
		p = unget_token(sort, &token);
		p = another_tempfile(sort, p);
		break;

	      default:
		parser_error(sort, &token, NSORT_UNEXPECTED_IN_LIST);
		break;
	    }
	} while (skipcomma(sort, &p, errs));
	if (found_paren && errs == sort->n_errors)
	    matching_paren(sort, &p, &token);
	break;

      case tokFILESYS:	 /* -filesys=[(] filename[{,transfer=N[km]}
						 {,count=N}] [)] */
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	found_paren = optional_paren(sort, &p, &token);
	p = another_filesys(sort, p);
	if (found_paren && errs == sort->n_errors)
	    matching_paren(sort, &p, &token);
	break;

#if 0
      case tokAIOCOUNT:		/* /aio_count=N */
	expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ);
	if (get_token(sort, &p, &token, IntSpecs) != tokINTEGERVALUE ||
	    token.value.u8val == 0)
	    parser_error(sort, &token, NSORT_NEEDS_POSITIVE_INT);
	else if (token.value.u8val > SYS.aio_max)
	    parser_error(sort, &token, NSORT_AIOCOUNT_TOO_LARGE, SYS.aio_max);
	else
	    IN.aio_cnt = OUT.aio_cnt = TEMP.aio_cnt = token.value.u8val;
	break;
#endif

      case tokRECORDCOUNT:	/* /record_count=N */
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	if (get_token(sort, &p, &token, IntSpecs) != tokINTEGERVALUE ||
	    token.value.u8val == 0)
	    parser_error(sort, &token, NSORT_NEEDS_POSITIVE_INT);
	else
	{
	    int scale = get_scale(&p, "kmg");

	    if (token.value.u8val > LONGLONG_MAX / scale)
		parser_error(sort, &token, NSORT_SCALE_OVERFLOW);
	    else
		sort->n_recs = (i8) token.value.u8val * scale;
	}
	break;

      case tokPROCESSES:	/* /processes=N */
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	if (get_token(sort, &p, &token, IntSpecs) != tokINTEGERVALUE ||
	    token.value.u8val == 0)
	    parser_error(sort, &token, NSORT_CPU_REQ_NEEDED);
	else
	{
	    /* XXX is asking for N cpus when fewer than N are available
	     * to this process a hard error or a soft warning?
	     */
	    if (prctl(PR_MAXPPROCS) < sysmp(MP_NPROCS))
	    {
		if (token.value.u8val > sysmp(MP_NPROCS))
		    parser_error(sort, &token, NSORT_CPU_REQ_TOOBIG,
					       sysmp(MP_NPROCS),
					       prctl(PR_MAXPPROCS));
		else if (token.value.u8val > prctl(PR_MAXPPROCS))
		    parser_error(sort, &token, NSORT_CPU_REQ_RESTRICTED,
					       prctl(PR_MAXPPROCS),
					       sysmp(MP_NPROCS));
	    }
	    sort->n_sprocs = token.value.u8val;
	}
	break;

      case tokSPECIFICATION:	/* -spec=<file-spec> */
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_SPECIFICATION_NEEDS_EQ))
	    break;
	if (get_token(sort, &p, &token, FileSpecs) == tokFILENAME)
	    process_specfile(sort, token.value.strval);
	else
	    parser_error(sort, &token, NSORT_FILENAME_MISSING);
	PARSE.title = NULL;
	break;

      case tokSUMMARIZE:
	parse_summarize(sort,  &p, &token);
	break;

      case tokSTATISTICS:	/* /statistics[=[(][prehash|noprehash][,wizard][)]] */
	if (expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NOERROR))
	{
	    found_paren = optional_paren(sort, &p, &token);
	    do
	    {
		switch (get_token(sort, &p, &token, StatSpecs))
		{
#if defined(tokPREHASH)
		  case tokPREHASH:
		    STATS.hash_details = TRUE;
		    break;

		  case tokNOPREHASH:
		    STATS.hash_details = FALSE;
		    break;
#endif

		  case tokWIZARD:
		    STATS.wizard = TRUE;
		    break;

		  default:
		    if (errs == sort->n_errors)
			parser_error(sort, &token, NSORT_BAD_HASHSPEC);
		    break;
		}
	    } while (skipcomma(sort, &p, errs));
	    if (found_paren && errs == sort->n_errors)
		matching_paren(sort, &p, &token);
	}
	/* One for each sorting sproc, 1 for the main sproc,
	 * and 1 for summary of all the aio sprocs.
	 */
	STATS.usages = (sprocstat_t  *) malloc((MAXSORTSPROC + 2) *
					       sizeof(sprocstat_t));
	break;

      case tokNOSTATISTICS:
	CONDITIONAL_FREE(STATS.usages);
	break;

      case tokFORMAT:
	/*	/format=[(]{record_size|size}:{N|
	 *		     variable[,{maximum|lrl}:N]}|
	 *		     {record_delimiter|delimiter}:C[,{maximum|lrl}:N][)]
	 */
	if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_NEEDS_EQ))
	    break;
	found_paren = optional_paren(sort, &p, &token);
	do
	{
	    switch (get_token(sort, &p, &token, FormatSpecs))
	    {
	      case tokRECSIZE:
	      case tokSIZE:
		if (!expect_token(sort, &p, &token, tokASSIGNVALUE, NSORT_COLON_EXPECTED))
		    break;
		switch (get_token(sort, &p, &token, RecsizeSpecs))
		{
		  case tokINTEGERVALUE:
		    if (token.value.i8val < 1 || token.value.i8val > 65535)
			parser_error(sort, &token, NSORT_BAD_REC_SIZE);
		    else
		    {
			RECORD.flags |= RECORD_FIXED;
			RECORD.length = token.value.i8val;
		    }
		    break;

		  case tokVARIABLE:
		    RECORD.flags |= RECORD_VARIABLE;
		    break;

		  default:
		    parser_error(sort, &token, NSORT_BAD_REC_SIZE);
		    break;
		}
		break;

	      case tokRECDELIM:
	      case tokDELIMITER:
		if (!expect_token(sort, &p, &token, tokASSIGNVALUE,
				  NSORT_COLON_EXPECTED))
		    break;
		if (get_token(sort, &p, &token, CharSpecs) != tokCHARVALUE)
		    parser_error(sort, &token, NSORT_DELIM_NEEDED);
		else
		{
		    RECORD.flags |= RECORD_STREAM;
		    RECORD.delimiter = token.value.strval[0];
		}
		break;

	      case tokMINIMUM:
		if (!(RECORD.flags & (RECORD_STREAM | RECORD_VARIABLE)))
		    parser_error(sort, &token, NSORT_REC_MUST_BE_VARLEN);
		else
		{
		    expect_token(sort, &p, &token, tokASSIGNVALUE,
				 NSORT_COLON_EXPECTED);
		    if (get_token(sort, &p, &token, NULL) != tokINTEGERVALUE)
			parser_error(sort, &token, NSORT_REC_MINLEN_NEEDS_INT);
		    else if (token.value.i8val < 1 || token.value.i8val > 65535)
			parser_error(sort, &token, NSORT_REC_MINLEN_INVALID);
		    else
			RECORD.minlength = token.value.i8val;
		}
		break;

	      case tokMAXIMUM:
	      case tokLRL:
		if (!(RECORD.flags & (RECORD_STREAM | RECORD_VARIABLE)))
		    parser_error(sort, &token, NSORT_REC_MUST_BE_VARLEN);
		else
		{
		    expect_token(sort, &p, &token, tokASSIGNVALUE,
				 NSORT_COLON_EXPECTED);
		    if (get_token(sort, &p, &token, NULL) != tokINTEGERVALUE)
			parser_error(sort, &token, NSORT_REC_MAXLEN_NEEDS_INT);
		    else if (token.value.i8val < 1 || token.value.i8val > 65535)
			parser_error(sort, &token, NSORT_REC_MAXLEN_INVALID);
		    else
			RECORD.maxlength = token.value.i8val;
		}
		break;
		    
	      default:
		if (errs == sort->n_errors)
		    parser_error(sort, &token, NSORT_FORMAT_SPEC);
		break;
	    }
	} while (skipcomma(sort, &p, errs));
	if (found_paren && errs == sort->n_errors)
	    matching_paren(sort, &p, &token);
	break;

#if 0
      case tokCHECKSEQUENCE:	/* permit the option though it is ignored */
      case tokNOCHECKSEQUENCE:	/* we need to do all the hard work of */
	break;			/* checking the order as a recode side-effect */
#endif

      case tokDUPLICATES:
	if (sort->n_summarized)
	    parser_error(sort, &token, NSORT_SUMMARIZE_DUPLICATES);
	sort->delete_dups = FALSE;
	break;

      case tokNODUPLICATES:
	sort->delete_dups = TRUE;
	break;

      case tokSTABLE:		/* permit the option though it is ignored */
      case tokNOSTABLE:		/* we do stable sorts for 'free' */
	break;

      case tokTOUCHMEM:
	sort->touch_memory = FlagTrue;
	break;

      case tokNOTOUCHMEM:
	sort->touch_memory = FlagFalse;
	break;

      case tokTWOPASS:
	sort->two_pass = TRUE;
	break;

      case tokEOF:
	break;

      default:
	/* parser_error(sort, &token, NSORT_NOT_STATEMENT); */
	break;
    }
    if (errs == sort->n_errors)
	expect_token(sort, &p, &token, tokEOF, NSORT_EXTRA_AFTER_STATEMENT);
    return (sort->last_error);
}

/*
** get_field_desc	- look up a field by name
**
*/
field_t *get_field_desc(record_t *rec, const char *name)
{
    field_t	*field;

    if (rec->fieldcount == 0)
	return (NULL);

    /*
    ** This uses the fieldcount as the loop terminator, rather than the
    ** field->type of typEOF.  get_field_desc() is called during parsing,
    ** before the 'marker' has been put at the end of the field list.
    */
    for (field = rec->fields + rec->fieldcount; --field >= rec->fields; )
	if (strcmp(field->name, name) == 0)
	    return (field);
    return ((field_t *) NULL);
}

/*
** nsort_statement	- determine whether a string apears to contain
**			  a sort specification command
**
**	Command strings which are abbreviated to "-<single-letter>" are
**	not recognized here as commands.  They are assumed to be
**	other argv-style command line options. If single-letter abbreviations
**	are wanted the alternate form "/<single-letter>" works.
**
**	Returns:
**		NULL if the string is not the start of a command
**		the address of the end of the command name if it is a command
*/
char *nsort_statement(const char *str)
{
    const keyword_t	*kw;
    const char		*end;

    while (isspace(str[0]))
	str++;
    if (((str[0] == '-' && str[2] != '\0') || str[0] == '/') &&
	(kw = keyword_lookup((sort_t *) NULL, (token_t *) NULL, &str[1], Statements)))
    {
	for (end = str + 1; IsIdentifier(*end); end++)
	    continue;

	/* Recognize ambiguous tokens as keywords only if they are at least
	 * two characters long.  This hopes to distinguish single letter
	 * command-line option names from ambiguously short keywords
	 */
	if (kw->value != tokAMBIGUOUSKEYWORD || (end - str) >= 2)
	    return ((char *) end);
    }
    return (NULL);
}

void nsort_options(sort_t *sort, int argc, char **argv)
{
    extern void usage(char *badarg);

    for (; argc != 0; argc--, argv++)
    {
	if (argv[0][0] != '-' || argv[0][1] == '\0')
	    another_infile(sort, argv[0]);
	else if (nsort_statement(argv[0]))
	    parse_statement(sort, argv[0], (char *) NULL);
	else switch (argv[0][1])
	{
	  case 'C': sort->n_recs = (i8) get_number(argv[0] + 2);
		    CHECK_POS(sort->n_recs); break;
	  case 'o': if (argv[0][2] != '\0' || argc == 1) usage(argv[0]);
		    assign_outfile(sort, argv[1]);
		    argc--; argv++;
		    break;
	  case 'O': sort->old_recode = 1; break;
	  case 't': Print_task = TRUE; CHECK_END; break;
	  case 'T': Print_task = (argv[0][2] == '\0') ? 2 : 3; break;
	  case 'u': COPY.target_size = atoi(argv[0] + 2) * 512;
		    CHECK_POS(COPY.target_size); break;
	  case 'v': sort->print_definition = TRUE; break;
	  case 'x': sort->test = 1; break;
	  default: die("Bad option %s", argv[0]); /* usage(argv[0]); */
	}
    }
}

void parse_string(sort_t *sort, const char *string, const char *title)
{
    char	*start, *end;
    char	prev = '\n';

    if (title && title[0] != '\0')
	PARSE.title = title;
    string = strdup(string);
    for (start = end = (char *) string;; end++)
    {
	/* Replace comment lines with spaces. XXX Complete fix soon */
	if (prev == '\n' && (*end == '#' || *end == '!'))
	{
	    do
		*end++ = ' ';
	    while (*end != '\0' && *end != '\n');
	}
	if (*end == '\0')	/* file probably was truncated */
	{			/* XXX complain or parse? */
	    parse_statement(sort, start, title);
	    break;
	}

	/* If the next line starts a statement, or is the end of the file
	 * then pass the part of the string in front of the next to the parser
	 */
	prev = *end;
	if (isspace(*end) &&
	    (nsort_statement(end + 1) ||
	     (end[0] == '\n' && (end[1] == '-' || end[1] == '/'))))
	{
	    *end = '\0';	/* replace space/newline with null */
	    parse_statement(sort, start, title);
	    PARSE.lineno++;
	    start = end + 1;
	}
    }
    free((void *) string);
}

/*
 * process_specfile	- parse contains of a file in a -spec statement
 */
int process_specfile(sort_t *sort, const char *specfile)
{
    char	*specbuf;
    int		fd;
    struct stat	stbuf;

    if (PARSE.specfile_depth >= SPECFILE_DEPTH_LIMIT)
    {
	runtime_error(sort, NSORT_SPEC_TOO_DEEP, PARSE.title, specfile);
	return (NSORT_SPEC_TOO_DEEP);
    }

    if ((fd = open(specfile, O_RDONLY, 0)) < 0)
    {
	runtime_error(sort, NSORT_SPEC_OPEN, specfile, strerror(errno));
	return (NSORT_SPEC_OPEN);
    }

    PARSE.specfile_depth++;
    PARSE.lineno = 1;

    if (fstat(fd, &stbuf) < 0)
	die("Cannot determine size of specification file %s: %s",
	    specfile, strerror(errno));
    specbuf = (char *) malloc(stbuf.st_size + 1);
    if (read(fd, specbuf, stbuf.st_size) != stbuf.st_size)
	die("Failure reading specification file %s: %s",
	    specfile, strerror(errno));
    specbuf[stbuf.st_size] = '\0';

    parse_string(sort, specbuf, specfile);

    close(fd);
    free(specbuf);

    PARSE.specfile_depth--;

    return (NSORT_NOERROR);
}

void parse_string1(sort_t *sort, const char *string, const char *title)
{
    char	*str = strdup(string);
    char	*start;
    char	*p;
    char	*end;

    start = end = str; 
    for (;;)
    {
	/* Look for an unescaped " -" which signifies the end of this command
	 * by marking the start of the next command.
	 */
	end = strstr(end, " -");
	while (end != NULL && end[-1] == '\\' &&
	       (p = strstr(end + 2, " -")) != NULL)
	    end = p;
		
	if (end == NULL)	/* no more statements after 'start' */
	{			
	    if (start != end)
		parse_statement(sort, start, title);
	    break;
	}
	end[0] = '\0';	/* replace the space in " -"  with null */
	parse_statement(sort, start, title);
	PARSE.lineno++;
	start = end + 1;	/* next statement starts after the null */
	end += 2;		/* skip over " -" (now "\0-") */
    }
    free(str);
}

#if defined(DEBUG2)
void print_statements(sort_t *sort)
{
    const tokenqual_t	*cmd;

    for (cmd = &StatementQualifiers[0]; cmd->token != tokEOF; cmd++)
    {
	fprintf(stderr, "-%s", NsortKeywords[cmd->token].name);
    	if (cmd->quals)
	{
	    fputc('=', stderr);
	    print_token_list(cmd->quals,
			     strlen(NsortKeywords[cmd->token].name) + 1,
			     TRUE);
	}
	fputc('\n', stderr);
    }
}
#endif
