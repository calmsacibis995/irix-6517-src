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
 *	print.c	- print sort descriptions, record headers, record data,..
 *
 *	$Ordinal-Id: print.c,v 1.17 1996/10/02 22:27:53 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include <stdio.h>
#include "otcsort.h"
#include <limits.h>

typedef struct typeinfo
{
	short	length;
	short	printwidth;
	char	*name;
} typeinfo_t;

typeinfo_t NsortTypeInfo[] =
{
			  /* data len  prt len  title */
	/* typeI8	    */ 8,	17,	"Int8",
	/* typeI4	    */ 4,	11,	"Int4",
	/* typeI2	    */ 2,	6,	"Int2",
	/* typeI1	    */ 1,	4,	"Int1",
	/* typeU8	    */ 8,	17,	"Unsign8",
	/* typeU4	    */ 4,	10,	"Unsign4",
	/* typeU2	    */ 2,	6,	"Unsign2",
	/* typeU1	    */ 1,	4,	"Unsign1",
	/* typeFloat	    */ 4,	13,	"Float",
	/* typeDouble	    */ 8,	16,	"Double",
	/* typeDecimal	    */ 250,	15,	"Packed",
	/* FixedDecimalString*/	250,	15,	"Decimal",
	/* DelimDecimalString*/ 250,	15,	"Decimal",
	/* typeFixedString  */ 250,	16,	"Char",
	/* typeLengthString */ 250,	16,	"Char",
	/* typeDelimString  */ 250,	16,	"Char"
};

char *NsortMethodName[] =
{
	"<unspecfied>",		/* SortMethUnspecified */
	"Record",		/* SortMethRecord */
	"Pointer"		/* SortMethPointer */
};

char *print_char(char ch, char *dest)
{
    if (isgraph(ch))
    {
	dest[0] = '\'';
	dest[1] = ch;
	dest[2] = '\'';
	dest[3] = '\0';
    }
    else switch (ch)
    {
      case '\0': strcpy(dest, "null"); break;
      case '\t': strcpy(dest, "tab"); break;
      case '\n': strcpy(dest, "newline"); break;
      case ' ' : strcpy(dest, "space"); break;
      default  : sprintf(dest, "'\\0%o'", ch); break;
    }
    return (dest);
}

void nsort_print_sort(sort_t *sort, FILE *fp)
{
    fprintf(fp, "%s %s", NsortMethodName[sort->method],
	sort->mergeonly ? "merge" : "sort");
    				  
    fputs(" of: ", fp);
    nsort_print_record(&sort->record, fp);
    if (sort->keycount > 0)
    {
	fputs("Keys: ", fp);
	nsort_print_keys(sort, fp);
	fputc('\n', fp);
    }
}

void nsort_print_record(record_t *rec, FILE *fp)
{
    char	delimstr[10];
    char	padstr[10];

    if (rec->flags & RECORD_FIXED)
    	fprintf(fp, "Fixed length record ");
    else if (rec->flags & RECORD_VARIABLE)
    	fprintf(fp, "Length-prefix record ");
    else if (rec->flags & RECORD_STREAM)
    	fprintf(fp, "Stream record ending in %s pad %s",
		    print_char(rec->delimiter, delimstr),
		    print_char(rec->pad, padstr));
    fprintf(fp, "\nLength is %d bytes taken up by %d field%s\n",
    	rec->length, rec->fieldcount, (rec->fieldcount == 1) ? "" : "s");
    nsort_print_fields(rec, fp);
}

/*
** nsort_print_fields - print the descriptions of a record's fields as text
*/
void nsort_print_fields(record_t *rec, FILE *fp)
{
    field_t	*field;
    char	delimstr[10];
    char	padstr[10];
    
    for (field = rec->fields; field->type != typeEOF; field++)
    {
    	if (field->flags & FIELD_VARIABLE)
		fputs("variable", fp);
	    else
		fputs("        ", fp);
	fprintf(fp, " %8s", NsortTypeInfo[field->type].name);
	fprintf(fp, " len %3d off %3d", field->length,
					field->position - rec->var_hdr_extra);
	switch (field->type)
	{
	  case typeDelimString:
	  case typeDelimDecimalString:
	    fprintf(fp, " delim %s", print_char(field->delimiter, delimstr));
	    /* Fall through */
	  case typeFixedString:
	  case typeLengthString:
	    fprintf(fp, " pad %s", print_char(field->pad, padstr));
	    break;
	}
	if (FIELD_NAMED(field->name))
	    fprintf(fp, " %s", field->name);
	fputc('\n', fp);
    }
}

/*
** nsort_print_keys - print the descriptions of a record's keys as text
*/
void nsort_print_keys(sort_t *sort, FILE *fp)
{
    keydesc_t	*key;
    field_t	*field;
    char	delimstr[10];
    char	padstr[10];
    
    for (key = sort->keys; key->type != typeEOF; key++)
    {
	for (field = RECORD.fields; field->type != typeEOF; field++)
	{
	    if (key->position == field->position &&
		key->type == field->type &&
		(key->length == field->length ||
		 (field->length == 0 && key->type == typeDelimString)))
		break;
	}
	if (key != sort->keys)
	    fputc(',', fp);
	if (field->type != typeEOF && FIELD_NAMED(field->name))
	{
	    fprintf(fp, "%s", field->name);
	}
	else
	{
	    fprintf(fp, "%8s", NsortTypeInfo[key->type].name);
	    fprintf(fp, " len %3d off %3d", key->length, key->position -
							 RECORD.var_hdr_extra);
	    if (key->flags & FIELD_DESCENDING)
		fputs(" descending", fp);
	    switch (key->type)
	    {
	      case typeDelimString:
	      case typeDelimDecimalString:
		fprintf(fp, " delim %s", print_char(key->delimiter, delimstr));
		/* Fall through */
	      case typeFixedString:
	      case typeLengthString:
		fprintf(fp, " pad %s", print_char(key->pad, padstr));
		break;
	    }
	}
	if (key->flags & FIELD_DESCENDING)
	    fputs(":desc", fp);
    }
}
/*
** nsort_print_keydata_header - print the key names (before printing the keys)
*/
void nsort_print_keydata_header(sort_t *sort, FILE *fp)
{
    keydesc_t	*key;
    field_t	*field;
    int		minwidth;
    char	*name;
    
    fputc('|', fp);
    for (key = sort->keys; key->type != typeEOF; key++)
    {
	name = "";
	for (field = RECORD.fields; field->type != typeEOF; field++)
	{
	    if (key->position == field->position &&
		key->length == field->length)
	    {
		name = field->name;
		break;
	    }
	}

	minwidth = max((int) strlen(name), NsortTypeInfo[key->type].printwidth);
	fprintf(fp, "%*s|", minwidth, name);
    }
    fputc('\n', fp);
}


/*
** nsort_print_data_header - print the field names (before printing the data)
*/
void nsort_print_data_header(sort_t *sort, unsigned print_filler, FILE *fp)
{
    field_t	*field;
    int		minwidth;
    
    if (!print_filler)
    {
	nsort_print_keydata_header(sort, fp);
	return;
    }

    fputc('|', fp);
    for (field = RECORD.fields; field->type != typeEOF; field++)
    {
    	/*
	** A boring (filler) field may be denoted by a null field name
	*/
	if (field->name == NULL)
	    continue;

	minwidth = max((int) strlen(field->name), NsortTypeInfo[field->type].printwidth);
	fprintf(fp, "%*s|", minwidth, field->name);
    }
    fputc('\n', fp);
}

#define VTBOLD	"\033[1;7m"
#define VTUL	"\033[4m"
#define VTNORM	"\033[m"

void print_string(const byte *p, int len, int delim, FILE *fp)
{
    int	i;
    int ch;

    for (i = len; i > 0; i--, p++)
    {
	if (delim >= 0 && *p == (char) delim)
	    break;
	if ((ch = *p) >= 0x80)
	{
	    fputs(VTBOLD, fp);
	    ch &= 0x7f;
	}
	if (ch == '\0')
	    fputs(VTUL "0" VTNORM, fp);
	else if (ch == '\n')
	    fputs(VTUL "n" VTNORM, fp);
	else if (ch == '\t')
	    fputs(VTUL "t" VTNORM, fp);
	else if (iscntrl(ch))
	    fprintf(fp, VTUL "^%c" VTNORM, ch + '@');
	else
	{
	    putc(ch, fp);
	    if (*p >= 0x80)
		fputs(VTNORM, fp);
	}
    }
}

/*
** print_datum	- print a single value as a certain typ
**
*/
void print_datum(type_t type, int position, int length, char delimiter, char *name, CONST byte *data, int reclen, FILE *fp)
{
    CONST byte	*p;
    int		minwidth;
    i4		tempi4;
    i8		tempi8;
    u8		tempu8;
    i2		tempi2;
    float	tempfloat;
    double	tempdouble;

    /*
    ** A very boring (filler) field is denoted by a null field name
    */
    if (name == NULL)
	return;

    minwidth = max((int) strlen(name), NsortTypeInfo[type].printwidth);
    p = data + position;

    switch (type)
    {
      case typeI1:
	fprintf(fp, "%*d", minwidth, *(i1 *) p);
	break;

      case typeI2:
	memmove(&tempi2, p, 2);
	fprintf(fp, "%*d", minwidth, tempi2);
	break;

      case typeI4:
	memmove(&tempi4, p, 4);
	fprintf(fp, "%*d", minwidth, tempi4);
	break;

      case typeI8:
	memmove(&tempi8, p, 8);
	fprintf(fp, "%*lld", minwidth, tempi8);
	break;

      case typeU8:
	memmove(&tempu8, p, 8);
	fprintf(fp, "%*llu", minwidth, tempu8);
	break;

      case typeU1:
	fprintf(fp, "%*u", minwidth, *(u1 *) p);
	break;

      case typeU2:
	memmove(&tempi2, p, 2);
	fprintf(fp, "%*u", minwidth, (u2) tempi2);
	break;

      case typeU4:
	memmove(&tempi4, p, 4);
	fprintf(fp, "%*u", minwidth, tempi4);
	break;

      case typeFloat:
	memmove(&tempfloat, p, 4);
	fprintf(fp, "%*g", minwidth, tempfloat);
	break;

      case typeDouble:
	memmove(&tempdouble, p, 8);
	fprintf(fp, "%*g", minwidth, tempdouble);
	break;

      case typeDelimString:
      case typeDelimDecimalString:
	print_string(p, reclen - position, delimiter, fp);
	break;
	    
      case typeFixedDecimalString:
      case typeFixedString:
	print_string(p, length, -1, fp);
	break;

      case typeLengthString:    /* whoops! this type doesn't exist yet */
      default:
	die("print_datum:unknown type %d", type);
	break;
    }
}

/*
** nsort_print_data	- print the values of a record's fields
**
**	Paramters:
**		rec	     - record_t contains the list of fields to display
**		data	     - address of the start of a data record
**		fp	     - stdio destination at which to write the data 
**		print_header - print a 'tuple header' line before the data
**
**
**	This version only handles version 1-style fields.
**	In particular each field always starts at the same offset
**	from the start of the record.
*/
void nsort_print_data(sort_t *sort,
		      CONST byte *data,
		      FILE *fp,
		      unsigned print_header,
		      unsigned print_filler)
{
    record_t	*rec;
    field_t	*field;
    int	reclen;

    if (!print_filler)
    {
	nsort_print_keydata(sort, data, fp, print_header);
	return;
    }

    rec = &sort->record;
    if (print_header)
    	nsort_print_data_header(sort, print_filler, fp);

    if (rec->flags & RECORD_FIXED)
    	reclen = rec->length;
    else if (rec->flags & RECORD_VARIABLE)
    	reclen = 2 + ulh(data);
    else
    	reclen = (int) ((byte *) memchr(data + RECORD.minlength - 1,
					RECORD.delimiter,
					SHRT_MAX) -
			data);
    if (sort->compute_hash)
    {
	unsigned	hash = calc_hash(sort, data, reclen);

	fputc('|', fp);
	print_datum(typeU4, 0, 4, '\0', "hash", (byte *) &hash, reclen, fp);
    }
	
    fputc('|', fp);
    for (field = rec->fields; field->type != typeEOF; field++)
    {
	print_datum(field->type, field->position, field->length, field->delimiter, field->name, data, reclen, fp);

	if ((field->position + field->length > reclen)  &&
	    !(field->flags & FIELD_DERIVED))
	    fprintf(fp, " Field %s(%d, %d) extends beyond end of record (%d)",
		field->name, field->position, field->length, reclen);
	fputc('|', fp);
    }
    fputc('\n', fp);
}

/*
** nsort_print_keydata	- print the values of a record's keys
**
**	Paramters:
**		sort	     - sort_t containing the list of keys to display
**		data	     - address of the start of a data record
**		fp	     - stdio destination at which to write the data 
**		print_header - print a 'tuple header' line before the data
**
**
**	This version only handles version 1-style keys.
**	In particular each key always starts at the same offset
**	from the start of the record.
*/
void nsort_print_keydata(sort_t *sort, CONST byte *data, FILE *fp, unsigned print_header)
{
    keydesc_t	*key;
    int	reclen;
    field_t	*field;
    char	*name;

    if (RECORD.flags & RECORD_FIXED)
    	reclen = RECORD.length;
    else if (RECORD.flags & RECORD_VARIABLE)
    	reclen = 2 + ulh(data);
    else
    	reclen = (int) ((byte *) memchr(data + RECORD.minlength - 1,
					RECORD.delimiter,
					SHRT_MAX) -
			data);
	
    if (sort->compute_hash)
    {
	unsigned	hash = calc_hash(sort, data, reclen);

	fputc('|', fp);
	print_datum(typeU4, 0, 4, '\0', "hash", (byte *) &hash, reclen, fp);
    }

    fputc('|', fp);
    for (key = sort->keys; key->type != typeEOF; key++)
    {
	name = "";
	for (field = RECORD.fields; field->type != typeEOF; field++)
	{
	    if (key->position == field->position &&
		key->length == field->length)
	    {
		name = field->name;
		break;
	    }
	}

	print_datum(key->type, key->position, key->length, key->delimiter, name, data, reclen, fp);

#if 0
	if (key->position + key->length > reclen)
	    fprintf(fp, "!! Key %s(%d, %d) extends beyond end of record (%d)",
		name, key->position, key->length, reclen);
#endif
	fputc('|', fp);
    }
    fputc('\n', fp);
}
