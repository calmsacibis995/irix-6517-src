/* excLib.h - exception library header */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,14nov93,hdn  added excExcepHook.
01b,08jun93,hdn  added support for c++. updated to 5.1.
01a,28feb92,hdn  written based on TRON, 68k version.
*/

#ifndef __INCexcI86Libh
#define __INCexcI86Libh

#ifdef __cplusplus
extern "C" {
#endif

#define LOW_VEC		0		/* lowest initialized vector */
#define HIGH_VEC	0xff		/* highest initialized vector */

typedef struct
    {
    UINT16 valid;	/* indicators that following fields are valid */
    UINT16 vecNum;	/* vector number */
    ULONG errCode;	/* error code */
    INSTR *pc;		/* program counter */
    ULONG statusReg;	/* status register */
    UINT16 csReg;	/* code segment register */
    UINT16 pad;		/* pad to four byte boundary */
    } EXC_INFO;

/* exception info valid bits */

#define EXC_VEC_NUM		0x01	/* vector number valid */
#define EXC_ERROR_CODE		0x02	/* error code valid */
#define EXC_PC			0x04	/* pc valid */
#define EXC_STATUS_REG		0x08	/* status register valid */
#define EXC_CS_REG		0x10	/* code segment register valid */
#define EXC_INVALID_TYPE	0x80	/* special indicator: ESF type was bad;
					 * type is in funcCode field */

/* variable declarations */

extern FUNCPTR  excExcepHook;   /* add'l rtn to call when exceptions occur */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void	excStub (void);
extern void	excIntStub (void);

#else	/* __STDC__ */

extern void	excStub ();
extern void	excIntStub ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCexcI86Libh */
