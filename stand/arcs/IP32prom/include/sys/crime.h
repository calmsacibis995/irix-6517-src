#ifndef _SYS_CRIME_H__
#define _SYS_CRIME_H__

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include <sys/types.h>
#include <sys/reg.h>

typedef unsigned long long _crmreg_t;
unsigned long long read_reg64(__psunsigned_t);
void write_reg64(long long, __psunsigned_t);

#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */

/*
 * Crime memory/cpu control status registers
 */
#define CRM_BASEADDR	0x14000000

#define CRM_ID		(CRM_BASEADDR + 0x00000000)
#define CRM_ID_MSK	0xffLL	/* 8-bit register */

/*
 * CRIME ID register bits
 */
#define CRM_ID_IDBITS	0xf0
#define CRM_ID_IDVALUE	0xa0
#define CRM_ID_REV	0x0f

#define CRM_CONTROL		(CRM_BASEADDR + 0x8)
#define CRM_CONTROL_MSK		0x3fffLL	/* 14-bit registers */

/*
 * CRIME control register bits
 */
#define CRM_CONTROL_TRITON_SYSADC       0x2000
#define CRM_CONTROL_CRIME_SYSADC        0x1000
#define CRM_CONTROL_HARD_RESET		0x0800
#define CRM_CONTROL_SOFT_RESET		0x0400
#define CRM_CONTROL_DOG_ENA		0x0200
#define CRM_CONTROL_ENDIANESS		0x0100

/* values for CRM_CONTROL_ENDIANESS */
#define CRM_CONTROL_ENDIAN_BIG		0x0100
#define CRM_CONTROL_ENDIAN_LITTLE	0x0000

#define CRM_CONTROL_CQUEUE_HWM		0x000f
#define CRM_CONTROL_CQUEUE_SHFT		0
#define CRM_CONTROL_WBUF_HWM		0x00f0
#define CRM_CONTROL_WBUF_SHFT		8

/*
 * macros to manipulate CRIME High Water Mark bits in
 * the CRIME control register.  Examples:
 *
 * foo = CRM_CONTROL_GET_CQUEUE_HWM(*(__uint64_t *)CRM_CONTROL)
 * CRM_CONTROL_SET_CQUEUE_HWM(*(__uint64_t *)CRM_CONTROL, 4)
 *
 * foo = CRM_CONTROL_GET_WBUF_HWM(*(__uint64_t *)CRM_CONTROL)
 * CRM_CONTROL_SET_WBUF_HWM(*(__uint64_t *)CRM_CONTROL, 4)
 */
#define CRM_CONTROL_GET_CQUEUE_HWM(x)	\
	(((x) & CRM_CONTROL_CQUEUE_HWM) >> CRM_CONTROL_CQUEUE_SHFT)
#define CRM_CONTROL_SET_CQUEUE_HWM(x,v)	\
	(((v) << CRM_CONTROL_CQUEUE_SHFT) | ((x) & ~CRM_CONTROL_CQUEUE_HWM))

#define CRM_CONTROL_GET_WBUF_HWM(x)	\
	(((x) & CRM_CONTROL_WBUF_HWM) >> CRM_CONTROL_WBUF_SHFT)
#define CRM_CONTROL_SET_WBUF_HWM(x,v)	\
	(((v) << CRM_CONTROL_WBUF_SHFT) | ((x) & ~CRM_CONTROL_WBUF_HWM))

#define CRM_INTSTAT	(CRM_BASEADDR + 0x10)
#define CRM_INTSTAT_MSK	0xffffffffLL	/* all intr registers are 32 bit */
#define CRM_INTMASK	(CRM_BASEADDR + 0x18)
#define CRM_INTMASK_MSK	0xffffffffLL	/* all intr registers are 32 bit */
#define CRM_SOFTINT	(CRM_BASEADDR + 0x20)
#define CRM_SOFTINT_MSK	0xffffffffLL	/* all intr registers are 32 bit */
#define CRM_HARDINT	(CRM_BASEADDR + 0x28)
#define CRM_HARDINT_MSK	0xf0ffffffLL	/* all intr registers are 32 bit */

/*
 * CRIME interrupt register bits.
 */
#define CRM_INT_VICE	0x80000000LL
#define CRM_INT_SOFT5	0x40000000LL
#define CRM_INT_SOFT4	0x20000000LL
#define CRM_INT_SOFT3	0x10000000LL
#define CRM_INT_SOFT2	0x08000000LL
#define CRM_INT_SOFT1	0x04000000LL
#define CRM_INT_SOFT0	0x02000000LL
#define CRM_INT_RE3	0x01000000LL
#define CRM_INT_RE1	0x00800000LL
#define CRM_INT_RE0	0x00400000LL
#define CRM_INT_MEMERR	0x00200000LL
#define CRM_INT_CRMERR	0x00100000LL
#define CRM_INT_GBE3	0x00080000LL
#define CRM_INT_GBE2	0x00040000LL
#define CRM_INT_GBE1	0x00020000LL
#define CRM_INT_GBE0	0x00010000LL
#define CRM_INT_GBEx	CRM_INT_GBE0|CRM_INT_GBE1|CRM_INT_GBE2|CRM_INT_GBE3
#define CRM_INT_MACE(i)	((long long)(1 << (i)))

#define NCRMINTS	32	/* number of interrupt sources in CRIME     */
#define NCRMGANG	16	/* number of interrupts which can be ganged */


#define CRM_DOG			(CRM_BASEADDR + 0x30)
#define McGriff			CRM_DOG
#define CRM_DOG_MSK		0x1fffffLL
#define CRM_DOG_POWER_ON_RESET	0x10000LL
#define CRM_DOG_WARM_RESET	0x080000LL
#define CRM_DOG_TIMEOUT		(CRM_DOG_POWER_ON_RESET|CRM_DOG_WARM_RESET)
#define CRM_DOG_VALUE		0x7fffLL

#define CRM_TIME		(CRM_BASEADDR + 0x38)
#define CRM_TIME_MSK		0xffffffffffffLL

#define CRM_CPU_ERROR_ADDR	(CRM_BASEADDR + 0x40)
#define CRM_CPU_ERROR_ADDR_SHFT	0
#define CRM_CPU_ERROR_ADDR_MSK	0x3ffffffffLL

#define CRM_CPU_ERROR_STAT		(CRM_BASEADDR + 0x48)
#define CRM_CPU_ERROR_ENA		(CRM_BASEADDR + 0x50)

/* 
 * bit definitions for CRIME/VICE error status and enable registers
 */
#define CRM_CPU_ERROR_MSK		0x3ffLL	/* cpu error stat is 9 bits */
#define CRM_CPU_ERROR_CPU_INV_ADDR_RD   0x200
#define CRM_CPU_ERROR_VICE_II		0x100
#define CRM_CPU_ERROR_VICE_SYSAD	0x80
#define CRM_CPU_ERROR_VICE_SYSCMD	0x40
#define CRM_CPU_ERROR_VICE_INV_ADDR	0x20
#define CRM_CPU_ERROR_CPU_II		0x10
#define CRM_CPU_ERROR_CPU_SYSAD		0x8
#define CRM_CPU_ERROR_CPU_SYSCMD	0x4
#define CRM_CPU_ERROR_CPU_INV_ADDR_WR	0x2
#define CRM_CPU_ERROR_CPU_INV_REG_ADDR	0x1

#define CRM_VICE_ERROR_ADDR	        (CRM_BASEADDR + 0x58)
#define CRM_VICE_ERROR_ADDR_MSK	        0x3fffffff

#define CRM_MEM_CONTROL			(CRM_BASEADDR + 0x200)
#define CRM_MEM_CONTROL_MSK		0x3LL	/* 2 bit register */
#define CRM_MEM_CONTROL_ECC_ENA		0x10
#define CRM_MEM_CONTROL_USE_ECC_REPL	0x20

/*
 * macros for CRIME memory bank control registers.
 */
#define CRM_MAXBANKS                    8
#define CRM_MEM_BANK_CTRL(x)		(CRM_BASEADDR + (0x208 + ((x) * 8)))
#define CRM_MEM_BANK_CTRL_MSK		0x11fLL	/* 9 bits 7:5 reserved */
#define CRM_MEM_BANK_CTRL_ADDR		0x1f
#define CRM_MEM_BANK_CTRL_SDRAM_SIZE	0x100
#define CRM_MEM_BANK_CTRL_ADDR_SHFT	0
#define CRM_MEM_BANK_CTRL_BANK_TO_ADDR(x) \
	(((x) & CRM_MEM_BANK_CTRL_ADDR) << 25)

#define CRM_MEM_REFRESH_CNTR		(CRM_BASEADDR + 0x248)
#define CRM_MEM_REFRESH_CNTR_MSK	0x7ffLL	/* 11-bit register */

/*
 * CRIME Memory error status register bit definitions
 */
#define CRM_MEM_ERROR_STAT		(CRM_BASEADDR + 0x250)
#define CRM_MEM_ERROR_STAT_MSK		0x0ff7ffffLL	/* 28-bit register */
#define CRM_MEM_ERROR_MACE_ID		0x0000007f
#define CRM_MEM_ERROR_MACE_ACCESS	0x00000080
#define CRM_MEM_ERROR_RE_ID		0x00007f00
#define CRM_MEM_ERROR_RE_ACC		0x00008000
#define CRM_MEM_ERROR_GBE_ACCESS	0x00010000
#define CRM_MEM_ERROR_VICE_ACCESS	0x00020000
#define CRM_MEM_ERROR_CPU_ACCESS	0x00040000
#define CRM_MEM_ERROR_RESERVED		0x00080000
#define CRM_MEM_ERROR_SOFT_ERR		0x00100000
#define CRM_MEM_ERROR_HARD_ERR		0x00200000
#define CRM_MEM_ERROR_MULTIPLE		0x00400000
#define CRM_MEM_ERROR_MEM_ECC_RD	0x00800000
#define CRM_MEM_ERROR_MEM_ECC_RMW	0x01000000
#define CRM_MEM_ERROR_INV_MEM_ADDR_RD	0x02000000
#define CRM_MEM_ERROR_INV_MEM_ADDR_WR	0x04000000
#define CRM_MEM_ERROR_INV_MEM_ADDR_RMW	0x08000000

#define CRM_MEM_ERROR_ADDR		(CRM_BASEADDR + 0x258)
#define CRM_MEM_ERROR_ADDR_MSK		0x3fffffffLL
#define CRM_MEM_ERROR_ADDR_SHFT		0

#define CRM_MEM_ERROR_ECC_SYN		(CRM_BASEADDR + 0x260)
#define CRM_MEM_ERROR_ECC_SYN_MSK	0xffffffffLL
#define CRM_MEM_ERROR_ECC_SYN_SHFT	0

#define CRM_MEM_ERROR_ECC_CHK		(CRM_BASEADDR + 0x268)
#define CRM_MEM_ERROR_ECC_CHK_MSK	0xffffffffLL
#define CRM_MEM_ERROR_ECC_CHK_SHFT	0

#define CRM_MEM_ERROR_ECC_REPL		(CRM_BASEADDR + 0x270)
#define CRM_MEM_ERROR_ECC_REPL_MSK	0xffffffffLL
#define CRM_MEM_ERROR_ECC_REPL_SHFT	0

#endif /* _SYS_CRIME_H__ */
