#ifndef TYPE_H
#define TYPE_H
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL type system declarations.
 */

struct scope;

/*
 * Data representation standards.
 */
enum datarep {
	drNil = 0,		/* unknown */
	drBigEndian = 1,	/* MSB first */
	drLittleEndian = 2,	/* LSB first */
	drXDR = 3,		/* Sun XDR */
	drASN1 = 4,		/* ASN.1 */
	drMax
};

/*
 * Base type codes and macro operators.  The encodings are carefully chosen;
 * see the macros below and the arrays in pidl_type.c and cg_*.c.
 */
enum basetype {
	btNil = 0,		/* unknown */
	btVoid = 1,		/* void */
	btChar = 2,		/* char, signed 8-bit int */
	btUChar = 3,		/* u_char */
	btShort = 4,		/* short, signed 16-bit int */
	btUShort = 5,		/* u_short */
	btInt = 6,		/* signed 32-bit int */
	btUInt = 7,		/* u_int */
	btSigned = 8,		/* signed int type modifier */
	btUnsigned = 9,		/* u_modifier */
	btLong = 10,		/* long, 32-bit int */
	btULong = 11,		/* u_long */
	btFloat = 12,		/* IEEE 32-bit float */
	btDouble = 13,		/* IEEE 64-bit float */
	btCache = 14,		/* protocol cache */
	btAddress = 15,		/* protocol address */
	btBool = 16,		/* RPCL bool */
	btOpaque = 17,		/* RPCL opaque */
	btString = 18,		/* RPCL string */
	btEnum = 19,		/* enumerated type */
	btStruct = 20,		/* structure */
	btTypeDef = 21,		/* defined type */
	btMax
};

#define	isintegral(bt)	(btChar <= (bt) && (bt) <= btULong)
#define	isarithmetic(bt)(btChar <= (bt) && (bt) <= btDouble)
#define	isordinal(bt)	((bt) == btEnum || isintegral(bt))
#define	issigned(bt)	(isintegral(bt) && !((bt) & 1))
#define	isunsigned(bt)	(isintegral(bt) && ((bt) & 1))
#define	signable(bt)	(issigned(bt) && (bt) != btSigned)
#define	unsignable(bt)	(issigned(bt) && (bt) != btSigned)
#define	sign(bt)	((bt) & ~1)
#define	unsign(bt)	((bt) | 1)

/*
 * Type qualifiers.
 */
enum typequal {
	tqNil = 0,		/* no qualifier */
	tqPointer = 1,		/* pointer to type */
	tqArray = 2,		/* fixed-length array */
	tqVarArray = 3,		/* variable-length array */
	tqMax
};

#define	TQSHIFT		2			/* width of enum typequal */
#define	TQMASK		((1 << TQSHIFT) - 1)	/* and typequal mask */

#define	TSBITS		10			/* bits of type symbol index */
#define	TQMAX		6			/* max. number of qualifiers */
#define	TQBITS		(TQMAX * TQSHIFT)	/* bits of type qualifiers */

/*
 * A typeword describes everything about a PIDL type except array dimensions
 * and bitfield width, which are stored in each field's abstract syntax node,
 * The bit fields are grouped to fit in 16-bit u_ints.
 */
typedef	union {
	unsigned long	tw_word;	/* word for clearing */
	struct {
		unsigned int	symndx:TSBITS;	/* type symbol index */
		unsigned int	base:5;		/* base type code */
		unsigned int	bitfield:1;	/* true if a bitfield */
		unsigned int	qual:TQBITS;	/* type qualifiers */
		unsigned int	rep:4;		/* data representation */
	} tw_type;
} typeword_t;

#define	tw_symndx	tw_type.symndx
#define	tw_base		tw_type.base
#define	tw_bitfield	tw_type.bitfield
#define	tw_qual		tw_type.qual
#define	tw_rep		tw_type.rep

#define	setbasetype(tw, bt)	((tw)->tw_symndx = (tw)->tw_base = (bt))

#define	qualifier(tw)	((tw).tw_qual & TQMASK)
#define	ordinaltype(tw)	(isordinal((tw).tw_base) && (tw).tw_qual == 0)
#define	typesym(tw)	(typesyms[(tw).tw_symndx])

extern struct symbol	*typesyms[];

/*
 * Base type sizes.  Not necessarily the same as sizeof(base-type) on the
 * compiler's host machine.
 */
#define	BitsPerByte	8
#define	BytesPerChar	1
#define	BytesPerShort	2
#define	BytesPerInt	4
#define	BytesPerLong	4
#define	BytesPerFloat	4
#define	BytesPerDouble	8
#define	BytesPerAddress	8

#define	BitsPerChar	(BytesPerChar * BitsPerByte)
#define	BitsPerShort	(BytesPerShort * BitsPerByte)
#define	BitsPerInt	(BytesPerInt * BitsPerByte)
#define	BitsPerLong	(BytesPerLong * BitsPerByte)
#define	BitsPerFloat	(BytesPerFloat * BitsPerByte)
#define	BitsPerDouble	(BytesPerDouble * BitsPerByte)
#define	BitsPerAddress	(BytesPerAddress * BitsPerByte)

/*
 * Type descriptor struct for statically initialized tables.
 */
struct typedesc {
	char		*td_name;	/* symbolic type name */
	unsigned short	td_namlen;	/* and length */
	unsigned char	td_code;	/* type code, e.g. drBigEndian, btInt */
	unsigned char	td_width;	/* bit width of base type */
};

extern void		installtypes(struct scope *);
extern void		definetype(struct symbol *);
extern typeword_t	typeword(struct symbol *, struct symbol *,
				 struct symbol *, struct symbol *,
				 enum datarep);
extern typeword_t	tagtype(struct symbol *, enum datarep, enum basetype);
extern typeword_t	qualify(typeword_t, enum typequal);
extern typeword_t	unqualify(typeword_t);
extern int		dimensions(typeword_t);
extern struct treenode	*declaretype(typeword_t, struct treenode *);
extern char		*typename(typeword_t);

#endif
