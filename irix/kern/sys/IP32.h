#ifndef __SYS_IP32_H__
#define __SYS_IP32_H__

#define _ARCSPROM

#ifdef _STANDALONE
#include <sys/mips_addrspace.h>
#endif
#include <sys/crime.h>
#include <sys/mace.h>

#define SPLMIN			0
#define SPLMAX			5

/*
 * Symbolic constants for use w/setcrimevector()
 */
#define VICE_CPU_INTR		31
#define MEMERR_INTR		21
#define CRMERR_INTR		20
#define SOFT_INTR(x)		(28 + (x))	/* x = 0 - 2  */
#define RE_INTR(x)		(22 + (x))	/* x = 0 - 5  */
#define GBE_INTR(x)		(16 + (x))	/* x = 0 - 3  */
#define MACE_INTR(x)		(x) 		/* x = 0 - 15 */

#define SPL0                    0               /* spl0()                */
#define SPL1                    1               /* spl1()                */
#define SPL3			1		/* splnet(), spl3()      */
#define SPL5			1		/* spltty(), spl5()      */
#define SPLHINTR		1		/* splhintr(), splgio2() */
#define SPL6			1		/* splhi(), spl6()       */
#define SPL65			2		/* splprof(), spl65()    */
#define SPL7			3		/* spl7()                */
#define SPLECC			4		/* splecc()              */

#define CRM_EXCL		0x1		/* no ganged intrs here */
#define CRM_CHKSTAT             0x2             /* check interrupt status */
                                                /* before calling handler */
#define CRM_DRVENB		0x4             /* driver enables CRIME intr */

#define	TLBLO_HWBITS		0x0fffffff	/* 22 bit ppn, plus CDVG */
#define TLBLO_PFNTOKDMSHFT	4		/* tlblo pfn to direct mapped */
#define TLBLO_HWBITSHIFT	4		/* A shift value for masking */

#define PHYS_RAMBASE            0x00000000	/* base addr for 256Mb space */
#define LINEAR_BASE             0x40000000
#define SEG0_BASE		0x00000000	/* can be mapped to k0/k1    */
#define SEG0_SIZE               0x10000000
#define SEG1_BASE		(LINEAR_BASE+SEG0_SIZE)	
                                                /* cannot be mapped to k0/k1 */
						/* address skips over 256Mb  */
						/* double mapped at 0x0      */
#define MINMEMSIZE              0x2000000       /* 32Mb, min mem config      */
#define K0_RAMBASE              PHYS_TO_K0(PHYS_RAMBASE)
#define K1_RAMBASE              PHYS_TO_K1(PHYS_RAMBASE)

#define PHYS_TO_K0_RAM(x)       PHYS_TO_K0((x)+K0_RAMBASE)
#define PHYS_TO_K1_RAM(x)       PHYS_TO_K1((x)+K1_RAMBASE)
#define SYMMON_STACK            PHYS_TO_K0_RAM(0x6000)
#define SYMMON_STACK_ADDR(x)	SYMMON_STACK
#if _MIPS_SIM == _ABI64
#define SYMMON_STACK_SIZE	0x2000
#else
#define SYMMON_STACK_SIZE	0x1000
#endif
#define RESTART_ADDR		PHYS_TO_K0_RAM(0x400)

#define ECCBYPASS_BASE		0x80000000	/* ECC-bypass memory alias */

#ifndef _STANDALONE
#include <sys/IP32flash.h>
#endif

#ifdef _LANGUAGE_C
#define RT_CLOCK_ADDR \
	 (struct ds17287_clk *)PHYS_TO_K1(ISA_RTC_BASE+7)
#else
#define RT_CLOCK_ADDR		PHYS_TO_K1(ISA_RTC_BASE+7)
#endif

#define RT_RAM_FLAGS		0x0
#define RT_FLAGS_INVALID	0x1
#define RT_RAM_BASE             0x1  /* offset for general r/w nvram */

#if 0
#define PROM_RAMBASE            PHYS_TO_K0_RAM(0x400000)
#define PROM_STACK              PHYS_TO_K1_RAM(0x800000)
#define PROM_TILE_BASE          PHYS_TO_K1_RAM(0x500000)
#define PROM_TILE_CNT           21
#else
#define PROM_RAMBASE            PHYS_TO_K0_RAM(0x1000000)
#define PROM_STACK              PHYS_TO_K1_RAM(0x1400000)
#define PROM_TILE_BASE          PHYS_TO_K1_RAM(0x1100000)
#define PROM_TILE_CNT           21
#endif


/*
 * RTC configuration data.
 */
#define RTC_BASE PHYS_TO_K1(ISA_RTC_BASE+7)

/*
 * Firmware RTC_NVRAM constants
 */
#define RTC_SAVE_UST   37	/* Saved UST value */
#define RTC_SAVE_REG   41	/* Beginning of 8 byte register save area */
#define RTC_RESET_CTR  49	/* Firmware reset counter byte */


/*
 * IP32 Serial port configuration data
 */
#define N16550PORTS 2		/* Number of 16550 serial ports */
#define SERIAL_PORT0_BASE PHYS_TO_K1(ISA_SER1_BASE + 7)
#define SERIAL_PORT1_BASE PHYS_TO_K1(ISA_SER2_BASE + 7)
#define SERIAL_CLOCK_FREQ (1843200)

#if 0
/*
 * Flash memory parameters
 */
#define FLASH_BASE		0x1fc00000
#define FLASH_PAGE_SIZE 	0x200
#define FLASH_PROTECTED		0x4000 /* Size of protected segment */
#define FLASH_SIZE              (512*1024)
#define FLASH_PROGRAMABLE	(FLASH_BASE+FLASH_PROTECTED)
#endif


/*
 * Ethernet configuration data
 */
#define MAC110_BASE		PHYS_TO_K1(MACE_ENET)

/*
 * Firmware dispatch codes
 *
 * NOTE: Probably shouldn't be here.
 */
#define FW_HARD_RESET	0	/* Power on or hard reset*/
#define FW_SOFT_RESET	1	/* Soft reset or NMI */
#define FW_EIM		2	/* Enter Interactive Mode */
#define FW_HALT		3	/* Halt */
#define FW_POWERDOWN	4	/* Power down */
#define FW_RESTART	5	/* Restart */
#define FW_REBOOT	6	/* Reboot */
#define FW_INIT   	7	/* fw init callback */


#ifdef LANGUAGE_C

/* chip interface structure for IP22 / HPC / WD33C93 */
typedef struct scuzzy {
	volatile unsigned char	*d_addr;	/* address register */
	volatile unsigned char	*d_data;	/* data register */
	volatile unsigned long	*d_ctrl;	/* control address */
	volatile unsigned long	*d_bcnt;	/* byte count register */
	volatile unsigned long	*d_curbp;	/* current buffer pointer */
	volatile unsigned long	*d_nextbp;	/* next buffer pointer */
	volatile unsigned long	*d_dmacfg;	/* fifo pointer register */
	volatile unsigned long	*d_piocfg;	/* fifo data register */
	unsigned char d_initflags;	/* initial flags for d_flags */
} scuzzy_t;

#endif

/* Location of the eframe and stack for the ecc handler.
 * Since the R4000 'replaces' KUSEG with an unmapped, uncached space
 * corresponding to physical memory when a cache error occurs, these are
 * the actual addresses used.
 */
#if R4000
#define CACHE_ERR_EFRAME	(0x1000 - EF_SIZE)
#define CACHE_ERR_ECCFRAME	(CACHE_ERR_EFRAME - ECCF_SIZE)

/* 4 arguments on the stack. */
#define CACHE_ERR_SP		(CACHE_ERR_ECCFRAME - 4 * sizeof(long))
#else
/* scratch area and ptr to ECC frame/stack */
#define CACHE_ERR_K1_SAVE       0x0ff8
#define CACHE_ERR_FRAMEPTR      0x0ff0
#define ECC_SCRATCH_LINE        0x0f00
#endif


/*
 * ECC error handler defines.
 * Define an additional exception frame for the ECC handler.
 * Save 3 more registers on this frame: C0_CACHE_ERR, C0_TAGLO,
 * and C0_ECC.
 * Call this an ECCF_FRAME.
 */
#define ECCF_CACHE_ERR	     0
#define ECCF_TAGLO	     1
#define ECCF_ECC	     2
#define ECCF_ERROREPC	     3
#define ECCF_PADDRHI         4  /* because CPU bus error address is 34 bits */
#define ECCF_PADDR	     5
#define ECCF_CES_DATA	     6
#define ECCF_CPU_ERR_STAT    7
#define ECCF_MEM_ERR_STAT    8
#define ECCF_CPU_ERR_ADDRHI  9 /* CPU bus error address is 34 bits */
#define ECCF_CPU_ERR_ADDR   10
#define ECCF_MEM_ERR_ADDR   11
#define ECCF_SIZE	(12 * sizeof(long))


#define MAXCPU	1

#define CAUSE_BERRINTR	0x100000	/* bit 20 of CRM_INTSTAT */


#define EGUN_PHYS	1	/* bad ecc to phys address */
#define EGUN_PHYS_WORD	2	/* bad ecc at memory word offset */
#define EGUN_PROCVIRT	3	/* bad ecc in proc virtual address */

#ifdef LANGUAGE_C
struct egun_cmd {
    unsigned long	addr;
    int			ecc_repl;
    pid_t		pid;
};
#endif /* LANGUAGE_C */

#ifndef STANDALONE
#if _SYSTEM_SIMULATION
#include <sys/MHSIM.h>
#endif
#endif


#ifdef LANGUAGE_C
#define MAXNVNAMELEN	32
/* format used to store nvram table information
 */
struct nvram_entry {
    char nt_name[MAXNVNAMELEN];		/* string name of entry   */
    char *nt_value;			/* current value of entry */
};

/* function prototypes for IP32 functions */

typedef void (*intvec_func_t)(eframe_t *, __psint_t);

/* IP32 specific functions */
#ifndef _STANDALONE

/*
 * Routines defined within 'mace.c', the MACE driver.
 */
extern int setcrimevector(int, int, intvec_func_t, __psint_t, short);
extern int unsetcrimevector(int, intvec_func_t);
extern void crime_intr_enable(int);
extern void crime_intr_disable(int);

typedef __uint64_t _macereg_t;

extern void setmaceisavector(int intr, _macereg_t macebits, intvec_func_t isr);
extern void unsetmaceisavector(int intr, _macereg_t macebits);

extern	void            mace_mask_write(_macereg_t m_mask);
extern	_macereg_t      mace_mask_read();
extern	void    	mace_mask_enable(_macereg_t driver_mask);
extern	void    	mace_mask_disable(_macereg_t driver_mask);
extern	void    	mace_mask_update(_macereg_t driver_mask, 
					 _macereg_t current_mask);

/* End of routines defined within 'mace.c' */

extern paddr_t get_isa_dma_buf_addr(void);
extern void isa_dma_buf_init(void);
extern int splint(int);
extern int splhintr(void);
extern void set_autopoweron(void);
extern void scrub_memory(caddr_t);
extern int addr_to_bank(paddr_t);
extern int vice_err(_crmreg_t, _crmreg_t);
extern int bank_size(_crmreg_t);
extern void load_nvram_tab(void);
extern unsigned char *etoh(char *);
extern void flash_write_env(void);
#ifdef TILES_TO_LPAGES
extern int splretr(void);
#endif /* TILES_TO_LPAGES */

/* from ml/ust_conv.c - use to fill in high 32 bits for 64-bit nanotime */
extern void fill_ust_highbits(unsigned int mace32, unsigned long long *mace64);

/* stuff which should be in a different header file (sys/ecc.h?) */
extern int _r4600sc_disable_scache(void);
extern void _r4600sc_enable_scache(void);
extern int _read_tag(int, caddr_t, int *);
extern int tlb_to_phys(k_machreg_t , paddr_t *, int *);
extern unsigned int r_phys_word(paddr_t);
extern unsigned int r_phys_word_erl(paddr_t);
extern int ecc_same_cache_block(int,paddr_t,paddr_t);
extern int decode_inst(eframe_t *, int, int *, k_machreg_t *, int *);
extern int ecc_meminit(pfn_t, pfn_t);
extern void ecc_enable(void);
extern void ecc_disable(void);
extern pfn_t last_phys_pfn(void);
extern int ecc_meminited;  /* amount of memory cleared by ecc_meminit */
extern void early_mte_zero(paddr_t, size_t);
#endif 

#if (_MIPS_ISA == 3 || _MIPS_ISA == 4)
#define WRITE_REG64(val, addr, type) \
	(*(volatile uint64_t *)(addr) = (val))
#else
#define WRITE_REG64(val, addr, type) \
	write_reg64((long long)(val), (__psunsigned_t)(addr))
#endif /* _MIPS_ISA == 3 or 4 */

#if (_MIPS_ISA == 3 || _MIPS_ISA == 4)
#define READ_REG64(addr, type) \
	((type)(*(volatile long long *)(addr)))
#else
#define READ_REG64(addr, type) \
	((type)read_reg64((__psunsigned_t)(addr)))
#endif /* _MIPS_ISA == 3 or 4 */

#define REG_RDANDWR8(_a, _m)	pciio_pio_write8(pciio_pio_read8((_a))&(_m),(_a))
#define REG_RDORWR8(_a, _m)	pciio_pio_write8(pciio_pio_read8((_a))|(_m),(_a)) 

/* Defines for CRIME Texture TLB */
#define CRM_TEX_MAX_RESIDENT	128	   /* Max. # of loadable tex. MipMaps */
#define CRM_TEX_TLB_SIZE	28	   /* Texture TLB size */
#define CRM_TEX_FIFOMAX	24		   /* High level for fast tex. loads */
#define CRM_TEX_FIFOSRL	18		   /* FIFO level shift right count */
#define CRM_TEX_FIFOMASK	0x7f	   /* FIFO level mask */
#define CRM_TEX_IBSTAT_ADDR	0xb5004000 /* Address of IB status */
#define CRM_TEX_TLB_FIRST_ADDR	0xb5001600 /* Address of 1st TLB entry */
#define CRM_TEX_TLB_LAST_ADDR	0xb50016d8 /* Address of last TLB entry */
struct CrmTexTlb {
	long long	tlbAddr;
	union {
		u_short taddr[4];
		u_long   laddr[2];
		long long   dw;
	}tlbData;
};

#ifdef MH_R10000_SPECULATION_WAR
/*
 * Kernel vaddr where extk0 is mapped ... this is aligned to make
 * the mapping starting at (SMALLMEM_K0_R10000_PFNMAX+1), so the
 * area can be mapped with big ptes (see extk0_avail_alloc).
 */
#define EXTK0_OFFSET	0x1800000
#define EXTK0_BASE	(K2BASE + EXTK0_OFFSET)
#endif /* MH_R10000_SPECULATION_WAR */

#endif /* LANGUAGE_C */

#ifdef R10000
/* scratch area and ptr to ECC frame/stack */
#define CACHE_TMP_EMASK		0x3e00
#define CACHE_TMP_EFRAME1	0x0c00
#define CACHE_TMP_EFRAME2	0x0e00
#endif /* R10000 */

#endif /* __SYS_IP32_H__ */
