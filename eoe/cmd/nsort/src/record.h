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
 * record.h - describe a record to be sorted
 *
 *
 *	Since the contract lists /fields, /keys, and /recordlayout
 *	but not a /record command, I assume that they are separated
 *	are far as the user is concerned.  This makes the sort description
 *	close to third normal form
 *
 *	$Ordinal-Id: record.h,v 1.14 1996/10/03 00:03:42 charles Exp $
 *	$Revision: 1.1 $
 */

#ifndef _RECORD_H
#define _RECORD_H

#include "ordinal.h"

typedef enum type
{
	typeEOF = -1,
	typeI8,			/* 0 */
	typeI4,
	typeI2,
	typeI1,
	typeU8,			/* 4 */
	typeU4,
	typeU2,
	typeU1,
	typeFloat,		/* 8 */
	typeDouble,
	typePacked,				/* DEC BCD format (which?) */
	typeFixedDecimalString,	/* 11 */	/* [0-9]*.[0-9]* */
	typeFirstStringType	= typeFixedDecimalString,
	typeDelimDecimalString,
	typeFixedString,	/* 13 */
	typeFirstCharType	= typeFixedString,
	typeDelimString,
	typeLengthString,
	typeNumTypes,
	typeBad
} type_t;

#define IsStringType(type)	(((unsigned) (type)) >= typeFirstStringType)
#define IsCharType(type)	(((unsigned) (type)) >= typeFirstCharType)
#define IsDelimType(type)	(((unsigned) (type)) == typeDelimString || \
				 ((unsigned) (type)) == typeDelimDecimalString)
#define IsVariableType(type)	IsDelimType(type)
#define IsNumericType(type)	(((unsigned) (type)) <= typeDouble)
#define IsIntegerType(type)	(((unsigned) (type)) <= typeU1)
#define IsUnsignedType(type)	(((unsigned) (type)) <= typeU1 && \
				 ((unsigned) (type)) >= typeU8)

/*
** The actual data of a record may be in either 
** a fixed-length or variable length format
**
**	Fixed length records contain all fixed length fields.
**	The record representation inside the sorting code is identical
**	to the input data stream/file's
**
**	Variable length records consist of the two byte length
**	followed by the data.
*/

/*
** Expression structure (minimal version)
**
**	This expression structure is only the minimum needed to support
**	record summarization in version 1 nsort. Only constants are supported -
***	no field names or operators!
*/
typedef struct expr
{
    type_t	type;		/* expression result type */
    u2		flags;		/* result type's flags */
    u2		length;
    union anyval *value;
} expr_t;


/* 
** field - describe the type, layout, etc of part of the data
**         in a record to be sorted
**
**	position is a strange bird.  it is:
**	#bytes into record of the field (if preceded by only fixed-len types),
**	or index into the field position table (if preceded by >0 varlen types)
**
**
**	The fields from type through position are the same as in a "keydesc_t".
*/
typedef struct field
{
	type_t	type;		/* enums are 'int's. if wasteful change to i1 */
	u2	flags;		/* FIELD_VARIABLE, FIELD_SHIFTY,.. */
	u1	number;		/* ordering priority given by NUMBER:N qual */
	char	delimiter;	/* delimiter for FIELD_DELIMSTRING */
	char	pad;		/* pad for character and binary strings */
	u2	length;		/* #bytes in field (max # for variable) */
	u2	position;	/* says where to find the field in the data */
	char	*name;		/* never null, may be "<unnamed>" */
	expr_t	*derivation;	/* if !null, the value of this derived field */
} field_t;

#define	FIELD_ODDSTART		0x0003	/* starts in the middle of an ovc */
					/* the previous key is fixed length */
#define	FIELD_DERIVED		0x0004	/* field was not in input record */
#define	FIELD_SHIFTY		0x0008	/* it is preceded by a varlen type */
#define	FIELD_DELIMSTRING	0x0010	/* it is a varlen delimited string */
#define	FIELD_TOGGLESIGN	0x0020	/* condition a signed int/float */
#define	FIELD_VARIABLE		0x0040	/* it is a length-prefix variable str */
#define	FIELD_DESCENDING	0x0080	/* order by descending if set */
#define	FIELD_VARPOSITION	0x0100	/* for RECORD_VARIABLE: the position has
					 * been increased by two byte len size*/

#define POS_INVALID		((u2)  ~0)

#define FIELD_NAMED(name)	((name)[0] != '<')

/*
** key - describe a key to be used in a sort ordering specification
**
**	The 'keyposition' is used by the recode routine to find the key
**	which describes the data to be used in the next step of ovc computation.
**	The ovc.offset for the start of a key is keyposition/sizeof(ovc.offset)
**
**	Since the divide truncates, it is possible for two adjacent one byte
**	fields to have the same offset.  This shouldn't be a problem, since
**	their values will always be combined together ((first << 8) | second)
**	before being compared. In this case, the second field's key
**	would not be used by recode().
**
**	The fields from type through position are the same is in a "field".
**	This is required for parse_key's call to parse_type_def.
*/
typedef struct keydesc
{
	type_t	type;		/* same as field.type */
	u2	flags;		/* same as field.flags */
	u1	number;		/* ordering priority given by NUMBER:N qual */
	char	delimiter;	/* same as field.delimiter */
	char	pad;		/* same as field.pad */
	u2	length;		/* same as field.length */
	u2	position;	/* same as field.position */
	u2	offset;		/* ovc.offset for its first data byte */
	u2	ending_offset;	/* ovc.offset for its last data byte */
	u4	delimword;
	u4	padword;
	u4	xormask;	/* 0 if ascending, ~0 if descending */
	char	*name;		/* name of field, or "<unnamed>" */
} keydesc_t;

/*
** record - describe the layout of chunk of data to be sorted
*/
typedef struct record
{
	u2	fieldcount;
	u2	flags;
	char	delimiter;	/* marks end of the (text, probably) record */
	char	pad;
	u2	length;		/* length of fixed records, max length for var*/
	u4	maxlength;	/* length of fixed records, max length for var*/
	u2	minlength;	/* length of fixed records, min length for var*/
	u2	max_seen;	/* length of largest record seen so far */
	u2	avg_seen;	/* average length of records seen so far */
	u2	var_hdr_extra;	/* 0 for stream, 2 for variable, 0 if fixed */
	u4	max_intern_bytes; /* maximum # bytes for an internal record */
#if 0
	u2	first_varlen;
	u2	fptsize;	/* number of fields described in an fpt entry */
	field_t	*firstvarfield;	/* 1st varlen field is last !FIELD_SHIFTY one */
#endif
	field_t	*fields;	/* fields in record order */
} record_t;

#define RECORD_STREAM	0x01	/* record ends when record.delimiter */
#define RECORD_VARIABLE	0x02	/* two byte length preceeds each record */
#define RECORD_FIXED	0x04	/* each record instance as the same length */
#define RECORD_TYPE	(RECORD_STREAM | RECORD_VARIABLE | RECORD_FIXED)

/*
** Record editing instruction
**	An array of the following items describes how the input record
**	will be transformed before sorting, and eventually how the
**	sorted records will be transformed as they are written to the output.
**	Each edit instruction fills the next length bytes of the dest
**	from the indicated source field or constant.
**	A length of 0 marks the end of the edit instruction list.
*/
typedef struct edit
{
	u1	is_field;	/* get edit data from record, or constant? */
	u1	is_variable;
	u2	position;	/* field->position if real, not derived */
	u4	length;		/* (max) size of this edit's result */
				/* if >= 64K then limit edit to real rec size */
	union edit_source {
	    field_t	*field;		/* needed for delim, pad, + others? */
	    ptrdiff_t	field_num;	/* index into record->fields */
	    byte	*constant;
	} source;
} edit_t;

#define	EDIT_ACTUAL_LENGTH	(64*1024)

/*
** A field position table may be useful for sorts whose keys starting postions
** vary from one record to the next; e.g. tab or comma delimited columns
** of text.
*/
typedef struct fpt
{
	byte	*record;
	u2		positions[4];	/* variable length, actual may be 1..fieldcount */
} fpt_t;

#define FPT_UNSEEN	(~0)	/* the fpt entry doesn't point to a field yet */

#endif	/* _RECORD_H */
