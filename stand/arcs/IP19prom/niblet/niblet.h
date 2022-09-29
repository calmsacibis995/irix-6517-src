/*
	niblet.h - 

		Definitions used by the niblet kernel

*/

/* If you change this stuff, you need to recompile cob too! */

#define R4000	1

#define MAXCPUS		16
#define PROCS_PER_TEST	15	/* Can't have more than this number of processes*/

/* #define	SOFTLOCKS */
#define MP
#define	LOCKS
#define LOCKCHECK
#define	CACHED
/* #define	IN_CACHE */

#define CLEARTLB
#define M32OR64

/* Default time quantum for task switching */
#define QUANTUM	5000000
#define WAITTIME 30

#define BARRIER_TIMEOUT	0x10000000

/* Global junk */

/* Set up 4k pages */
#define PGSZ_BITS	12
#define PG_SIZE		4096
#define PGSZ_MASK	0	/* 12 bit pages are the smallest possible */
#define OFFSET_MASK	4095	/* 2^12 - 1 */

#define CONFIG_BITS	5 		/* Use "cacheable exclusive on write"
					 * for k0seg references.
				 	 */
#define TEXT_COHERENCY	5	/* Text defaults to noncoherent */
#define DATA_COHERENCY	5	/* Data defaults to read exclusive */

#define PG_FLAGS	TLBLO_VMASK | TLBLO_DMASK
			/* Valid, Dirty */
#ifdef TIMEOUT
#define NIB_SR		((SR_IMASK & ~(SR_IBIT6|SR_IBIT1|SR_IBIT2))|SR_IE|SR_EXL|SR_USER)
#else
#define NIB_SR		((SR_IMASK & ~(SR_IBIT6|SR_IBIT4|SR_IBIT1|SR_IBIT2))|SR_IE|SR_EXL|SR_USER)
#endif /* TIMEOUT */
#define VA2PG(a)	(((a) & 0x1fffffff) >> PGSZ_BITS)

/* Where's the globally shared memory? */
#define SHARED_VADDR	0x300000U
#define SHARED_LENGTH	8U * PG_SIZE

/* Where's the page table? */
#define PTEBASE		0xff800000	/* TLBCTXT_BASEMASK */

/* Number of wired TLB entries */
#define WIRED_TLB		16	/* Must be a power of two */
#define WIRED_TLB_MASK		WIRED_TLB - 1

/* Assembly to C (and back) macros for tables */
#define OFF_TO_IND(x)	(x)>>2
#define IND_TO_OFF(x)	(x)<<2

#define MAGIC1	0x12345678
#define MAGIC2	0x0fedcba9

/* Per CPU slot/slice addressed directory defines. */
/* Four doublewords per entry. */
#define DIR_ENT_SIZE	16
#define DIR_SHIFT	4	/* log2(DIR_ENT_SIZE) */

#define DIR_PRIVOFF	0	/* Offset of private pointer in dir ent */
#define DIR_BAROFF	8	/* Offset of "I'm here marker" */


/* Private data area defines: */
#define P_REGSAVE	0x00
#define P_NIBLET_AREA	0x100
#define P_RA		0x100
#define P_SR		0x108
#define P_SP		0x110
#define P_ICNT		0x118
#define P_CURPROC	0x120
#define	P_RUNQ		0x128
#define	P_MVPC		0x130
#define	P_SCHCNT	0x138
#define	P_CPUNUM	0x140
#define P_PID		0x158
#define P_SAVEAREA	0x160
#define PRIV_SIZE	0x1c0

#define PRIVATE		t4
#define CURPROC		t5
#define MVPC		t6

/* Process Table Entry Definitions */
/* Leave room for 64 bits of each of these: */
#define U_PC		0x00
#define U_R1		0x08
#define U_R2		0x10
#define U_R3		0x18
#define U_R4		0x20
#define U_R5		0x28
#define U_R6		0x30
#define U_R7		0x38
#define U_R8		0x40
#define U_R9		0x48
#define U_R10		0x50
#define U_R11		0x58
#define U_R12		0x60
#define U_R13		0x68
#define U_R14		0x70
#define U_R15		0x78
#define U_R16		0x80
#define U_R17		0x88
#define U_R18		0x90
#define U_R19		0x98
#define U_R20		0xa0
#define U_R21		0xa8
#define U_R22		0xb0
#define U_R23		0xb8
#define U_R24		0xc0
#define U_R25		0xc8
#define U_R26		0xd0
#define U_R27		0xd8
#define U_R28		0xe0
#define U_R29		0xe8
#define U_R30		0xf0
#define U_R31		0xf8
#define U_CSIN		0x100
#define U_CSOUT		0x108
/* From here on down, everything's 32 bits */
#define U_OUTENHI	0x110
#define U_INENHI	0x114
#define U_STAT		0x118
#define U_PID		0x11c
#define U_XA		0x120
#define U_PGTBL		0x124
#define U_QUANT		0x128
#define U_NEXT		0x12c
#define U_MYADR 	0x130
#define U_PTEXT		0x134
#define U_PDATA		0x138
#define U_PBSS		0x13c
#define U_VTEXT		0x140
#define U_VDATA		0x144
#define U_VBSS		0x148
#define U_TSIZE		0x14c
#define U_DSIZE		0x150
#define U_BSIZE		0x154
#define U_MAGIC1	0x158
#define U_MAGIC2	0x15c
#define U_SHARED	0x160
#define U_EXCCOUNT	0x164
#define U_ERRCOUNT	0x168
#define U_EXCTBLBASE	0x16c
/* This puppy's 32 * 4 bytes long. */
/* The next entry should be at U_EXCTBLBASE (0x16c) + 0x80  = 0x1ec */
#define U_EXCCNTBASE	0x1ec
/* This puppy's 32 * 4 bytes long. */
/* The next entry should be at U_EXCRCNTBASE (0x1ec) + 0x80 = 0x26c */
#define U_EXCENHIBASE	0x26c
/* This puppy's 32 * 4 bytes long. */
/* The next entry should be at U_EXCENHIBASE (0x26c) + 0x80 = 0x2ec */
#define U_ERRTBLBASE	0x2ec
#define U_ERRCNTBASE	0x2f0
#define U_PGTBLVEND	0x2f4
#define U_SIZE		0x2f8 /* The size needs to be a doubleword multiple */
#define RQ_UPTR		0
#define RQ_NEXT		4
#define RQ_SLOTSIZE	8
#define RQ_SIZE		(RQ_SLOTSIZE * 16)	/* Max procs * size of entry */

/* Interrupt table offsets */
#define I_VECTOR	0
#define I_COUNT		32	/* 8 interrupts * 4 bytes */
#define I_ASID		64	/* I_COUNT + 8 interrupts * 4 bytes */
#define I_ENHI		96

/* Page Table Entry Definitions */
#define P_SIZE	16
#define P_VSTRT	0
#define P_VEND	4
#define P_PPAGE 8
#define P_FLAGS	12

/* Start-up block */
/* dcob stores information that niblet needs in the block of memory
 * Starting where niblet's exception handlers would go (nib_exc_start)
 * if we actually copied them there.
 */
/* Offsets into the startup block: */

#define STUP_PROCTBL	0
#define STUP_TIMEOUT	4
#define STUP_NCPUS	8
#define STUP_WHY	12
#define STUP_DIRECTORY	16
#define STUP_WHO	20
#define STUP_PID	24


#define RESTORE_T(_base);			\
	ld	r1, U_R1(_base); 		\
	ld	r8, U_R8(_base); 		\
	ld	r9, U_R9(_base); 		\
	ld	r10, U_R10(_base); 		\
	ld	r11, U_R11(_base); 		\
	ld	r12, U_R12(_base); 		\
	ld	r13, U_R13(_base); 		\
	ld	r14, U_R14(_base); 		\
	ld	r24, U_R24(_base); 		\
	ld	r25, U_R25(_base); 		\
	ld	r31, U_R31(_base); 		\
	ld	r15, U_R15(_base)

/* Skip k0/k1 (r26/r27) */
#define RESTORE_REST(_base) 			\
	ld	r2, U_R2(_base); 		\
	lw	r20, U_STAT(_base); 		\
	ld	r3, U_R3(_base); 		\
	ld	r4, U_R4(_base); 		\
	ld	r5, U_R5(_base); 		\
	ld	r6, U_R6(_base); 		\
	ld	r7, U_R7(_base); 		\
	mtc0	r20, C0_SR;			\
	nop;					\
	nop;					\
	nop;					\
	nop;					\
	ld	r16, U_R16(_base); 		\
	ld	r17, U_R17(_base); 		\
	ld	r18, U_R18(_base); 		\
	ld	r19, U_R19(_base); 		\
	ld	r20, U_R20(_base); 		\
	ld	r21, U_R21(_base); 		\
	ld	r22, U_R22(_base); 		\
	ld	r23, U_R23(_base); 		\
	ld	r28, U_R28(_base); 		\
	ld	r29, U_R29(_base); 		\
	ld	r30, U_R30(_base); 		\
	nop

#define RESTORE_ALL(_reg)			\
	RESTORE_T(_reg);			\
	RESTORE_REST(_reg)

#define SAVE_T(_base)				\
	sd	r8, U_R8(_base); 		\
	sd	r9, U_R9(_base); 		\
	sd	r10, U_R10(_base); 		\
	sd	r11, U_R11(_base); 		\
	sd	r12, U_R12(_base); 		\
	sd	r13, U_R13(_base); 		\
	sd	r14, U_R14(_base); 		\
	sd	r15, U_R15(_base); 		\
	sd	r24, U_R24(_base); 		\
	sd	r25, U_R25(_base); 		\
	sd	r31, U_R31(_base);		\
	sd	r1, U_R1(_base)

/* Skip k0/k1 (r26/r27) */
#define SAVE_REST(_base) 			\
	sd	r2, U_R2(_base); 		\
	sd	r3, U_R3(_base); 		\
	sd	r4, U_R4(_base); 		\
	sd	r5, U_R5(_base); 		\
	sd	r6, U_R6(_base); 		\
	sd	r7, U_R7(_base); 		\
	sd	r16, U_R16(_base); 		\
	sd	r17, U_R17(_base); 		\
	sd	r18, U_R18(_base); 		\
	sd	r19, U_R19(_base); 		\
	sd	r20, U_R20(_base); 		\
	sd	r21, U_R21(_base); 		\
	sd	r22, U_R22(_base); 		\
	sd	r23, U_R23(_base); 		\
	mfc0	r2, C0_SR;			\
	sd	r28, U_R28(_base); 		\
	sd	r29, U_R29(_base); 		\
	sd	r30, U_R30(_base); 		\
	sw	r2, U_STAT(_base);		\
	nop

#define SAVE_ALL(_reg)				\
	SAVE_T(_reg);				\
	SAVE_REST(_reg)

