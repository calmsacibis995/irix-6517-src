/* dbgI86Lib.h - header file for arch dependent portion of debugger */

/*
modification history
--------------------
01c,29nov93,hdn  added a definition EDI - EFLAGS.
01b,27aug93,hdn  added support for c++
01a,11sep91,hdn  written based on TRON version.
*/

#ifndef __INCI86dbgh
#define __INCI86dbgh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

typedef struct
    {
    unsigned int	db0;
    unsigned int	db1;
    unsigned int	db2;
    unsigned int	db3;
    unsigned int	db6;
    unsigned int	db7;
    } DBG_REGS;

#define BREAK_ESF ESF0
#define TRACE_ESF ESF0

#define EDI	0	/* register offset in regSet */
#define ESI	1
#define EBP	2
#define ESP	3
#define EBX	4
#define EDX	5
#define ECX	6
#define EAX	7
#define EFLAGS	8

#endif	/* _ASMLANGUAGE */


#define BRK_INST	0x1000	/* instruction hardware breakpoint */
#define BRK_DATAW1	0x1400	/* data write 1 byte breakpoint */
#define BRK_DATAW2	0x1500	/* data write 2 byte breakpoint */
#define BRK_DATAW4	0x1700	/* data write 4 byte breakpoint */
#define BRK_DATARW1	0x1c00	/* data read-write 1 byte breakpoint */
#define BRK_DATARW2	0x1d00	/* data read-write 2 byte breakpoint */
#define BRK_DATARW4	0x1f00	/* data read-write 4 byte breakpoint */

#define BRK_HARDWARE	0x1000	/* hardware breakpoint bit */
#define BRK_HARDMASK	0x1f00	/* hardware breakpoint mask */


#define SAVED_DBGREGS		0x18		/* 6 debug registers */
#define SAVED_REGS		0x38		/* 8 + 6 registers */

#define BREAK_INST		0xcc		/* int 3 */

#define TRACE_FLAG		0x0100		/* TF in EFLAGS */
#define INT_FLAG		0x0200		/* IF in EFLAGS */

#define MAX_HARDBP_COUNT	4		/* max number of hardware bp */

#define DBG_INST_ALIGN	1

/* instruction patterns and masks */
#define	ENTER			0xc8
#define	PUSH_EBP		0x55
#define	MOV_ESP0		0x89
#define	MOV_ESP1		0xe5
#define	LEAVE			0xc9
#define RET			0xc3
#define RETADD			0xc2
#define CALL_DIR		0xe8
#define CALL_INDIR0		0xff
#define CALL_INDIR1		0x10

#define	ENTER_MASK		0xff
#define	PUSH_EBP_MASK		0xff
#define	MOV_ESP0_MASK		0xff
#define	MOV_ESP1_MASK		0xff
#define	LEAVE_MASK		0xff
#define RET_MASK		0xff
#define RETADD_MASK		0xff
#define CALL_DIR_MASK		0xff
#define CALL_INDIR0_MASK	0xff
#define CALL_INDIR1_MASK	0x38

/* break-point enable/disable bits */

/* trace enable/disable bits */

/* trace status bits */

/* break-point status bits */

#ifdef __cplusplus
}
#endif

#endif /* INCI86dbgh */
