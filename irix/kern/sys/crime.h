#ifndef _SYS_CRIME_H__
#define _SYS_CRIME_H__

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include <sys/types.h>
#include <sys/reg.h>

typedef unsigned long long _crmreg_t;
unsigned long long read_reg64(__psunsigned_t);
void write_reg64(long long, __psunsigned_t);
int get_crimerev(void);

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

#define CRM_REV_PETTY	0x0
#define CRM_REV_11	0x11
#define CRM_REV_13	0x13
#define CRM_REV_14	0x14

#define CRM_IS_PETTY	(get_crimerev() == CRM_REV_PETTY)
#define CRM_IS_REV_1_1	(get_crimerev() == CRM_REV_11)
#define CRM_IS_REV_1_3	(get_crimerev() == CRM_REV_13)
#define CRM_IS_REV_1_4	(get_crimerev() == CRM_REV_14)

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
#define CRM_INT_SOFT2	0x40000000LL
#define CRM_INT_SOFT1	0x20000000LL
#define CRM_INT_SOFT0	0x10000000LL
#define CRM_INT_RE5	0x08000000LL
#define CRM_INT_RE4	0x04000000LL
#define CRM_INT_RE3	0x02000000LL
#define CRM_INT_RE2	0x01000000LL
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
#ifdef MASTER_FREQ
#undef MASTER_FREQ
#endif
#define MASTER_FREQ 66666500    /* Crime upcounter frequency */
#define DNS_PER_TICK 15         /* for delay_calibrate       */
#define PICOSEC_PER_TICK 15000  /* for query_cyclecounter    */

#define CRM_CPU_ERROR_ADDR	(CRM_BASEADDR + 0x40)
#define CRM_CPU_ERROR_ADDR_SHFT	0
#define CRM_CPU_ERROR_ADDR_MSK	0x3ffffffffLL

#define CRM_CPU_ERROR_STAT		(CRM_BASEADDR + 0x48)
#define CRM_CPU_ERROR_ENA		(CRM_BASEADDR + 0x50)

/* 
 * bit definitions for CRIME/VICE error status and enable registers
 */
#define CRM_CPU_ERROR_MSK		0x7LL	/* cpu error stat is 9 bits */
#define CRM_CPU_ERROR_CPU_ILL_ADDR	0x4
#define CRM_CPU_ERROR_VICE_WRT_PRTY	0x2
#define CRM_CPU_ERROR_CPU_WRT_PRTY	0x1

/* 
 * these are the definitions for the error status/enable  register in
 * petty crime.  Note that the enable register does not exist in crime
 * rev 1 and above.
 */
#define CRM_CPU_ERROR_MSK_REV0		0x3ffLL	/* cpu error stat is 9 bits */
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
#define CRM_MEM_CONTROL_ECC_ENA		0x1
#define CRM_MEM_CONTROL_USE_ECC_REPL	0x2

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
#define CRM_MEM_ERROR_RE_ACCESS         0x00008000
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


#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#ifndef _STANDALONE
#include <sys/time.h> /* for stamp_t */

/*
 * Graphics info for other drivers (ie., video capture)
 */
typedef struct crime_frameinfo {
	stamp_t ust;            /* UST acquired atomically with 'line' */
	stamp_t field;          /* current field number */
	int flags;              /* same as ng1_timing_info */
	short fieldrate;        /* fields per sec */
	short linesperframe;    /* lines per frame, including blanking */
	short line;             /* current scanline being displayed */
	char swap_pending;      /* caller has a swapbuf request pending */
	char swaps_canceled;    /* counts canceled swap requests */
} crime_frameinfo_t;

int crime_getustmsc(unsigned int bnum, struct crime_frameinfo *a);

#endif

/*
 * Horizontal Line Interrupt interface.
 */
typedef unsigned int crime_hli_id_t;
typedef unsigned int crime_hli_flags_t;

#define CRIME_VR_REL 0	/* Interrupt line relative to Video Retrace */
#define CRIME_VD_REL 1	/* Interrupt line relative to Video Display */

typedef struct _crime_hli_event {
	crime_hli_id_t		id;
	void 			(*fn)();   /* Function to invoke at interrupt */
	void 			*arg;	   /* Arguments to "fn" */
	unsigned int 		line;	   /* Horizontal line for interrupt */
	unsigned int		latency;   /* Allowable delay after "line" */
	struct _crime_hli_event *next;	   /* For linked list of events */
} crime_hli_event_t;

crime_hli_id_t crime_hli_sched(u_int,  void (*fn)(), void *, u_int, u_int,
							crime_hli_flags_t );
int crime_hli_unsched( u_int, crime_hli_id_t );
int crime_hli_update( u_int, crime_hli_id_t, u_int, int, crime_hli_flags_t );

/*
 * Miscellaneous graphics - video interface.
 */
#define	CRIME_INTERFACE_REVISION	0

typedef struct crime_vinfo {
	unsigned char vi_boardrev;	/* crime board revision */
	unsigned char vi_crimerev;	/* crime chip revision */
	unsigned char vi_gberev;	/* gbe chip revision */
	unsigned char vi_fieldrate;	/* graphics video timing rate */
	short vi_width;			/* display width in pixels */
	short vi_height;		/* display height in pixels */
	short vi_flags;			/* see below */
} crime_vinfo_t;

	/* crime_vinfo_t.vi_flags */

/* If set, graphics is in graphics mode, else textport mode */
#define CRIME_VI_GFXMODE 1

/* If set, current video timing is stereo */
#define CRIME_VI_STEREO  2


int crime_present( int interface_revision );
int crime_video_info( int boardnum, crime_vinfo_t *vinfo );
int crime_resync_sched( int boardnum,
	void (*func)( void *arg, int reason ), void *arg );

	/* Values for crime_resync_sched 'reason' */

/* GBE is about to shut down for video timing change */
#define	CRIME_GBE_STOP		1

/* GBE video timing init completed */
#define	CRIME_GBE_START		2

typedef void (*crm_callback_t)(void *);
int crime_retrace_callback(int bd, crm_callback_t cb, void *arg);

#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */



#endif /* _SYS_CRIME_H__ */
