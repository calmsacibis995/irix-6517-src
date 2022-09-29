/*
	niblet.h - 

		Definitions used by the niblet kernel

*/

/* If you change this stuff, you need to recompile cob too! */

/* #define R4000	1 */ /* MARGIE changed */
#ifdef R4000
#define LW lw
#define SW sw
#define ADD add
#define ADDI addi
#define ADDIU addiu
#define ADDU addu
#define SRL srl
#define SLL sll
#define LL ll
#define SC sc
#define SUB sub
#define SUBU subu
#define WORDSZ_SHIFT 2
#define SR_COUNTS_BIT SR_IBIT8
#define CAUSE_COUNTS_BIT CAUSE_IP8
#define ADDRSPACE_RGN_UNMAPPED_CACHED   0x80000000
#define ADDRSPACE_RGN_UNMAPPED_UNCACHED 0xa0000000
#define ADDRSPACE_ADDRESS               0x1fffffff
#define CLEAR_INTERRUPTS                0xb
#elif TFP
/* LA, LI, MFC0, MTC0, PTR_WORD defined in asm.h */
#define LW ld
#define SW sd
#define ADD dadd
#define ADDI daddi
#define ADDIU daddiu
#define ADDU daddu
#define SRL dsrl
#define SLL dsll
#define LL lld
#define SC scd
#define SUB dsub
#define SUBU dsubu
#define WORDSZ_SHIFT 3
#define SR_COUNTS_BIT SR_IBIT11
#define CAUSE_COUNTS_BIT CAUSE_IP11
#define SR_KSUMASK SR_KU
#define C0_ENHI C0_TLBHI
#define C0_ENLO C0_TLBLO
#define C0_INX C0_TLBSET
#define PFNSHIFT TLBLO_PFNSHIFT
#define SR_USER SR_KU

/* 
   These constants are constants that are used by one name in
   the code, but defined but another (but similar) name in the tfp.h
   file.  
*/
#define TLBLO_VMASK TLBLO_V
#define TLBLO_DMASK TLBLO_D
#define TLBLO_CSHIFT TLBLO_CACHSHIFT

/*
#define TLBLO_CSHIFT ENTRYLO_CSHIFT
#define	TLBLO_PFNMASK	       ENTRYLO_PFNMASK
#define	TLBLO_DMASK	           ENTRYLO_DMASK
#define	TLBLO_VMASK	           ENTRYLO_VMASK
*/
#define ADDRSPACE_RGN_UNMAPPED_CACHED   0xa800000000000000
#define ADDRSPACE_RGN_UNMAPPED_UNCACHED 0x9000000000000000
#define ADDRSPACE_ADDRESS               0xffffffffff
#define CLEAR_INTERRUPTS                0x6
#endif

#ifdef RAW_NIBLET
/* #define NIBLET_INFO_ADDR 0xa800000000006000; */
#define NIBLET_RAWDATA_ADDR 0xa800000000111000
#endif

#ifdef RAW_NIBLET
#define GET_STARTUP_DATA_LOC(reg) \
	LI	reg, NIBLET_RAWDATA_ADDR
#else
#define GET_STARTUP_DATA_LOC(reg) \
	LA reg, nib_exc_start 
#endif

/* Dcob copies the handlers to this location.  Niblet.s puts this address
   into UBASE */
#ifdef RAW_NIBLET
#endif

#define TLB_REFILL_HANDLER_LOC 0xa800000000009000
#define TLB_REFILL_HANDLER_LOC_UNCACHED 0x9000000000009000


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
/*R4000 counts from 0 to QUANTUM, then takes an interrupt.
TFP countes from QUANTUM up to 0x7ffffff then takes an interrupt
*/
#ifdef R4000
#define QUANTUM	5000000
#elif TFP
#ifdef QUANTUM_SHORT
/* From 0x7ffff000 to 0x80000000 is the same as from 0d0 to 0d255 */
#define QUANTUM	0x7fffff00
#else
/* From 0x7ff85edf to 0x80000000 is the same as from 0d0 to 0d500000 */
#define QUANTUM	0x7ff85edf
#endif
#endif
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

/* MARGIE FIX  - added in SR_IBIT3|SR_IBIT5 for Uart problem */
/* Also added in SR_XX and SR_UX */
/* #define NIB_SR		((SR_IMASK & ~(SR_IBIT6|SR_IBIT4|SR_IBIT1|SR_IBIT2))|SR_IE|SR_EXL|SR_USER) */
#ifdef NO_PREEMPTION
/* #define NIB_SR		((SR_IMASK & ~(SR_COUNTS_BIT|SR_IBIT6|SR_IBIT4|SR_IBIT1|SR_IBIT2|SR_IBIT3|SR_IBIT5))|SR_IE|SR_EXL|SR_USER|SR_UX|SR_XX) */
#define NIB_SR		((SR_IMASK & ~(SR_COUNTS_BIT|SR_IBIT6|SR_IBIT1|SR_IBIT2))|SR_IE|SR_EXL|SR_USER|SR_UX|SR_XX)
#else
/* removed ibit4, ibit3, ibit5 */
#define NIB_SR		((SR_IMASK & ~(SR_IBIT6|SR_IBIT1|SR_IBIT2))|SR_IE|SR_EXL|SR_USER|SR_UX|SR_XX)
#endif

#endif /* TIMEOUT */
#define VA2PG(a)	(((a) & ADDRSPACE_ADDRESS) >> PGSZ_BITS)

/* Where's the globally shared memory? */
#define SHARED_VADDR	0x300000U
#define SHARED_LENGTH	8U * PG_SIZE

/* Where's the page table? */
#ifdef R4000
#define PTEBASE		0xff800000	/* TLBCTXT_BASEMASK */
#elif TFP
/* In TFP, is this an appropriate address to access the pt from? FIX??? */
/* #define PTEBASE		0xc0000000ff800000 */	/* TLBCTXT_BASEMASK */
#define PTEBASE		0x40000000ff800000	/* TLBCTXT_BASEMASK */
#endif


/* Number of wired TLB entries */
#define WIRED_TLB		16	/* Must be a power of two */
#define WIRED_TLB_MASK		WIRED_TLB - 1

/* Assembly to C (and back) macros for tables */
/* MARGIE changed for makeing proc table all 64 bit entries */
#define OFF_TO_IND(x)	(x)>>3
#define IND_TO_OFF(x)	(x)<<3

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
#define P_R1		0x08
#define P_R2		0x10
#define P_R3		0x18
#define P_R4		0x20
#define P_R5		0x28
#define P_R6		0x30
#define P_R7		0x38
#define P_R8		0x40
#define P_R9		0x48
#define P_R10		0x50
#define P_R11		0x58
#define P_R12		0x60
#define P_R13		0x68
#define P_R14		0x70
#define P_R15		0x78
#define P_R16		0x80
#define P_R17		0x88
#define P_R18		0x90
#define P_R19		0x98
#define P_R20		0xa0
#define P_R21		0xa8
#define P_R22		0xb0
#define P_R23		0xb8
#define P_R24		0xc0
#define P_R25		0xc8
#define P_R26		0xd0
#define P_R27		0xd8
#define P_R28		0xe0
#define P_R29		0xe8
#define P_R30		0xf0
#define P_R31		0xf8
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
/* MARGIE changed all these to 64 bits */
#define U_OUTENHI	0x110
#define U_INENHI	0x118
#define U_STAT		0x120
#define U_PID		0x128
#define U_XA		0x130
#define U_PGTBL		0x138
#define U_QUANT		0x140
#define U_NEXT		0x148
#define U_MYADR 	0x150
#define U_PTEXT		0x158
#define U_PDATA		0x160
#define U_PBSS		0x168
#define U_VTEXT		0x170
#define U_VDATA		0x178
#define U_VBSS		0x180
#define U_TSIZE		0x188
#define U_DSIZE		0x190
#define U_BSIZE		0x198
#define U_MAGIC1	0x1a0
#define U_MAGIC2	0x1a8
#define U_SHARED	0x1b0
#define U_EXCCOUNT	0x1b8
#define U_ERRCOUNT	0x1c0
#define U_EXCTBLBASE	0x1c8
/* This puppy's 32 * 4 bytes long. */  /* MARGIE changed to 32 * 8 bytes long */
/* The next entry should be at U_EXCTBLBASE (0x1c8) + 0x100  = 0x2c8 */
#define U_EXCCNTBASE	0x2c8
/* This puppy's 32 * 4 bytes long. */ /* MARGIE changed to 32 * 8 bytes long */
/* The next entry should be at U_EXCRCNTBASE (0x2c8) + 0x100 = 0x3c8 */
#define U_EXCENHIBASE	0x3c8
/* This puppy's 32 * 4 bytes long. */ /* MARGIE changed to 32 * 8 bytes long */
/* The next entry should be at U_EXCENHIBASE (0x3c8) + 0x100 = 0x4c8 */
#define U_ERRTBLBASE	0x4c8
#define U_ERRCNTBASE	0x4d0
#define U_PGTBLVEND	0x4d8
#define U_SIZE		0x4e0 /* The size needs to be a doubleword multiple */
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

/* MARGIE made these all double words.  They used to be single words,
   but STUP_PROCTBL, at least, must be a double word because it is
   a 64 bit address in TFP, so it is easier to just make them all doubles
   Corresponding to this change, in dcob.c I change the store_words()
   used to storing to this save area into store_double_lo().
 */
#define STUP_PROCTBL	0
#define STUP_TIMEOUT	8
#define STUP_NCPUS	16
#define STUP_WHY	24
#define STUP_DIRECTORY	32
#define STUP_WHO	40
#define STUP_PID	48


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
	LW	r20, U_STAT(_base); 		\
	ld	r3, U_R3(_base); 		\
	ld	r4, U_R4(_base); 		\
	ld	r5, U_R5(_base); 		\
	ld	r6, U_R6(_base); 		\
	ld	r7, U_R7(_base); 		\
	MTC0	(r20, C0_SR);			\
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
	ld	r29, U_R29(_base); 		\
	nop


/*
	Margie removed these from restore rest because they are used for debug to mem
	ld	r28, U_R28(_base); 		\
	ld	r30, U_R30(_base); 		\
*/

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
	MFC0	(r2, C0_SR);			\
	sd	r30, U_R30(_base); 		\
	sd	r2, U_STAT(_base);		\
	nop

/*
	Margie removed these from restore rest because they are used for debug to mem
	sd	r28, U_R28(_base); 		\
	sd	r29, U_R29(_base); 		\
*/

#define SAVE_ALL(_reg)				\
	SAVE_T(_reg);				\
	SAVE_REST(_reg)

#define SAVE_PRIVATE_KERNEL_REGS(_base) \
	sd	r8, P_R8(_base); 				\
	sd	r9, P_R9(_base); 				\
	sd	r10, P_R10(_base); 				\
	sd	r11, P_R11(_base); 				\
	sd	r12, P_R12(_base); 				\
	sd	r13, P_R13(_base); 				\
	sd	r14, P_R14(_base); 				\
	sd	r31, P_R31(_base);				\
	sd	r1,  P_R1(_base);				\

#define SAVE_PRIVATE_NON_KERNEL_REGS(_base) 		\
	sd	r1, P_R1(_base);				\
	sd	r2, P_R2(_base); 				\
	sd	r3, P_R3(_base); 				\
	sd	r4, P_R4(_base); 				\
	sd	r5, P_R5(_base); 				\
	sd	r6, P_R6(_base); 				\
	sd	r7, P_R7(_base); 				\
	sd	r15, P_R15(_base); 				\
	sd	r16, P_R16(_base); 				\
	sd	r17, P_R17(_base); 				\
	sd	r18, P_R18(_base); 				\
	sd	r19, P_R19(_base); 				\
	sd	r20, P_R20(_base); 				\
	sd	r21, P_R21(_base); 				\
	sd	r22, P_R22(_base); 				\
	sd	r23, P_R23(_base); 				\
	sd	r24, P_R24(_base); 				\
	sd	r25, P_R25(_base); 				\
	sd	r28, P_R28(_base); 				\
	sd	r29, P_R29(_base); 				\
	sd	r30, P_R30(_base); 				\


#define SAVE_PRIVATE_ALL(_base)				\
	SAVE_PRIVATE_KERNEL_REGS(_base)			\
	SAVE_PRIVATE_NON_KERNEL_REGS(_base)	   



