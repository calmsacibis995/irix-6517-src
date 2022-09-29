/**************************************************************************
 *                                                                        *
 *	       Copyright (C) 1992-1993, Silicon Graphics, Inc.            *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_EVEREST_EVERROR_H__
#define __SYS_EVEREST_EVERROR_H__

#include <sys/reg.h>

#ifdef _LANGUAGE_C
#include <sys/types.h>
#include <sys/EVEREST/mc3.h>

/************************************************************************	
 *									*
 *		#     # ####### ####### #######    #			*
 *		##    # #     #    #    #         ###			*
 *		# #   # #     #    #    #          #			*
 *		#  #  # #     #    #    #####				*
 *		#   # # #     #    #    #          #			*
 *		#    ## #     #    #    #         ###			*
 *		#     # #######    #    #######    #			*
 *									*
 *	This structure is used outside the kernel by crash analysis	*
 *	tools like the FRU analyzer and icrash.  Please don't change	*
 *	structure defitions, and only add new structures at the end of  *
 *	everror.  You should also bump EVERROR_VERSION after making.	*
 *	any change.  Also note that there's a limited amount of space 	*
 *	for everror in low memory.  See the comments on everror_t for	*
 *	more information.						*
 *									*
 ************************************************************************/
#ident	"$Revision: 1.42 $"
/*
 * Version Changes:
 * Version 1: Whatever it was in Source file Revision 1.31
 * Version 2: EV_NUM_DANGADAPS is bumped to 12 from 8. 
 */
#define EVERROR_VERSION	3
#define	EVERROR_VALID	(0xdeadbeef)

typedef struct iperror {
	evreg_t	a_error;
} iperror_t;
#endif /* _LANGUAGE_C */

/*
 * Per processor private error states include,
 *      1. CC error reg.
 *      2. Evereything about exception.
 *      3. CPU cache (optional).
 *      
 * The cpuerror_t struct should be initialized with an impossible value just
 * to be sure that the save error state attempt did not succeed.
 */

/*
 * The size of cache for everest is,
 *	Primary Code/Data:	16 byte (4 word) cache line and 32K byte each.
 *	Secondary Cache:	128 byte (32 word) cache line and IM byte.
 */

#define PCACHE_LINESIZE		16
#define PCACHE_SIZE		0x4000
#define PCACHE_LINES		(SCACHE_SIZE/ SCACHE_LINESIZE)
#define SCACHE_LINESIZE		128
#define SCACHE_SIZE		0x100000
#define SCACHE_LINES		(SCACHE_SIZE / SCACHE_LINESIZE)


#ifdef _LANGUAGE_C

typedef struct pcache_line {
	unchar	p_data[PCACHE_LINESIZE];
	unchar	p_parity[PCACHE_LINESIZE/8];	/* byte parity */
	uint	p_tag;
} pcache_line_t;

typedef struct scache_line {
	unchar	s_data[SCACHE_LINESIZE];
	unchar	s_ecc[SCACHE_LINESIZE/8];       /* 64-bit data + 7-bit ecc */
	uint	s_tag;
	uint	s_bustag;
} scache_line_t;

typedef struct cachedump {		/* for complete dump */
#if defined(IRIX5_3) || defined(IRIX5_2)
	pcache_line_t	*i_cachedump;	/* PCACHE_LINES */
	pcache_line_t	*d_cachedump;
	scache_line_t	*s_cachedump;
#else
	int i_cachedump;
	int d_cachedump;
	int s_cachedump;
#endif
} cachedump_t;

typedef struct cacheline {		/* for cache line/phys address dump */
	pcache_line_t	i_cacheline[8]; /* for each phys address, it may be   */
	pcache_line_t	d_cacheline[8]; /* in one of 8 different pcache lines */
	scache_line_t	s_cacheline;
} cacheline_t;
	
/* 
 * NOTE : 
 * Have pointers to cachline instead of actual space. At the low address 
 * in memory where EVERROR is dumped, we just have 2k space. 
 * When we do cache line dumping, we identify some other memory area, 
 * dump the cache lines there, and place the pointers here 
 * !!!!!!! Changed cacheline  to pointer instead of space holder !!!!!!!!!
 */
typedef struct cpuerror {
#if IP25
        __uint32_t cc_ertoip;
#else
	ushort	cc_ertoip;
	ushort  external_intr;
#endif /* IP25 */

#ifdef	IRIX5_2
	/* These were removed after 5.2... */
	/* Since everror_t structure is running out of space, and these two
	 * fields are not in use, they loose their chance!! - Bhanu
	 */
	cacheline_t     *cacheline;
	cachedump_t     *cachedump;
#endif

} cpuerror_t;

	
typedef struct mc3error {
	unchar	dummy;		/* to force alignment, not needed really */
	unchar 	ebus_error;
	unchar	mem_error[2];
	uint	erraddrhi[2];
	uint	erraddrlo[2];
	ushort	syndrome0[2];
	ushort	syndrome1[2];
	ushort	syndrome2[2];
	ushort	syndrome3[2];
} mc3error_t;


/*
 *  In-memory accumulation of memory singlebit ecc error information.
 */
#define	MC3_SIMMS_PER_BANK 4
#ifndef _STANDALONE
typedef struct mc3_bank_err {
	int		m_first_err_time;
	int		m_last_err_time;
	int		m_last_log_time;
	unsigned	m_bank_errcount;
	unsigned	m_simm_errcount[MC3_SIMMS_PER_BANK];
} mc3_bank_err_t;

typedef struct mc3_array_err {
	unsigned	m_unk_bank_errcount; /* unresolvable to specific bank */
	mc3_bank_err_t	m_bank_errinfo[MC3_NUM_BANKS];
} mc3_array_err_t;
#endif

/*
 * The number of io boards is restricted by windows.
 * IO4 config registers are double word addressed but ebus error address
 * register pair (0xb and 0xc) holds 32bit each, be careful with the handling.
 */
typedef struct io4error {
	uint	ibus_error;
	uint	ebus_error;
	uint	ebus_error1;	/* These two together makes one 64 bit reg */
	uint	ebus_error2;
} io4error_t;

typedef struct epcerror {
	uint	ibus_error;
	uint	ia_np_error1;
	uint	ia_np_error2;
} epcerror_t;

typedef struct vmeccerror {
	ushort	error;
	uint	addrebus;
	uint	addrvme;
	uint	xtravme;
} vmeccerror_t;

typedef struct io4hiperror {
	ushort	error;
	uint	addrebus;
	uint	addrvme;
	uint	xtravme;
} io4hiperror_t;

typedef struct fchiperror {
	uint   error;
	evreg_t ibus_addr;
	uint   fci;
} fchiperror_t;

typedef struct fcgerror {	/* for graphics adaptors (FCGs and HIPs) */
	uint	error;
} fcgerror_t;

typedef struct dangerror {
	ushort p_error;
	ushort md_error;
	ushort sd_error;
	ushort md_cmplt;
} dangerror_t;

typedef struct s1error {
	uint	ibus_error;
	uint	csr;
	uint	ierr_loglo;
	uint	ierr_loghi;
} s1error_t;

typedef struct cpuerr_ext {
#if 	IP25
        evreg_t	ext_cc_eraddr;
#else 
        uint	cpu_cache_err;
#endif
} cpuerr_ext_t;

/*
 * Everest Error Data Structure..
 * NOTE :
 * Total Size of this structure should not exceed 0x800 bytes....
 * As of now it uses up 0x760 bytes..
 */
typedef struct everror {
	iperror_t	ip[EV_MAX_IPS];
	cpuerror_t	cpu[EV_MAX_CPUS];
	mc3error_t	mc3[EV_MAX_MC3S];
	io4error_t	io4[EV_MAX_IO4S];
	epcerror_t	epc[EV_MAX_EPCADAPS];
	vmeccerror_t	vmecc[EV_MAX_VMEADAPS];
	fchiperror_t	fvmecc[EV_MAX_VMEADAPS];
	io4hiperror_t	io4hip[EV_MAX_HIPADAPS];
	fchiperror_t	fio4hip[EV_MAX_HIPADAPS];
	fcgerror_t	fcg[EV_MAX_FCGADAPS];
	fchiperror_t	ffcg[EV_MAX_FCGADAPS];
	dangerror_t	dang[EV_MAX_DANGADAPS];
	s1error_t	s1[EV_MAX_S1ADAPS];
/* PLEASE ADD ANY FURTHER DATA STRUCTURES BELOW THIS LINE. While adding,
 * please make sure that the size of this structure DOES NOT EXCEED 0x800
 * bytes. Since this is located at fixed address in low memory, any 
 * excessive bytes would spill into adjacent data structures, and cause
 * weird problems. As is stands now, it uses about 0x700 bytes.
 */
	struct everror_ext 	*ev_ext;
} everror_t;

typedef struct everror_ext {
#if IP25
        void 		*eex_ecc;	/* array of ECC frames */
#endif
        cpuerr_ext_t	eex_cpu[EV_MAX_CPUS];
} everror_ext_t;

/*
 * The error structure is right behind config info.
 */
#define EVERROR		((everror_t*)EVERROR_ADDR)
#define EVERROR_EXT		((everror_ext_t*)EVERROR_EXT_ADDR)

#endif /* _LANGUAGE_C */


/*
 * CC ERTOIP- To do extra-kernel analysis we need IP19/IP21/IP25 in the same
 * 	file, hence IPXX_CC_...
 */
#if R4000
#define IP19_CC_ERROR_SCACHE_MBE	0x0001
#define IP19_CC_ERROR_SCACHE_SBE	0x0002
#define IP19_CC_ERROR_PARITY_TAGRAM	0x0004
#define IP19_CC_ERROR_PARITY_A		0x0008
#define IP19_CC_ERROR_PARITY_D		0x0010
#define IP19_CC_ERROR_MYREQ_TIMEOUT	0x0020
#define IP19_CC_ERROR_MYRES_TIMEOUT	0x0040
#define IP19_CC_ERROR_MYINT_TIMEOUT	0x0080
#define IP19_CC_ERROR_MY_ADDR		0x0100
#define IP19_CC_ERROR_MY_DATA		0x0200
#define IP19_CC_ERROR_ASYNC		0x0400

/* There's no point in making the kernel code messy so we define these. */
#define CC_ERROR_MYREQ_TIMEOUT	IP19_CC_ERROR_MYREQ_TIMEOUT
#define CC_ERROR_MYRES_TIMEOUT	IP19_CC_ERROR_MYRES_TIMEOUT
#define CC_ERROR_PARITY_D	IP19_CC_ERROR_PARITY_D
#define CC_ERROR_MY_ADDR	IP19_CC_ERROR_MY_ADDR
#define CC_ERROR_MY_DATA	IP19_CC_ERROR_MY_DATA
#endif /* R4000 */

#if R10000
#define	IP25_CC_ERROR_MBE_INTR		0x0002
#define	IP25_CC_ERROR_SBE_INTR		0x0004
#define IP25_CC_ERROR_PARITY_TAGRAM	0x0008
#define IP25_CC_ERROR_PARITY_A		0x0010
#define IP25_CC_ERROR_MYREQ_TIMEOUT	0x0020
#define IP25_CC_ERROR_MYRES_TIMEOUT	0x0040
#define	IP25_CC_ERROR_MYINT_TIMEOUT	0x0080

#define IP25_CC_ERROR_MY_ADDR		0x0100
#define IP25_CC_ERROR_MY_DATA		0x0200
#define IP25_CC_ERROR_ASYNC		0x0400
#define	IP25_CC_ERROR_T5_BADDATA	0x0800

#define	IP25_CC_ERROR_D_0_15		0x1000
#define	IP25_CC_ERROR_D_16_31		0x2000
#define	IP25_CC_ERROR_D_32_47		0x4000
#define	IP25_CC_ERROR_D_48_63		0x8000
#define IP25_CC_ERROR_PARITY_D		0xf000

#define	IP25_CC_ERROR_DBE_SYSAD		0x10000
#define	IP25_CC_ERROR_SBE_SYSAD		0x20000
#define	IP25_CC_ERROR_PARITY_SS		0x40000
#define	IP25_CC_ERROR_PARITY_SC		0x80000

#define	IP25_CC_ERROR_ME		0x100000

/* There's no point in making the kernel code messy so we define these. */
#define CC_ERROR_MYREQ_TIMEOUT	IP25_CC_ERROR_MYREQ_TIMEOUT
#define CC_ERROR_MYRES_TIMEOUT	IP25_CC_ERROR_MYRES_TIMEOUT
#define CC_ERROR_PARITY_D	IP25_CC_ERROR_PARITY_D
#define CC_ERROR_MY_ADDR	IP25_CC_ERROR_MY_ADDR
#define CC_ERROR_MY_DATA	IP25_CC_ERROR_MY_DATA

/* Error address register definitions */

#define	IP25_CC_ERADDR_ADDR		0xffffffffff
#define	IP25_CC_ERADDR_CAUSE_SHFT	40
#define	IP25_CC_ERADDR_CAUSE_MASK	(3LL<<IP25_CC_ERADDR_CAUSE_SHFT)
#define	IP25_CC_ERADDR_CAUSE(x)		\
        (((x) & IP25_CC_ERADDR_CAUSE_MASK) >> IP25_CC_ERADDR_CAUSE_SHFT)
#define	IP25_CC_ERADDR_VALID		(1LL<<42)

#endif /* R10000 */

#if TFP
#define IP21_CC_ERROR_DB0_PE		0x0001
#define IP21_CC_ERROR_DB1_PE		0x0002
#define IP21_CC_ERROR_PARITY_A		0x0004
#define IP21_CC_ERROR_MYREQ_TIMEOUT0	0x0008
#define IP21_CC_ERROR_MYREQ_TIMEOUT1	0x0010
#define IP21_CC_ERROR_MY_ADDR0		0x0020
#define IP21_CC_ERROR_MY_ADDR1		0x0040
#define IP21_CC_ERROR_DATA_SENT0	0x0080
#define IP21_CC_ERROR_DATA_SENT1	0x0100
#define IP21_CC_ERROR_DATA_RECV		0x0200
#define IP21_CC_ERROR_ASYNC		0x0400
#define IP21_CC_ERROR_MYRES_TIMEOUT0	0x0800
#define IP21_CC_ERROR_MYRES_TIMEOUT1	0x1000
#define IP21_CC_ERROR_PARITY_DBCC	0x2000

#define IP21_CC_ERROR_MYREQ_TIMEOUT	(IP21_CC_ERROR_MYREQ_TIMEOUT0 | \
					 IP21_CC_ERROR_MYREQ_TIMEOUT1)
#define IP21_CC_ERROR_MYRES_TIMEOUT	(IP21_CC_ERROR_MYRES_TIMEOUT0 | \
					 IP21_CC_ERROR_MYRES_TIMEOUT1)
#define IP21_CC_ERROR_PARITY_D	(IP21_CC_ERROR_DB0_PE | IP21_CC_ERROR_DB1_PE)
#define IP21_CC_ERROR_MY_ADDR	(IP21_CC_ERROR_MY_ADDR0 | \
				 IP21_CC_ERROR_MY_ADDR1)
#define IP21_CC_ERROR_MY_DATA	(IP21_CC_ERROR_DATA_SENT0 | \
				 IP21_CC_ERROR_DATA_SENT1 | \
				 IP21_CC_ERROR_DATA_RECV)

/* There's no point in making the kernel code messy so we define these. */
#define CC_ERROR_MYREQ_TIMEOUT	IP21_CC_ERROR_MYREQ_TIMEOUT
#define CC_ERROR_MYRES_TIMEOUT	IP21_CC_ERROR_MYRES_TIMEOUT
#define CC_ERROR_PARITY_D	IP21_CC_ERROR_PARITY_D
#define CC_ERROR_MY_ADDR	IP21_CC_ERROR_MY_ADDR
#define CC_ERROR_MY_DATA	IP21_CC_ERROR_MY_DATA
#endif /* TFP */

/*
 * A-chip error
 */
#define A_ERROR_ADDR_HERE_TIMEOUT	0x1000
#define A_ERROR_ADDR_HERE_TIMEOUT0	0x1000
#define A_ERROR_ADDR_HERE_TIMEOUT1	0x2000
#define A_ERROR_ADDR_HERE_TIMEOUT2	0x4000
#define A_ERROR_ADDR_HERE_TIMEOUT3	0x8000
#define A_ERROR_ADDR_HERE_TIMEOUT_ALL	0xf000
#define A_ERROR_CC2D_PARITY		0x0100
#define A_ERROR_CC2D_PARITY_ALL		0x0f00
#define A_ERROR_MY_ADDR_ERR		0x0020
#define A_ERROR_ADDR_ERR		0x0010
#define A_ERROR_CC2A_PARITY		0x0001
#define A_ERROR_CC2A_PARITY_ALL		0x000f

/*
 * MC3
 */
#define MC3_EBUS_ERROR_SENDER_ADDR	0x8
#define MC3_EBUS_ERROR_SENDER_DATA	0x4
#define MC3_EBUS_ERROR_ADDR		0x2
#define MC3_EBUS_ERROR_DATA		0x1

#define MC3_MEM_ERROR_MULT		0x8
#define MC3_MEM_ERROR_READ_SBE		0x4
#define MC3_MEM_ERROR_READ_MBE		0x2
#define MC3_MEM_ERROR_PWRT_MBE		0x1

#define MC3_MEM_ERROR_MBE (MC3_MEM_ERROR_READ_MBE|MC3_MEM_ERROR_PWRT_MBE)

/*
 * IO4_CONF_IBUSERROR (0x07) bit field definitions.
 */	
#define IO4_IBUSERROR_IOA(e)		((e&0x00070000) >> 16)
#define IO4_IBUSERROR_COMMAND		0x8000
#define IO4_IBUSERROR_PIORESPCMND	0x4000
#define IO4_IBUSERROR_DMAWCMND		0x2000
#define IO4_IBUSERROR_DMAWDATA		0x1000
#define IO4_IBUSERROR_PIORESPDATA	0x0800
#define IO4_IBUSERROR_PIOWCMND		0x0400
#define IO4_IBUSERROR_PIOWDATA		0x0200
#define IO4_IBUSERROR_PIORCMND		0x0100
#define IO4_IBUSERROR_GFXWCMND		0x0080
#define IO4_IBUSERROR_DMARESPCMND	0x0040
#define IO4_IBUSERROR_IARESP		0x0020
#define IO4_IBUSERROR_1LVLMAPRESP	0x0010
#define IO4_IBUSERROR_1LVLMAPDATA	0x0008
#define IO4_IBUSERROR_2LVLMAPRESP	0x0004
#define IO4_IBUSERROR_2LVL1MAP		0x0002
#define IO4_IBUSERROR_STICKY		0x0001

#define IO4_IBUS_CMDERR_FR_IOA		(IO4_IBUSERROR_COMMAND		| \
					 IO4_IBUSERROR_PIORESPCMND	| \
					 IO4_IBUSERROR_DMAWCMND)

#define IO4_IBUSERROR_MAPRAM		(IO4_IBUSERROR_1LVLMAPDATA	| \
					 IO4_IBUSERROR_2LVL1MAP)

#define IO4_IBUS_CMDERR_TO_IOA		(IO4_IBUSERROR_PIOWCMND		| \
					 IO4_IBUSERROR_PIORCMND		| \
					 IO4_IBUSERROR_DMARESPCMND	| \
					 IO4_IBUSERROR_GFXWCMND		| \
					 IO4_IBUSERROR_1LVLMAPRESP	| \
					 IO4_IBUSERROR_2LVLMAPRESP)

/*
 * IO4_CONF_EBUSERROR bit field definition.
 */
#define IO4_EBUSERROR_PIOQFULL_MASK	0xFF000000
#define IO4_EBUSERROR_PIOQFULL_SHIFT	24
#define IO4_EBUSERROR_DATA_ERR		0x0200
#define IO4_EBUSERROR_TIMEOUT		0x0100
#define IO4_EBUSERROR_INVDIRTYCACHE	0x0080
#define IO4_EBUSERROR_TRANTIMEOUT	0x0040
#define IO4_EBUSERROR_MY_ADDR_ERR	0x0020
#define IO4_EBUSERROR_PIO		0x0010
#define IO4_EBUSERROR_BADIOA		0x0008
#define IO4_EBUSERROR_ADDR_ERR		0x0004
#define IO4_EBUSERROR_MY_DATA_ERR	0x0002
#define IO4_EBUSERROR_STICKY		0x0001

/*
 * IO4_CONF_IO4_EBUSERROR2_0 bit field definition.
 */		     
#define IO4_EBUSERROR2_ADDRESSL		0 /* the low 32 bit of ebus address */

/*
 * IO4_CONF_IO4_EBUSERROR2_1 bit field definition.
 */
#define IO4_EBUSERROR2_HIADDRMASK	0x000000FF
#define IO4_EBUSERROR2_COMMAND		0x0000FF00
#define IO4_EBUSERROR2_IOA_ID		0x00070000
#define IO4_EBUSERROR2_WRITEBACK	0x00080000
#define IO4_EBUSERROR2_DMAWRITE		0x00100000
#define IO4_EBUSERROR2_READEXCL		0x00200000
#define IO4_EBUSERROR2_DMAREAD		0x00400000
#define IO4_EBUSERROR2_ADDRMAP		0x00800000
#define IO4_EBUSERROR2_IAINTR		0x01000000

/*
 * VMECC
 */
#define VMECC_ERROR_VMEBUS_PIOW	0x0001
#define VMECC_ERROR_VMEBUS_PIOR	0x0002
#define VMECC_ERROR_VMEBUS_SLVP	0x0004
#define VMECC_ERROR_VMEBUS_TO	0x0008
#define VMECC_ERROR_FCIDB_TO	0x0010
#define VMECC_ERROR_FCI_PIOPAR	0x0020
#define VMECC_ERROR_OVERRUN	0x0040
#define VMECC_ERROR_DROPMODE	0x0080


/*
 * EPC
 */
#define EPC_IERR_IA(e)		((e & 0x000F) >> 0)
#define EPC_IERR_NP(e)		((e & 0x00F0) >> 4)
#define EPC_IERR_NP_IOA(e)      ((e & 0x0F00) >> 8)
#define EPC_IERR_EPC(e)		((e & 0xF000) >> 12)
#define	EPC_IBUS_PAR(e)		((e & 0xF0000) >> 16)
#define	EPC_DMA_RDRSP_TOUT	0x200000

#define EPC_IERR_IA_NOERROR	0
#define EPC_IERR_IA_PIOW_DATA	1
#define EPC_IERR_IA_DMAR_DATA	2
#define EPC_IERR_IA_UNXPCTD_DMA	3
#define EPC_IERR_IA_BAD_CMND	4

#define EPC_IERR_NP_NOERROR	0
#define EPC_IERR_NP_PIOW_DATA	1
#define EPC_IERR_NP_GFXW_DATA	2
#define EPC_IERR_NP_DMAR_DATA	3
#define EPC_IERR_NP_AMAP_DATA	4

#define EPC_IERR_EPC_NOERROR	0
#define EPC_IERR_EPC_PIOR_CMND	1
#define EPC_IERR_EPC_PIOR_DATA	2
#define EPC_IERR_EPC_DMAR_CMND	3
#define EPC_IERR_EPC_DMAW_CMND	4
#define EPC_IERR_EPC_DMAW_DATA	5
#define EPC_IERR_EPC_INTR_CMND	6
#define EPC_IERR_EPC_PIOR_CMNDX	9
#define EPC_IERR_EPC_PIOR_DATAX	10
#define EPC_IERR_EPC_DMAR_CMNDX	11
#define EPC_IERR_EPC_DMAW_CMNDX	12
#define EPC_IERR_EPC_DMAW_DATAX	13


/*
 * F-chip 
 */
#define FCHIP_ERROR_OVERWRITE		0x00000001
#define FCHIP_ERROR_LOOPBACK_RECEIVED	0x00000002
#define FCHIP_ERROR_LOOPBACK_ERROR	0x00000004

#define FCHIP_ERROR_TO_IBUS_CMND	0x00000008
#define FCHIP_ERROR_TO_IBUS_PIOR_DATA	0x00000010
#define FCHIP_ERROR_TO_IBUS_DMAR_TO	0x00000020
#define FCHIP_ERROR_FR_IBUS_CMND	0x00000040
#define FCHIP_ERROR_FR_IBUS_DMAR_DATA	0x00000080
#define FCHIP_ERROR_FR_FCI_DMAW_DATA	0x00000100
#define FCHIP_ERROR_FR_FCI_PIOR_DATA	0x00000200
#define FCHIP_ERROR_FR_IBUS_PIOW_DATA	0x00000400
#define FCHIP_ERROR_FR_FCI_LOAD_ADDRR	0x00000800
#define FCHIP_ERROR_TO_IBUS_DMAW_CMND	0x00001000
#define FCHIP_ERROR_TO_IBUS_AMAP_CMND	0x00002000
#define FCHIP_ERROR_TO_IBUS_INTR_CMND	0x00004000
#define FCHIP_ERROR_FR_FCI_LOAD_ADDRW	0x00008000
#define FCHIP_ERROR_FR_FCI_UNKNOWN_CMND	0x00010000
#define FCHIP_ERROR_TO_IBUS_AMAP_TO	0x00020000
#define FCHIP_ERROR_FR_IBUS_AMAP_DATA	0x00040000
#define FCHIP_ERROR_FR_IBUS_PIOW_INTRNL	0x00080000
#define FCHIP_ERROR_FR_IBUS_SUPRISE	0x00100000
#define FCHIP_ERROR_TO_IBUS_DMAW_DATA	0x00200000

#define FCHIP_ERROR_SYSTEM_FCI_RESET	0x00400000
#define FCHIP_ERROR_SOFTWARE_FCI_RESET	0x00800000
#define FCHIP_ERROR_MASTER_RESET	0x01000000
#define FCHIP_ERROR_FERROR_FCI_RESET	0x02000000
#define FCHIP_ERROR_F_RESET_IN_PROGRESS	0x04000000
#define FCHIP_ERROR_DROP_PIOW_MODE	0x08000000
#define FCHIP_ERROR_DROP_DMAW_MODE	0x10000000
#define FCHIP_ERROR_FAKE_DMAR_MODE	0x20000000

#define FCHIP_ERROR_MASK		0x003FFFF8


#define	FCHIP_TIMEOUT_ERR	(FCHIP_ERROR_TO_IBUS_DMAR_TO	| \
				 FCHIP_ERROR_TO_IBUS_AMAP_TO)

#define	IBUS2F_DATA_ERR		(FCHIP_ERROR_FR_IBUS_DMAR_DATA	| \
				 FCHIP_ERROR_FR_IBUS_PIOW_DATA	| \
				 FCHIP_ERROR_FR_IBUS_AMAP_DATA)


#define F2IBUS_DATA_ERR		(FCHIP_ERROR_TO_IBUS_PIOR_DATA  | \
				 FCHIP_ERROR_TO_IBUS_DMAW_DATA)

#define	F2IBUS_CMND_ERR		(FCHIP_ERROR_TO_IBUS_CMND	| \
                                 FCHIP_ERROR_TO_IBUS_DMAW_CMND  | \
				 FCHIP_ERROR_TO_IBUS_AMAP_CMND  | \
				 FCHIP_ERROR_TO_IBUS_INTR_CMND)

#define	FCI2F_PAR_ERR		(FCHIP_ERROR_FR_FCI_LOAD_ADDRR	| \
				 FCHIP_ERROR_FR_FCI_DMAW_DATA	| \
				 FCHIP_ERROR_FR_FCI_PIOR_DATA	| \
				 FCHIP_ERROR_FR_FCI_LOAD_ADDRW	| \
				 FCHIP_ERROR_FR_FCI_UNKNOWN_CMND)




/*
 * The following are exported functions of the error handling module.
 * All take no arguments.
 * xxx_clear will clear the h/w error registers and the EVERROR struct.
 * xxx_log fills the EVERROR struct with h/w error reg values.
 * xxx_show print the error message derived from EVERROR.
 * xxx_fru isolates the broken module to be replaced (FRU) based on EVERROR.
 */

#ifdef _LANGUAGE_C

extern void everest_error_handler(eframe_t *, void *);
extern void everest_error_clear	(int);
extern void everest_error_log	(int);
extern void everest_error_show	(void);
extern void everest_error_fru	(eframe_t *);
extern void ev_perr(int, char *, ...);

extern void io4_error_show	(int slot);
extern void io4_error_log	(int slot);
extern void io4_error_clear	(int slot);

extern void mc3_error_clear	(int slot);

extern void cc_error_show	(int slot, int pcpuid);
extern int  cc_error_log	(void);
extern void cc_error_clear	(int);

extern void vmecc_error_clear	(int slot, int padap);
extern void vmecc_error_show	(int slot, int padap);
extern void vmecc_error_log	(int slot, int padap);
extern void io4hip_error_clear	(int slot, int padap);
extern void io4hip_error_show	(int slot, int padap);
extern void io4hip_error_log	(int slot, int padap);
extern void fchip_error_clear	(int slot, int padap);

extern volatile int in_everest_error_handler;
extern int everest_error_intr_reason;

extern void everest_error_intr	(void);
extern void everest_error_sendintr(int reason);

#if TFP
extern void tfp_clear_gparity_error(void);
#endif

#define EVERROR_INTR_CLEAR	1
#define EVERROR_INTR_PANIC	2

#endif /* _LANGUAGE_C */

#endif
