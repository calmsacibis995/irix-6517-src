#ident "$Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/IP22/include/RCS/saioctl.h,v 1.1 1994/07/20 23:49:06 davidl Exp $"
/* $Copyright: $ */

/*
 * saioctl.h -- standalone ioctl definitions
 */

#ifdef LANGUAGE_C
#include "sys/dkio.h"
#include "sys/gen_ioctl.h"
#endif LANGUAGE_C

#ifndef NULL
#define NULL 0
#endif

#define	EOF	(-1)			/* EOF from getc() */

/*
 * general ioctl's
 */
#define	FIOCNBLOCK	(('f'<<8)|1)	/* set non-blocking io */
#define	FIOCSCAN	(('f'<<8)|2)	/* scan for input */
/*
 * "tty" ioctl's
 */
#define	TIOCRAW		(('t'<<8)|1)	/* no special chars on char devices */
#define	TIOCFLUSH	(('t'<<8)|2)	/* flush input */
#define	TIOCPROTO	(('t'<<8)|3)	/* control pseudo console access */
#define	TIOCREOPEN	(('t'<<8)|4)	/* reopen to effect baud rate chg */
#define TIOCRAWRAW	(('t'<<8)|8)	/* absolutely no special chars */

/*
 * network ioctl's
 */
#define	NIOCBIND	(('n'<<8)|1)	/* bind network address */
		
/*
 * pseudo console ioctl's
 */
#define	PIOCENABLE	(('p'<<8)|1)	/* enable device as console */
#define	PIOCDISABLE	(('p'<<8)|2)	/* disable device as console */
#define	PIOCSHOW	(('p'<<8)|3)	/* show enabled console devices */

/*
 * bit field descriptions for printf %r and %R formats
 */

/*
 * printf("%r %R", val, reg_descp);
 * struct reg_desc *reg_descp;
 *
 * the %r and %R formats allow formatted print of bit fields.  individual
 * bit fields are described by a struct reg_desc, multiple bit fields within
 * a single word can be described by multiple reg_desc structures.
 * %r outputs a string of the format "<bit field descriptions>"
 * %R outputs a string of the format "0x%x<bit field descriptions>"
 *
 * The fields in a reg_desc are:
 *	unsigned rd_mask;	An appropriate mask to isolate the bit field
 *				within a word, and'ed with val
 *
 *	int rd_shift;		A shift amount to be done to the isolated
 *				bit field.  done before printing the isolate
 *				bit field with rd_format and before searching
 *				for symbolic value names in rd_values
 *
 *	char *rd_name;		If non-null, a bit field name to label any
 *				out from rd_format or searching rd_values.
 *				if neither rd_format or rd_values is non-null
 *				rd_name is printed only if the isolated
 *				bit field is non-null.
 *
 *	char *rd_format;	If non-null, the shifted bit field value
 *				is printed using this format.
 *
 *	struct reg_values *rd_values;	If non-null, a pointer to a table
 *				matching numeric values with symbolic names.
 *				rd_values are searched and the symbolic
 *				value is printed if a match is found, if no
 *				match is found "???" is printed.
 *				
 */

/*
 * register values
 * map between numeric values and symbolic values
 */
#ifdef LANGUAGE_C
struct reg_values {
	unsigned rv_value;
	char *rv_name;
};

/*
 * register descriptors are used for formatted prints of register values
 * rd_mask and rd_shift must be defined, other entries may be null
 */
struct reg_desc {
	unsigned rd_mask;	/* mask to extract field */
	int rd_shift;		/* shift for extracted value, - >>, + << */
	char *rd_name;		/* field name */
	char *rd_format;	/* format to print field */
	struct reg_values *rd_values;	/* symbolic names of values */
};
#endif LANGUAGE_C

#ifdef LANGUAGE_C
extern	int errno;	/* just like unix ??? */
#endif LANGUAGE_C

#define	EXCEPT_NORM	1
#define	EXCEPT_UTLB	2
#define	EXCEPT_BRKPT	3
#define	EXCEPT_CACHE	4
#define	EXCEPT_XTLB	5

/*
 * register names
 */
#define	R_R0		0
#define	R_R1		1
#define	R_R2		2
#define	R_R3		3
#define	R_R4		4
#define	R_R5		5
#define	R_R6		6
#define	R_R7		7
#define	R_R8		8
#define	R_R9		9
#define	R_R10		10
#define	R_R11		11
#define	R_R12		12
#define	R_R13		13
#define	R_R14		14
#define	R_R15		15
#define	R_R16		16
#define	R_R17		17
#define	R_R18		18
#define	R_R19		19
#define	R_R20		20
#define	R_R21		21
#define	R_R22		22
#define	R_R23		23
#define	R_R24		24
#define	R_R25		25
#define	R_R26		26
#define	R_R27		27
#define	R_R28		28
#define	R_R29		29
#define	R_R30		30
#define	R_R31		31
#define	R_F0		32
#define	R_F1		33
#define	R_F2		34
#define	R_F3		35
#define	R_F4		36
#define	R_F5		37
#define	R_F6		38
#define	R_F7		39
#define	R_F8		40
#define	R_F9		41
#define	R_F10		42
#define	R_F11		43
#define	R_F12		44
#define	R_F13		45
#define	R_F14		46
#define	R_F15		47
#define	R_F16		48
#define	R_F17		49
#define	R_F18		50
#define	R_F19		51
#define	R_F20		52
#define	R_F21		53
#define	R_F22		54
#define	R_F23		55
#define	R_F24		56
#define	R_F25		57
#define	R_F26		58
#define	R_F27		59
#define	R_F28		60
#define	R_F29		61
#define	R_F30		62
#define	R_F31		63
#define	R_EPC		64
#define	R_MDHI		65
#define	R_MDLO		66
#define	R_SR		67
#define	R_CAUSE		68
#define	R_TLBHI		69
#define R_PID		R_TLBHI
#define	R_TLBLO		70
#define	R_BADVADDR	71
#define	R_INX		72
#define	R_RAND		73
#define	R_CTXT		74
#define	R_EXCTYPE	75
#define	R_C1_EIR	76
#define	R_C1_SR		77
#define R_ERROR		78
/* r4000 only registers */
#define R_TLBLO0	R_TLBLO
#define R_TLBLO1	79
#define R_PAGEMASK	80
#define R_WIRED		81
#define R_COUNT		82
#define R_COMPARE	83
#define R_CONFIG	84
#define R_LLADDR	85
#define R_WATCHLO	86
#define R_WATCHHI	87
#define R_XCONTEXT	88
#define R_ECC		89
#define R_CACHEERR	90
#define R_TAGLO		91
#define R_TAGHI		92
#define R_ERROREPC	93

#define	NREGS		94

/*
 * compiler defined bindings
 */
#define	R_ZERO		R_R0
#define	R_AT		R_R1
#define	R_V0		R_R2
#define	R_V1		R_R3
#define	R_A0		R_R4
#define	R_A1		R_R5
#define	R_A2		R_R6
#define	R_A3		R_R7
#define	R_T0		R_R8
#define	R_T1		R_R9
#define	R_T2		R_R10
#define	R_T3		R_R11
#define	R_T4		R_R12
#define	R_T5		R_R13
#define	R_T6		R_R14
#define	R_T7		R_R15
#define	R_S0		R_R16
#define	R_S1		R_R17
#define	R_S2		R_R18
#define	R_S3		R_R19
#define	R_S4		R_R20
#define	R_S5		R_R21
#define	R_S6		R_R22
#define	R_S7		R_R23
#define	R_T8		R_R24
#define	R_T9		R_R25
#define	R_K0		R_R26
#define	R_K1		R_R27
#define	R_GP		R_R28
#define	R_SP		R_R29
#define	R_FP		R_R30
#define	R_RA		R_R31

/*
 * Stack modes
 */
#define	MODE_NORMAL	0	/* executing on normal stack */
#define	MODE_FAULT	1	/* executing on fault stack */
