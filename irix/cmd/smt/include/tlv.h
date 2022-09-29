#ifndef TLV_H
#define TLV_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

#define TLV_HDRSIZE     (sizeof(short) * 2)

/* String alignment flag */
#define TLV_NOALIGN	0
#define TLV_PREALIGN	1
#define TLV_POSTALIGN	2
#define TLV_BADALIGN	3

/*
 * SMT INFO FIELDS.
 */
typedef struct {
	u_short	ptype;
	u_short plen;
} SMT_TLV;

typedef union {
	u_long lword;
	SMT_TLV tlv;
} SMTTLV;	/* force long ward align */
#define	tlv_type	tl.tlv.ptype
#define	tlv_len		tl.tlv.plen

#define TLV_ID_MASK	0xf000
#define TLV_SUBID_MASK	0x0f00
#define TLV_OBJ_MASK	0x00ff
#define typetoid(type) ((type&TLV_ID_MASK)>>12)
#define typetosubid(type) ((type&TLV_SUBID_MASK)>>8)
#define typetoobj(type)	(type&TLV_OBJ_MASK)

/*
 * tlv_tl
 *
 *	parse: yank 2byte type into long
 *	       yank 2byte length into long.
 *
 *	build: put long type into 2byte
 *		put long length into 2byte.
 *
 *	Also, check the data length validity.
 */
extern int tlv_parse_tl(char **, u_long *, u_long *, u_long *);
extern int tlv_build_tl(char **, u_long *, u_long, u_long);

/*
 *	tlv_char:
 *		parse: pull out 1byte into integer.
 *		build: put integer into 1bytes.
 */
extern int tlv_parse_char(char **, u_long *, u_long *);
extern int tlv_build_char(char **, u_long *, u_long);

/*
 *	tlv_short:
 *		parse: pull out 2bytes into integer.
 *		build: put integer into 2bytes.
 */
extern int tlv_parse_short(char **, u_long *, u_long	*, int);
extern int tlv_build_short(char **, u_long *, u_long,    int);

/*
 *	tlv_int:
 *		parse: pull out 4bytes into integer.
 *		build: put integer to 4bytes.
 */
extern int tlv_parse_int(char **, u_long *, u_long *, int);
extern int tlv_build_int(char **, u_long *, u_long,	int);

/*
 * tlv_string:
 *
 *	parse/build string.
 */
extern int tlv_parse_string(char **, u_long *, char *, int, int);
extern int tlv_build_string(char **, u_long *, char *, int, int);

#endif
