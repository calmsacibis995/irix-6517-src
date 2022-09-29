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
 *  consent of Ordinal Technology Corp.					    *
 ****************************************************************************
 *
 *
 * nsorterrno.h	- error and warning message numbers
 *
 *	$Ordinal-Id: nsorterrno.h,v 1.5 1996/11/05 19:33:31 charles Exp $
 *	$Revision: 1.1 $
 */
#ifndef _NSORTERRNO_H_
#define _NSORTERRNO_H_

#define NSORT_NOERROR			0
#define NSORT_STATEMENT_START		(-1)
#define NSORT_COLON_EXPECTED		(-2)
#define NSORT_COMMA_EXPECTED		(-3)
#define NSORT_UNEXPECTED_IN_LIST	(-4)
#define NSORT_PAREN_EXPECTED_BEFORE	(-5)
#define NSORT_PAREN_EXPECTED_AFTER	(-6)
#define NSORT_AMBIGUOUS_IDENTIFIER	(-7)
#define NSORT_UNEXPECTED_KEYWORD	(-8)
#define NSORT_UNKNOWN_KEYWORD		(-9)
#define NSORT_NEEDS_EQ			(-10)
#define NSORT_NEEDS_POSITIVE_INT	(-11)
#define NSORT_SCALE_OVERFLOW		(-12)
#define NSORT_TYPE_MISMATCH		(-13)
#define NSORT_TYPE_WITHOUT_LENGTH	(-14)
#define NSORT_EXTENDS_PAST_END		(-15)	/* position + length > record len (parse time)*/
#define NSORT_UNSIGNED_WITHOUT_TYPE	(-16)
#define NSORT_FIELD_NEEDS_EQ		(-17)	/* "name" expects "=" or ":" */
#define NSORT_METHOD_NEEDS_EQ		(-18)
#define NSORT_SPECIFICATION_NEEDS_EQ	(-19)
#define NSORT_FIELD_ALREADY_NAMED	(-20)	/* a second NAME=xxx for /field */
#define NSORT_BAD_FIELD_NAME		(-21)	/* no ident found where field name expected */
#define NSORT_BAD_REC_SIZE		(-22)	/* record_size: < 1 or > 65534 no good */
#define NSORT_BAD_REC_SIZE_SPEC		(-23)	/* record_size: needs an int or 'variable' */
#define NSORT_REC_MUST_BE_VARLEN	(-24)	/* max,lrl, or min rec len needs varlen recs */
#define NSORT_REC_MAXLEN_NEEDS_INT	(-25)	/* max or lrl needs :<integer> */
#define NSORT_REC_MAXLEN_INVALID	(-26)	/* max or lrl <1 or > 65534 */
#define NSORT_REC_MINLEN_NEEDS_INT	(-27)	/* min needs :<integer> */
#define NSORT_REC_MINLEN_INVALID	(-28)	/* min < 1 or > 65534 */
#define NSORT_DELIM_NEEDED		(-29)	/* CHARVALUE expected after delimiter: */
#define NSORT_TYPE_NOT_DELIMITABLE	(-30)	/* delimiter for non-string type */
#define NSORT_FIELD_ALREADY_TYPED	(-31)	/* field already has a type */
#define NSORT_FLOAT_SIZE		(-32)	/* float must be length 4 or unspecified */
#define NSORT_DOUBLE_SIZE		(-33)	/* double must be length 8 or unspecified */
#define NSORT_PACKED_UNIMPLEMENTED	(-34)
#define NSORT_PAD_NEEDED			(-35)	/* CHARVALUE expected after pad: */
#define NSORT_POSITION_NEEDED		(-36)	/* INTVALUE expected after position: */
#define NSORT_DUPLICATE_FIELDNAME	(-37)	/* another field already has this name */
#define NSORT_FIELD_SIZE_NEEDED		(-38)	/* INTVALUE expected after size: */
#define NSORT_FIELD_SIZE_DELIM		(-39)	/* field can't have both size: and delimiter: */
#define NSORT_FIELD_ALREADY_SIZED	(-40)	/* field already has a size specification */
#define NSORT_FIELD_ALREADY_DELIMITED	(-41)	/* field already has a delimiter */
#define NSORT_FIELD_SYNTAX		(-42)	/* unknown token inside /field (...) list */
#define NSORT_POSITION_POSITIVE		(-43)	/* position:N but N was zero */
#define NSORT_KEY_FIELD_MISSING		(-44)
#define NSORT_KEY_ALREADY_TYPED		(-45)	/* two type definitions in a key */
#define NSORT_MMAP_ZERO_FAILED		(-46)	/* mmap %d bytes of /dev/zero failed: %s */
#define NSORT_REDUNDANT_ORDERING	(-47)
#define NSORT_KEY_SYNTAX		(-48)
#define NSORT_KEY_NUMBER		(-49)
#define NSORT_KEY_NUMBER_INVALID	(-50)	/* number:n < 1 or > 255 */
#define NSORT_KEY_NUMBER_DUPLICATE	(-51)	/* Number:n when another key already has n */
#define NSORT_DERIVED_NEEDS_VALUE	(-52)
#define NSORT_DERIVED_FIELD_MISSING	(-53)	/* (as yet) unspecified field in /derived stmt */
#define NSORT_NON_NUMERIC_DERIVED	(-54)
#define NSORT_NON_STRING_DERIVED	(-55)
#define NSORT_DERIVED_VALUE_UNSUPPORTED	(-56)
#define NSORT_SUMMARIZE_FIELD_MISSING	(-57)	/* (as yet) unspecified field in /summarize stmt */
#define NSORT_DATA_FIELD_MISSING		(-58)	/* (as yet) unspecified field in /data stmt */
#define NSORT_MEMORY_NEEDED		(-59)	/* /memory= without an integer # of kbytes */
#define NSORT_IOSIZE_UNALIGNED		(-60)	/* {in,out,temp}file buf=N % dio.d_miniosz */
#define NSORT_IOSIZE_TOO_LARGE		(-61)	/* {in,out,temp}file buf=N > dio.d_maxiosz */
#define NSORT_AIOCOUNT_TOO_LARGE	(-62)	/* {in,out,temp}file count=N > _SC_AIO_MAX */
#define NSORT_OUTPUT_ALREADY		(-63)	/* a second /outfile specification */
#define NSORT_CPU_REQ_NEEDED		(-64)	/* /processors= without an integer # of cpus */
#define NSORT_CPU_REQ_TOOBIG		(-65)	/* #cpus asked for > #cpus in this system */
#define NSORT_CPU_REQ_RESTRICTED	(-66)	/* #cpus asked for > #unrestricted cpus in sys*/
#define NSORT_FORMAT_SPEC		(-67)
#define NSORT_BAD_METHOD			(-68)
#define NSORT_BAD_HASHSPEC		(-69)
#define NSORT_VARIABLE_NEEDS_KEY	(-70)
#define NSORT_BAD_CHARACTER_SPEC	(-71)	/* bad escaped character e.g. '\0q' */
#define NSORT_CHARACTER_TOO_LARGE	(-72)	/* oversized character e.g. '\06777' */
#define NSORT_MISSING_QUOTE		(-73)	/* no trailing single quote in a CHARVALUE */
#define NSORT_FILENAME_MISSING		(-74)	/* zero-length filename (e.g. "tempfil=," ) */
#define NSORT_FILESYS_NAME_MISSING	(-75)	/* zero-length filename/filesysname */
#define NSORT_SUMMARIZE_DUPLICATES	(-76)	/* asked to do keep-dups summarizing sort */
#define NSORT_NOT_STATEMENT		(-77)	/* stmt starts with a non-command token */
#define NSORT_EXTRA_INSIDE_STATEMENT	(-78)	/* spurious chars inside statement's comma-list */
#define NSORT_EXTRA_AFTER_STATEMENT	(-79)	/* spurious chars after apparent end-of-statement */
#define NSORT_PREHASHING_INAPPLICABLE	(-80)	/* must be a delete-dup sort to hash */
#define NSORT_RECORD_SORTS_VARLEN	(-81)	/* explicit record sort directive for varlen */
#define NSORT_RECORD_SORT_TOO_LARGE	(-82)	/* explicit record sort directive for big rec */
#define NSORT_RECORD_TOO_LONG		(-83)	/* varlen: rec longer than decl'd max rec len */
#define NSORT_RECORD_TOO_SHORT		(-84)	/* varlen: rec shorter than decl'd min reclen */
#define NSORT_DERIVED_TOO_FAR		(-85)	/* The derived field %s would extend beyond %d */
#define NSORT_PARTIAL_RECORD		(-86)	/* The last, partial record of input file %s was not sorted */
#define NSORT_DELIM_MISSING		(-87)	/* The record at %d in file %d had no record delimiter */
#define NSORT_RADIX_PREHASH_INCOMPAT	(-88)	/* -meth:radix,prehash not supported together */
#define NSORT_FIELD_BEYOND_END		(-89)	/* field extends past eorec (runtime) */
#define NSORT_PAST_MEMORY_LIMIT		(-90)	/* a /memory request beyond rlimit(RSS) */
#define NSORT_SPEC_OPEN			(-91)	/* Specification file %s open failure %s */
#define NSORT_SPEC_TOO_DEEP		(-92)	/* Loop detected: /specification file %s including %s */
#define NSORT_INPUT_OPEN		(-93)	/* Input file %s open failure %s */
#define NSORT_OUTPUT_OPEN	(-94)	/* Input file %s open failure %s */
#define NSORT_FILESYS_NOTFOUND	(-95)	/* stat failure on a filesys or element name */
#define NSORT_TEMPFILE_STAT	(-96)	/* Work_file %s stat failure %s */
#define NSORT_TEMPFILE_BAD_TYPE	(-97)	/* Work_file %s is not a dir or regular file */
#define NSORT_TEMPFILE_OPEN	(-98)	/* Work_file %s open failure %s */
#define NSORT_INPUT_TOO_LARGE	(-99)	/* Input size exceeds that allowed by available memory*/
#define NSORT_PIN_FAILED	(-100)	/* Memory pinning failed at %d of %d kbytes: %s */
#define NSORT_HASH_WITHOUT_RADIX (-101)	/* Hashing is supported only in radix sorts */
#define NSORT_MEMORY_TOO_SMALL	(-102)	/* This sort requires at least %dM of memory */
#define NSORT_INVALID_CONTEXT	(-103)	/* invalid context passed to an api function */
#define NSORT_INVALID_ARGUMENT	(-104)	/* invalid argument to an api function */
#define NSORT_UNMAP_FAIL	(-105)	/* failure %s unmapping 0x%x:0x%x*/
#define NSORT_MALLOC_FAIL	(-106)	/* malloc returned null */
#define NSORT_APIFILES		(-107)	/* api sort has input or output file specification */
#define NSORT_INTERNAL_NO_MEM	(-108)	/* internal sort requires more mem than is available */
#define NSORT_KEY_BEYOND_END	(-109)	/* key extends past eorec (runtime) */
#define NSORT_ERROR_IGNORED	(-110)	/* This sort died due to a fatal err */
#define NSORT_CANCELLED		(-111)	/* sort cancelled; no more ops allowed*/
#define NSORT_CANT_SEEK		(-112)	/* The file %s is not seekable and cannot be used for %s i/o */
#define NSORT_INT_SIZE		(-113)	/* The size of a signed integer may be 1, 2, 4, or 8 bytes */
#define NSORT_SUMMARY_NEEDS_INT (-114)/* A summarizing field must be a 4 or 8 byte integer */
#define NSORT_LAST_ERROR	NSORT_SUMMARY_NEEDS_INT

/*
 * These non-fatal warnings return have positive values
 */
#define NSORT_CLOSE_FAILED	1	/* The file %s was not closed: %s */

#define NSORT_UNLINK_FAILED	2	/* The temp file %s could not be removed: %s */
#define NSORT_LAST_WARNING	NSORT_UNLINK_FAILED

#endif /* _NSORTERRNO_H_ */
