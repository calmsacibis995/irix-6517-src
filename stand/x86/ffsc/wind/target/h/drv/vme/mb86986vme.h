/* mb86986Vme.h - Fujitsu MB86986 MBus to VMEbus Interface Controller header */

/* Copyright 1994-1995 Wind River Systems, Inc. */

/* Copyright 1994-1995 FORCE COMPUTERS */

/*
modification history
--------------------
01c,15mar95,vin  integrated with 5.2.
01b,11apr94,lsr  suppressed the MVIC_MBUS_ID define to be sure that
		 the ID is somewhere defined.
01a,01jun93,tkj  original MVIC release (Derived from vbic.h 01e).
*/

/*
This header file defines the register layout of the Fujitsu MB86986
MBus to VMEbus Interface Controller chip (MVIC) and supplies alpha-numeric
equivalences for register bit definitions.
*/

/* _PFN = 24 bit Page frame number = Physical address >> PAGE_SHIFT */
/* This is needed because the full physical address is 36 bits. */

/* _OFS = Offset from preceeding _PFN entry. */
/* This can be used to compute both the virtual and physical addresses. */

#ifndef __INCmb86986Vmeh
#define __INCmb86986Vmeh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE

/* typedefs */

typedef struct  /* MVICInit - MVIC chip initialization data for mvicInit() */
    {
    FUNCPTR	mvicNmiRoutine;		/* MVIC A16 NMI: Interrupt handler */
    int		mvicNmiArg;		/* Argument for above */
    FUNCPTR	mvicDdRoutine;		/* MVIC DMA Done Int: Int. Handler */
    int		mvicDdArg;		/* Argument for above */
    FUNCPTR	mvicDeRoutine;		/* MVIC DMA Error Int: Int. Handler */
    int		mvicDeArg;		/* Argument for above */
    FUNCPTR	mvicSeRoutine;		/* MVIC Slave Error Int: Int. Handler*/
    int		mvicSeArg;		/* Argument for above */
    FUNCPTR	mvicMeRoutine;		/*MVIC Master Error Int: Int. Handler*/
    int		mvicMeArg;		/* Argument for above */
    UINT	mvicMbar;		/* MBAR - MBus Base Address Register */
    UINT	mvicVmcr;		/* VMCR - VMEbus Master Configuration*/
    UINT	mvicVscr;		/* VSCR - VMEbus Slave Configuration */
    UINT	mvicVasmbar;		/* VASMBAR - VMEbus A16 Slave */
    UINT	mvicVa16rnr;		/* VA16RNR - VMEbus A16 Reset & NMI */
    void *	mvicVrtBase;		/* First virtual address of A24 VDMA */
    void *	mvicVrtTop;		/* Last virtual address of A24 VDMA */
    } MVICInit;

#endif	/* _ASMLANGUAGE */

/******************************************************************************
*
* Control register definitions
*/

					/* Add MVIC's MBus ID to a _PFN */
#define MVIC_PFN_WITH_MID(pfn)		((0x01000*MVIC_MBUS_ID)+(pfn))

#define	MVIC_PFN	MVIC_PFN_WITH_MID(0xff0fff) /* Chip Physical Address */

#define	MVIC_MIR_OFS	0xffc	/* MBus ID Register */
#define	MVIC_MBAR_OFS	0xff8	/* MBUS Base Address Register */

#define	MVIC_VMCR_OFS	0xff0	/* VMEbus Master Configuration Register */
#define	MVIC_VSCR_OFS	0xfe8	/* VMEbus Slave Configuration Register */
#define	MVIC_VASMBAR_OFS 0xfe0	/*VMEbus A16 Slave MBus Base Address Register*/
#define	MVIC_VA16RNR_OFS 0xfd8	/* VMEbus A16 Reset & NMI Register */
#define	MVIC_DCSR_OFS	0xfd0	/* DMA Control Shadow Register */
#define	MVIC_DMRSASR_OFS 0xfc8	/* DMA MBus Read Starting Address Register */
#define	MVIC_DMWSASR_OFS 0xfc0	/* DMA MBus Write Starting Address Register */
#define	MVIC_DNCBPR_OFS	0xfb8	/* DMA Next Command Block Pointer Register */
#define	MVIC_ISR_OFS	0xfb0	/* MVIC Interrupt Status Register */
#define	MVIC_SENDI_OFS	0xfa0	/* VMEbus Send Interrupt Register */
#define	MVIC_VL7IAR_OFS	0xf7c	/* VMEbus Level 7 Int. Acknowledge Register */
#define	MVIC_VL6IAR_OFS	0xf78	/* VMEbus Level 6 Int. Acknowledge Register */
#define	MVIC_VL5IAR_OFS	0xf74	/* VMEbus Level 5 Int. Acknowledge Register */
#define	MVIC_VL4IAR_OFS	0xf70	/* VMEbus Level 4 Int. Acknowledge Register */
#define	MVIC_VL3IAR_OFS	0xf6c	/* VMEbus Level 3 Int. Acknowledge Register */
#define	MVIC_VL2IAR_OFS	0xf68	/* VMEbus Level 2 Int. Acknowledge Register */
#define	MVIC_VL1IAR_OFS	0xf64	/* VMEbus Level 1 Int. Acknowledge Register */
#define	MVIC_VTBAR_OFS	0x000	/* VDMA Translation Base Address Register */

/******************************************************************************
* Control register field definitions
*/

/*
* MVIC_MIR -- MBus ID Register
*/

#define	MVIC_MIR	(*(UINT *) (MMU_MVIC_VA+MVIC_MIR_OFS))

/* MVIC_MIR_PART -- Part number */
#define	MVIC_MIR_PART_MASK	0xffff0000
#define	MVIC_MIR_PART_SHIFT	16

/* MVIC_MIR_DEV -- Device number */
#define	MVIC_MIR_DEV_MASK	0x0000ff00
#define	MVIC_MIR_DEV_SHIFT	8

/* MVIC_MIR_REV -- Revision */
#define	MVIC_MIR_REV_MASK	0x000000f0
#define	MVIC_MIR_REV_SHIFT	4

/* MVIC_MIR_VEN -- Vendor */
#define	MVIC_MIR_VEN_MASK	0x0000000f
#define	MVIC_MIR_VEN_SHIFT	0

/*
* MVIC_MBAR -- MBus Base Address Register
*/

#define	MVIC_MBAR	(*(UINT *) (MMU_MVIC_VA+MVIC_MBAR_OFS))

/* MVIC_MBAR_NVMSBA -- (MBus -> VME) Normal VME MBus Slave Base Address */
#define	MVIC_MBAR_NVMSBA_MASK		0xf0000000
#define	MVIC_MBAR_NVMSBA_SHIFT		28

/* MVIC_MBAR_BVMSBA -- (MBus -> VME) Block mode VME MBus Slave Base Address */
#define	MVIC_MBAR_BVMSBA_MASK		0x0f000000
#define	MVIC_MBAR_BVMSBA_SHIFT		24

/* MVIC_MBAR_UVMSBA --
   (MBus -> VME) User defined addr modifier VME MBus Slave Base Address */
#define	MVIC_MBAR_UVMSBA_MASK		0x00f00000
#define	MVIC_MBAR_UVMSBA_SHIFT		20

/* MVIC_MBAR_MMBA -- (A32, A24 VME -> MBus) MBus Master Base Address */
#define	MVIC_MBAR_MMBA_MASK		0x0000fff0
#define	MVIC_MBAR_MMBA_SHIFT		4

/* MVIC_MBAR_TT -- Timer Test */
#define	MVIC_MBAR_TT_MASK		0x00000008
#define	MVIC_MBAR_TT_ENABLE		0x00000008
#define	MVIC_MBAR_TT_ENABLE_NOT		0
#define	MVIC_MBAR_TT_DISABLE		MVIC_MBAR_TT_ENABLE_NOT

/* MVIC_MBAR_LB -- Loopback */
#define	MVIC_MBAR_LB_MASK		0x00000004
#define	MVIC_MBAR_LB_ENABLE		0x00000004
#define	MVIC_MBAR_LB_ENABLE_NOT		0
#define	MVIC_MBAR_LB_DISABLE		MVIC_MBAR_LB_ENABLE_NOT

/* MVIC_MBAR_MR -- (MBus -> VME) MBus Retry Period */
#define	MVIC_MBAR_MR_MASK		0x00000003	/* Time @ 33 1/3 MHz */
#define	MVIC_MBAR_MR_1024		0x00000000	/* 30.7 usec */
#define	MVIC_MBAR_MR_256		0x00000001	/*  7.7 usec */
#define	MVIC_MBAR_MR_64			0x00000002	/*  1.9 usec */
#define	MVIC_MBAR_MR_NONE		0x00000003	/* No time out */

/*
* MVIC_VMCR -- VMEbus Master Configuration Register -- Read/Write
*/

#define	MVIC_VMCR	(*(UINT *) (MMU_MVIC_VA+MVIC_VMCR_OFS))

/* MVIC_VMCR_VMSA32ASE --
   (A32 VME -> MBus) VME MBus Slave A32 Address space enable */
#define	MVIC_VMCR_VMSA32ASE_MASK	0x80000000
#define	MVIC_VMCR_VMSA32ASE_ENABLE	0x80000000
#define	MVIC_VMCR_VMSA32ASE_ENABLE_NOT	0
#define	MVIC_VMCR_VMSA32ASE_DISABLE	MVIC_VMCR_VMSA32ASE_ENABLE_NOT

/* MVIC_VMCR_VMSA24ASE --
   (A24 VME -> MBus) VME MBus Slave A24 Address space enable */
#define	MVIC_VMCR_VMSA24ASE_MASK	0x40000000
#define	MVIC_VMCR_VMSA24ASE_ENABLE	0x40000000
#define	MVIC_VMCR_VMSA24ASE_ENABLE_NOT	0
#define	MVIC_VMCR_VMSA24ASE_DISABLE	MVIC_VMCR_VMSA24ASE_ENABLE_NOT

/* MVIC_VMCR_VMSA16ASE --
   (A16 VME -> MBus) VME MBus Slave A16 Address space enable */
#define	MVIC_VMCR_VMSA16ASE_MASK	0x20000000
#define	MVIC_VMCR_VMSA16ASE_ENABLE	0x20000000
#define	MVIC_VMCR_VMSA16ASE_ENABLE_NOT	0
#define	MVIC_VMCR_VMSA16ASE_DISABLE	MVIC_VMCR_VMSA16ASE_ENABLE_NOT

/* MVIC_VMCR_A32UAM -- (MBus -> A32 VME) A32 User defined Address Modifier */
#define	MVIC_VMCR_A32UAM_MASK		0x0f000000
#define	MVIC_VMCR_A32UAM_SHIFT		24

/* MVIC_VMCR_A24UAM -- (MBus -> A24 VME) A24 User defined Address Modifier */
#define	MVIC_VMCR_A24UAM_MASK		0x00f00000
#define	MVIC_VMCR_A24UAM_SHIFT		20

/* MVIC_VMCR_A16UAM -- (MBus -> A16 VME) A16 User defined Address Modifier */
#define	MVIC_VMCR_A16UAM_MASK		0x000f0000
#define	MVIC_VMCR_A16UAM_SHIFT		16

/* MVIC_VMCR_DVMA -- (A24 VME -> MBus) A24 DVMA Base Address */
#define	MVIC_VMCR_DVMA_MASK		0x00007800
#define	MVIC_VMCR_DVMA_SHIFT		11

/* MVIC_VMCR_IRL -- (-> VME) Interrupt Request Level */
#define	MVIC_VMCR_IRL_MASK		0x00000700
#define	MVIC_VMCR_IRL_SHIFT		8

/* MVIC_VMCR_DT -- (MBus -> VME) VMEbus DTACK Time-out */
#define	MVIC_VMCR_DT_MASK		0x000000c0	/* Time @ 33 1/3 MHz */
#define	MVIC_VMCR_DT_2048		0x00000000	/* 61.4 usec */
#define	MVIC_VMCR_DT_512		0x00000040	/* 15.4 usec */
#define	MVIC_VMCR_DT_128		0x00000080	/*  3.8 usec */
#define	MVIC_VMCR_DT_NONE		0x000000c0	/* No time out */

/* MVIC_VMCR_BRT -- (MBus -> VME) VMEbus Bus Request Time-out */
#define	MVIC_VMCR_BRT_MASK		0x00000030	/* Time @ 33 1/3 MHz */
#define	MVIC_VMCR_BRT_64K		0x00000000	/* 1,966.1 usec */
#define	MVIC_VMCR_BRT_16K		0x00000010	/*   491.5 usec */
#define	MVIC_VMCR_BRT_512		0x00000020	/*    12.8 usec */
#define	MVIC_VMCR_BRT_NONE		0x00000030	/* No time out */

/* MVIC_VMCR_BRL -- (MBus -> VME) Bus Request Level */
#define	MVIC_VMCR_BRL_MASK		0x0000000c
#define	MVIC_VMCR_BRL_SHIFT		2

/* MVIC_VMCR_VAM -- VMEbus Arbiter Mode */
#define	MVIC_VMCR_VAM_MASK		0x00000002
#define	MVIC_VMCR_VAM_ROUND_ROBIN	0x00000002
#define	MVIC_VMCR_VAM_FIXED		0

/* MVIC_VMCR_S1E -- Slot 1 Enable */
#define	MVIC_VMCR_S1E_MASK		0x00000001
#define	MVIC_VMCR_S1E_ENABLE		0x00000001
#define	MVIC_VMCR_S1E_ENABLE_NOT	0
#define	MVIC_VMCR_S1E_DISABLE		MVIC_VMCR_S1E_ENABLE_NOT

/*
* MVIC_VSCR -- VMEbus Slave Configuration Register -- Read/Write
*/

#define	MVIC_VSCR	(*(UINT *) (MMU_MVIC_VA+MVIC_VSCR_OFS))

/* MVIC_VSCR_WSZ -- (A32 VME -> MBus) A32 VME Slave Window Size */
#define	MVIC_VSCR_WSZ_MASK		0x0f000000
#define	MVIC_VSCR_WSZ_16M		0x00000000
#define	MVIC_VSCR_WSZ_32M		0x01000000
#define	MVIC_VSCR_WSZ_64M		0x02000000
#define	MVIC_VSCR_WSZ_128M		0x03000000
#define	MVIC_VSCR_WSZ_256M		0x04000000
#define	MVIC_VSCR_WSZ_512M		0x05000000
#define	MVIC_VSCR_WSZ_1024M		0x06000000
#define	MVIC_VSCR_WSZ_2048M		0x07000000
#define	MVIC_VSCR_WSZ_4096M		0x08000000
#define	MVIC_VSCR_WSZ_SHIFT		24

/* MVIC_VSCR_WST -- (A32 VME -> MBus) A32 VME Slave Window Start */
#define	MVIC_VSCR_WST_MASK		0x00ff0000
#define	MVIC_VSCR_WST_SHIFT		16

/* MVIC_VSCR_SBA -- (A16 VME -> MBus) A16 Slave VME Base Address */
#define	MVIC_VSCR_SBA_MASK		0x0000ffff
#define	MVIC_VSCR_SBA_SHIFT		0

/*
* MVIC_VASMBAR --
  (A16 VME -> MBus) VMEbus A16 Slave MBus Base Address Register
*/

#define	MVIC_VASMBAR	(*(UINT *) (MMU_MVIC_VA + MVIC_VASMBAR_OFS))

/*
* MVIC_VA16RNR --
  (A16 VME -> MBus) VMEbus A16 Reset & NMI Register -- Read/Write
*/

#define	MVIC_VA16RNR	(*(UINT *) (MMU_MVIC_VA + MVIC_VA16RNR_OFS))

/* Reset */
/* MVIC_VA16RNR_VA16SRA -- (A16 VME ->) VMEbus A16 Slave Reset Address */
#define	MVIC_VA16RNR_VA16SRA_MASK	0xfffe0000
#define	MVIC_VA16RNR_VA16SRA_SHIFT	17

/* MVIC_VA16RNR_A16SRE -- (A16 VME ->) A16 Reset Enable */
#define	MVIC_VA16RNR_A16RE_MASK		0x00010000
#define	MVIC_VA16RNR_A16RE_ENABLE	0x00010000
#define	MVIC_VA16RNR_A16RE_ENABLE_NOT	0
#define	MVIC_VA16RNR_A16RE_DISABLE	MVIC_VA16RNR_A16RE_ENABLE_NOT

/* MVIC_VA16RNR_VA16SR --
   (A16 VME ->) VMEbus A16 Slave Reset Address & Enable (Composite) */
#define	MVIC_VA16RNR_VA16SR_MASK	0xffff0000
#define	MVIC_VA16RNR_VA16SR_SHIFT	16

/* NMI */
/* MVIC_VA16RNR_VA16SNA -- (A16 VME ->) VMEbus A16 Slave NMI Address */
#define	MVIC_VA16RNR_VA16SNA_MASK	0x00000fffe
#define	MVIC_VA16RNR_VA16SNA_SHIFT	1

/* MVIC_VA16RNR_A16NE -- (A16 VME ->) A16 NMI Enable */
#define	MVIC_VA16RNR_A16NE_MASK		0x00000001
#define	MVIC_VA16RNR_A16NE_ENABLE	0x00000001
#define	MVIC_VA16RNR_A16NE_ENABLE_NOT	0
#define	MVIC_VA16RNR_A16NE_DISABLE	MVIC_VA16RNR_A16NE_ENABLE_NOT

/* MVIC_VA16RNR_VA16SN --
   (A16 VME ->) VMEbus A16 Slave NMI Address & Enable (Composite) */
#define	MVIC_VA16RNR_VA16SN_MASK	0x0000ffff
#define	MVIC_VA16RNR_VA16SN_SHIFT	0

/*
* MVIC_ISR -- MVIC Interrupt Status Register -- Read only
*/

#define	MVIC_ISR	(*(UINT *) (MMU_MVIC_VA + MVIC_ISR_OFS))

/* MVIC_ISR_DB -- DMA Busy */
#define	MVIC_ISR_DB_MASK		0x80000000
#define	MVIC_ISR_DB_BUSY		0x80000000
#define	MVIC_ISR_DB_IDLE		0

/* MVIC_ISR_A16N -- (A16 VME -> MBus) A16 NMI (Used as mailbox interrupt) */
#define	MVIC_ISR_A16N_MASK		0x00000010
#define	MVIC_ISR_A16N_SET		0x00000010
#define	MVIC_ISR_A16N_CLEARED		0

/* MVIC_ISR_DD -- DMA Done */
#define	MVIC_ISR_DD_MASK		0x00000008
#define	MVIC_ISR_DD_SET			0x00000008
#define	MVIC_ISR_DD_CLEARED		0

/* MVIC_ISR_DE -- DMA Error */
#define	MVIC_ISR_DE_MASK		0x00000004
#define	MVIC_ISR_DE_SET			0x00000004
#define	MVIC_ISR_DE_CLEARED		0

/* MVIC_ISR_SE -- (VME -> MBus) Slave Error */
#define	MVIC_ISR_SE_MASK		0x00000002
#define	MVIC_ISR_SE_SET			0x00000002
#define	MVIC_ISR_SE_CLEARED		0

/* MVIC_ISR_ME -- (MBus -> VME) Master Error */
#define	MVIC_ISR_ME_MASK		0x00000001
#define	MVIC_ISR_ME_SET			0x00000001
#define	MVIC_ISR_ME_CLEARED		0

/*
* MVIC_SENDI -- (-> VME) VMEbus Send Interrupt Register -- Read/Write
*/

#define	MVIC_SENDI	(*(UINT *) (MMU_MVIC_VA + MVIC_SENDI_OFS))

/* MVIC_SENDI_ISTAT -- Interrupt status */
#define	MVIC_SENDI_ISTAT_MASK		0x80000000
#define	MVIC_SENDI_ISTAT_ENABLE		0x80000000
#define	MVIC_SENDI_ISTAT_ENABLE_NOT	0
#define	MVIC_SENDI_ISTAT_DISABLE	MVIC_SENDI_ISTAT_ENABLE_NOT

/* MVIC_SENDI_IVEC -- Interrupt vector */
#define	MVIC_SENDI_IVEC_MASK		0x000000ff
#define	MVIC_SENDI_IVEC_SHIFT		0

/*
* MVIC_VL1IAR to MVIC_VL7IAR --
  (<- VME) VMEbus Level xxx Interrupt Acknowledge Register
*/

#define	MVIC_VL7IAR_VA	(MMU_MVIC_VA + MVIC_VL7IAR_OFS)
#define	MVIC_VL6IAR_VA	(MMU_MVIC_VA + MVIC_VL6IAR_OFS)
#define	MVIC_VL5IAR_VA	(MMU_MVIC_VA + MVIC_VL5IAR_OFS)
#define	MVIC_VL4IAR_VA	(MMU_MVIC_VA + MVIC_VL4IAR_OFS)
#define	MVIC_VL3IAR_VA	(MMU_MVIC_VA + MVIC_VL3IAR_OFS)
#define	MVIC_VL2IAR_VA	(MMU_MVIC_VA + MVIC_VL2IAR_OFS)
#define	MVIC_VL1IAR_VA	(MMU_MVIC_VA + MVIC_VL1IAR_OFS)

#define	MVIC_VL7IAR	((int *) MVIC_VL7IAR_VA)
#define	MVIC_VL6IAR	((int *) MVIC_VL6IAR_VA)
#define	MVIC_VL5IAR	((int *) MVIC_VL5IAR_VA)
#define	MVIC_VL4IAR	((int *) MVIC_VL4IAR_VA)
#define	MVIC_VL3IAR	((int *) MVIC_VL3IAR_VA)
#define	MVIC_VL2IAR	((int *) MVIC_VL2IAR_VA)
#define	MVIC_VL1IAR	((int *) MVIC_VL1IAR_VA)

/*
* MVIC_VTBAR -- (VME -> MBus) 256 x VDMA Translation Base Address Register
*/

#define	MVIC_VTBAR	((UINT *) (MMU_MVIC_VA+MVIC_VTBAR_OFS))

#define	MVIC_VTBAR_VALID_MASK		0x00020000
#define	MVIC_VTBAR_VALID_VALID		0x00020000
#define	MVIC_VTBAR_VALID_VALID_NOT	0
#define	MVIC_VTBAR_VALID_INVALID	MVIC_VTBAR_VALID_VALID_NOT

#define	MVIC_VTBAR_VDMA_MASK		0x0001ffff
#define	MVIC_VTBAR_VDMA_SHIFT		0

#define	MVIC_VTBAR_VVDMA_MASK		0x0003ffff	/* Composite */
#define	MVIC_VTBAR_VVDMA_SHIFT		0

/*
* MBus address output of MVIC A24 VDMA Translation
*/

#define	MVIC_VDMA_PFN_MMBA_MASK		0xf00000	/* MBAR_MMBA Portion */
#define	MVIC_VDMA_PFN_MMBA_SHIFT	(24-4)		/* Must be the same */
							/* for all pages */

#define	MVIC_VDMA_PHY_ZERO_MASK		0xe0000000	/* Must be zero */

#define	MVIC_VDMA_PHY_VTBAR_MASK	0x1ffff000	/* VTBAR_VDMA Portion*/
#define	MVIC_VDMA_PHY_VTBAR_SHIFT	12

#ifndef	_ASMLANGUAGE

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	mvicInit (MVICInit * initData);
extern STATUS	mvicMailboxEnable (char *mailboxAdrs);
extern BOOL	mvicProcessInt (void);
#else	/* __STDC__ */

extern STATUS	mvicInit ();
extern STATUS	mvicMailboxEnable ();
extern BOOL	mvicProcessInt ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCmb86986Vmeh */
