/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Since the VME subsystem is in use by two different set of users, 
 * the device driver writers and the user level VME users, the
 * VME support files should try and avoid as much kernel data structure
 * dependency as possible.
 */
#ifndef __SYS_UNIV_VMESTR_H
#define __SYS_UNIV_VMESTR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/iograph.h>		/* IO graph vertex		    */
#include <sys/pio.h>			/* piomap_t, etc		    */
#include <sys/xtalk/xwidget.h>		/* Xtalk widget stuff		    */
#include <sys/PCI/pciio.h>
#include <sys/PCI/univ_vmereg.h>

/*
 * IO graph specific definitions for VME Bus 
 */
#define	EDGE_ROOT_VMEBUS	"vme"
extern	vertex_hdl_t		root_vmebus;


#define	VME_IRQMAX		8

/* This file defines the various data structures used to manage the
 * VME Bus subsystem provided via Newbridge's Universe  PCI<-> VME
 * Controller.
 */
#define	UNIV_VALID		0xFEEDDEAD

#define	SIMG_CNT		5	/* Count of Local slave images 	    */
#define	UNFIX_IMGINDX		0	/* Index of Unfixed slave image     */
#define	A32ADD_IMGINDX		1	/* Additional A32 Slave image 	    */
#define	A32N_IMGINDX		2	/* A32 NonPrivelege Slave Image     */
#define	A32S_IMGINDX		3	/* A32 Supervisory Slave Image	    */
#define	A64_IMGINDX		4	/* A64 Slave Image	    	    */
#define	SLSI_IMGINDX		5	/* A16/24 NP/Sup Slave Image	    */
#define	MAX_IMGINDX		(SLSI_IMGINDX+1)

#define	UNPCI_UNFIXSZ		(128*1024)	/* 128k space for unfixed map */
/* Few Macros for PCI Slave image */
/*
 * XXX: 256MB is a good number in N mode on SN0. This number should be
 *      512MB in M mode. Keep this in mind.
 */
#define	UNPCI_A32S_IMGSZ		(256*1024*1024)
#define	UNPCI_A32N_IMGSZ		(256*1024*1024)

/* 
 * 256 Mbytes of space for A64 mapping. 
 */
#define	UNPCI_A64_IMGSZ		(256*1024*1024)

/*
 * 64 MBytes space for Special local slave image
 */
#define	UNPCI_SPLSLVSZ		(64*1024*1024)

/* Special Slave Image organization, and related Macros 	*/
/*
 * This's the way the 4 quadrant A16/A24 space would look.
 *
pci_vbase ------------> ________________________________
                        |                               |
                        |                               |
                        |       A24 NP, DATA, 32bit     |
                        |                               |
pci_vbase+0xff0000      |_______________________________|
                        |                               |
                        |       A16 NP, DATA, 32bit     |
pci_vbase+0x1000000     |_______________________________|
                        |                               |
                        |                               |
                        |       A24 SP, DATA, 32bit     |
                        |                               |
pci_vbase+0x1ff0000     |_______________________________|
                        |                               |
                        |       A16 SP, DATA, 32bit     |
pci_vbase+0x2000000     |_______________________________|
                        |                               |
                        |                               |
                        |       A24 NP, DATA, 16bit     |
                        |                               |
pci_vbase+0x2ff0000     |_______________________________|
                        |                               |
                        |       A16 NP, DATA, 16bit     |
pci_vbase+0x3000000     |_______________________________|
                        |                               |
                        |                               |
                        |       A24 SP, DATA, 16bit     |
                        |                               |
pci_vbase+0x3ff0000     |_______________________________|
                        |                               |
                        |       A16 SP, DATA, 16bit     |
pci_vbase+0x4000000     |_______________________________|
 *
 *
 */

#define A24NP_DAT32_BS		0
#define A24NP_DAT32_BD		0x0FF0000

#define A24SP_DAT32_BS		0x1000000
#define A24SP_DAT32_BD		0x1FF0000

#define A24NP_DAT16_BS		0x2000000
#define A24NP_DAT16_BD		0x2FF0000

#define A24SP_DAT16_BS		0x3000000
#define A24SP_DAT16_BD		0x3FF0000

#define A16NP_DAT32_BS		0x0FF0000
#define A16NP_DAT32_BD		0x1000000

#define A16SP_DAT32_BS		0x1FF0000
#define A16SP_DAT32_BD		0x2000000

#define A16NP_DAT16_BS		0x2FF0000
#define A16NP_DAT16_BD		0x3000000

#define A16SP_DAT16_BS		0x3FF0000
#define A16SP_DAT16_BD		0x4000000

#define	PIO_A16START		0
#define	PIO_A16END		0x10000

#define	PIO_A24START		0x0800000
#define	PIO_A24END		(0x1000000 - PIO_A16END)

#define	PIO_A32START		0x0
#define	PIO_A32END		0x80000000

#define VALID_VME(addr,sz,st,end)       (((__int64_t)addr >= st) && ((addr+sz) <= end))

/* VME DMA address range defintion	*/
#define VMEDMA_CTL	(VMEIMG_CTL_ENABLE | \
                        VMEIMG_CTL_PWENBL| /* Posted write enable */ 	\
                        VMEIMG_CTL_PREN   | /* Prefetch Enable */ 	\
                        VMEPGM_MODE_BOTH  | /* Program and data mode*/ 	\
			VMESUP_MODE_BOTH  | /* Sup/NP Mode */ 		\
                        VMEIMG_CTL_LD64EN /* Enable 64bit Local bus */ 	\
			)

/* A24 Specific DMA Definition		*/
#define DMAVME_A24BS		0x0	   /* DMA Base address on VME for A24 */
#define DMAVME_A24BD		0x800000   /* DMA Upper address on VME Bus    */
#define DMAPCI_A24LTO		0x7F800000 /* VME Base xlates to this on PCI  */
#define DMAPCI_A24UTO		0          /* Bits 32-52 on PCI Bus 	      */
#define DMAVME_A24CTL		(VMEDMA_CTL|(VMEVAS_A24 << DMA_CTL_VAS_SHFT))

/* A32 Specific DMA Definitions		*/
#define DMAVME_A32BS		0x80000000 /* DMA Base address on VME for A32 */
#define DMAVME_A32BD		0xBF800000 /* DMA Bound address on VME Bus    */
#define DMAPCI_A32LTO		0x40000000 /* VME Bus xlates to this on PCI   */
#define DMAPCI_A32UTO		0
#define DMAVME_A32CTL		(VMEDMA_CTL|(VMEVAS_A32 << DMA_CTL_VAS_SHFT))

/* A64 Specific DMA Definitions		*/
#define	DMAVME_A64BS		0x10000000 /* DMA Base address on VME for A64 */
#define	DMAVME_A64BD		0x10000100 /* DMA Bound address on VME Bus    */
#define	DMAPCI_A64LTO		0x10000000 /* VME Base xlates to this on PCI  */
#define	DMAPCI_A64UTO		0x10000000 /* DMA Base address on VME for A64 */
#define DMAVME_A64CTL		(VMEDMA_CTL|(VMEVAS_A64 << DMA_CTL_VAS_SHFT))

/* Interrupt Specific Defintions	*/
#define	UVINT_LVLMAX		8		/* MAX interrupt levels used */
#define	UVINT_ERRLVL		0		/* PCI Intr lvl for Errors */
#define	UVINT_DMALVL		1		/* PCI Intr lvl for DMA eng */
#define	UNVME_ERRINT_DEST	(cpuid_t)0	/* CPU handling Error Intr */
#define	UNVME_DMAINT_DEST	(cpuid_t)0	/* CPU handling DMA  Intr  */



typedef struct univ_pci_slvimg {
	__int32_t	pci_numusers;	/* User count 			    */
	__uint32_t  	pci_ctrl ;	/* Control Register 		    */
	__uint32_t	pci_base;	/* Base address of this image 	    */
	__uint32_t	pci_bound;	/* highest address decoded for this */
	__uint32_t 	pci_lto;	/* Translation offset to VME 0:31   */
	__uint32_t	pci_uto;	/* Translation offset to VME 32:52  */
	caddr_t		pci_vbase;	/* Virtual address to access this */
} un_pcisimg_t;


typedef struct univ_slsimg {
	__int32_t	pci_numusers;	/* User count 			    */
	__uint32_t	slsi_base;	/* PCI Base address		    */
	__uint32_t	slsi_regval;	/* PCI SLSI Register Value	    */
	caddr_t		slsi_vbase;	/* CPU addressible base address     */
} un_pcislsi_t;

typedef struct univ_vme_slvimg {
	__uint32_t  	vme_ctrl ;	/* Control Register 		     */
	__uint32_t	vme_base;	/* Base address of this image 	     */
	__uint32_t	vme_bound;	/* highest address decoded for this  */
	__uint32_t 	vme_lto;	/* Translation offset to PCI 0:31    */
	__uint32_t	vme_uto;	/* Translation offset to PCI 32:52   */
} un_vmesimg_t;

/* This structure would be primarily used by the users who need to 
 * use the compare and swap support provided by Universe. 
 */
typedef struct univ_splcyc_reg {
	__uint32_t	scyc_ctl;	/* Control values of spl cycle 	     */
	__uint32_t	scyc_laddr;	/* Local bus address for spl cycle   */
	__uint32_t	scyc_enabl;	/* Enable bits for spl cycle 	     */
	__uint32_t	scyc_cmpdata;	/* Data to compare  		     */
	__uint32_t	scyc_swapdata;	/* Data to be written back 	     */
} un_splcyc_t;

/* 
 * DMA Structure for Command List. 
 * Should this go in a different file, since it may be used by the 
 * User level programs also ??
 */
typedef struct univ_dmastr {
	__uint32_t	dma_ctl;	/* Dma Control Parameters. 	*/
	__uint32_t	dma_xfercnt;	/* DMA transfer Count 		*/
	__uint32_t	dma_pci_laddr;	/* DMA PCI Bus address 0:31 	*/
	__uint32_t	dma_pci_uaddr;	/* DMA PCI Bus address 32:63 	*/
	__uint32_t	dma_vme_laddr;	/* DMA VME Bus address 32:63 	*/
	__uint32_t	dma_vme_uaddr;	/* DMA VME Bus address 32:63 	*/
	__uint32_t	dma_lcmdpp;	/* DMA Low Command pkt Ptr 	*/
	__uint32_t	dma_ucmdpp;	/* DMA High Command Pkt Ptr 	*/
	/* DMA GCSR is not needed as part of the structure that would be
	 * read by the hardware for linked list DMA. Need to take it out,
	 * after confirming that.
	 */
	__uint32_t	dma_gcsr;	/* DMA Global Cmd/stat reg 	*/
} univ_usrdma_t;

/* Values for dma_flags */
#define	UNDMA_ACTIVE		1	/* DMA Currently active */
#define	UNDMA_CHMODE		2	/* DMA In Chain Mode */

/*
 * Universe Error Logging register format. This has the fields for 
 * both PCI and VME Error logging Registers. 
 */
typedef struct univ_errlogreg{
	__uint32_t	un_lcmderr;	/* Command Error Log */
	__uint32_t	un_pci_laddr;	/* PCI Low address bits on Error    */
	__uint32_t	un_pci_uaddr;	/* PCI Hi  address bits on Error    */
	__uint32_t	un_vme_am;	/* VME AM value during Error 	    */
	__uint32_t	un_vme_laddr;	/* VME Low address Register 	    */
	__uint32_t	un_vme_uaddr;	/* VME Upper address Register 	    */
} univ_errlog_t;


/* 
 * One of the following structures would exist for each Universe
 * Controller found in the system.
 * This structure primarily has image of those registers, which have to
 * be modified based on what was previously stored there. 
 * It also has few more fields, which provide additional information 
 * for debugging (For e.g, Given a PCI address, we can check if there is
 * any slave image mapped to that.
 */
typedef struct v_univ_controller_s {
	__uint32_t	un_valid;	/* = UNIV_VALID when valid         */
	vertex_hdl_t	un_vertex;	/* VME Bus Vertex		   */
	vertex_hdl_t	un_pcidev;	/* PCI dev vertex passed from top  */

	/* Additional vertexes supported under Universe */
	vertex_hdl_t	un_a16sv;	/* Vertex for A16 S space	   */
	vertex_hdl_t	un_a16nv;	/* Vertex for A16 N space	   */
	vertex_hdl_t	un_a24sv;	/* Vertex for A24 S space	   */
	vertex_hdl_t	un_a24nv;	/* Vertex for A24 N space	   */
	vertex_hdl_t	un_a32sv;	/* Vertex for A32 S space	   */
	vertex_hdl_t	un_a32nv;	/* Vertex for A32 N space	   */
	vertex_hdl_t	un_a64  ;	/* Vertex for A64   space	   */

	lock_t 		un_mutex;	/* Spin lock to for MP purpose	   */ 

	/* Generic per controller information */
	uchar_t		un_iadapno;	/* Internal Adapter number 	   */
	uchar_t		un_xadapno;	/* External Adapter number 	   */
	caddr_t		un_piobase;	/* Address used by CPU for PIOs.   */
	size_t		un_piosize; 	/* Size of PIO space allocated	   */

	/* Replace appropriate structure name */

	/* PCI Specific Register values		*/
	/* Although some of these registers are Read only, their values
	 * will be read and retained here for the purpose of 
	 * dubuggability from core dumps
	 */
	__uint32_t	un_pciid;	/* PCI ID-> Dev-ID, Vend-ID	   */
	__uint32_t	un_pcicsr;	/* Universe PCI Control/Status reg */
	iopaddr_t	un_configaddr;	/* Universe PCI Configuration addr */
	iopaddr_t	un_baseaddr;	/* Universe PCI base address	   */

	__uint32_t	un_confctl;	/* Universe Config Control Reg     */
	__uint32_t	un_confclass;	/* Universe Config Class Reg       */

	/* Slave (Local and VME) Image specific Registers */

	un_pcislsi_t	un_slsimg;	   /* Special PCI Slave image reg   */
	un_pcisimg_t	un_pcisimg[SIMG_CNT]; /* PCI Slave image regs       */
	un_vmesimg_t	un_vmesimg[SIMG_CNT]; /* VME Slave image regs       */
	lock_t 		un_unfixlock;	/* Lock to access floating image    */

	pciio_piomap_t	un_piomap[MAX_IMGINDX];/* PIO Maps for all images  */

	__uint32_t	un_dmaflag;	/* Status of DMA Usage (in use/not) */
	iopaddr_t	un_dmallstart;	/* Pointer to DMA linked list chain */
	__uint32_t	un_dmagcsr;	/* Copy of DMA Control/status reg   */

	/* Interrupt specific Information */
	uchar_t		un_intbmap;	/* bit map of int levels used	    */
	__uint32_t	un_lclint_enbl;	/* Local Interrupt enable mask	    */
	__uint32_t	un_lclint_map0;	/* Reg to map VME intr to PCI       */
	__uint32_t	un_lclint_map1; /* Reg to map other intr to PCI     */
	__uint32_t	un_swint_stat;  /* Status/ID for Sw generated intr  */

	/* Univere Control Register */
	__uint32_t	un_pcimisc;	/* PCI Miscellaneous control reg    */
	__uint32_t	un_mastctl;	/* Master Control Register          */
	__uint32_t	un_miscctl;	/* Miscellaneous Control Register   */
	__uint32_t	un_miscstat;	/* MIscellaneous Status Register    */
	__uint32_t	un_useram;	/* User AM codes 		    */

	/* Error Logging Registers */
	univ_errlog_t	un_errlog;	/* Error Logging Register data 	    */

	/* Software specific Information	*/
	int		un_irqmask;	/* Mask indicating IRQs to be enabled */
	pciio_intr_t	un_pciintr[UVINT_LVLMAX]; /*  Interrupt levels used */
	cpuid_t		un_vmeipl[VME_IRQMAX];

	struct	v_univ_controller_s	*un_next;/* Pointer to next in List  */

} v_unvme_t;

/* Generic Register Get and Set Macros 	*/
#define	UN_GET_REG(w, r) 	*(__uint32_t *)((w) + (r))
#define	UN_SET_REG(w, r, v)	*(__uint32_t *)((w) + (r)) = (v)

#define	LOCK_UNIV(u)		mutex_spinlock(&u->un_mutex)
#define	UNLOCK_UNIV(u,s)	mutex_spinunlock(&u->un_mutex, s)

#define	FLOCK_UNIV(u)		mutex_spinlock(&u->un_unfixlock)
#define	FUNLOCK_UNIV(u,s)	mutex_spinunlock(&u->un_unfixlock, s)

extern void pio_mapfree_vme(piomap_t *);
extern void pio_maplock_vme(piomap_t *);
extern void pio_mapunlock_vme(piomap_t *);
extern int  pio_mapfix_vme(piomap_t *);
extern ulong pio_map_vme(piomap_t *, iopaddr_t, int);
extern int  unvme_xtoiadap(int);

extern int  unvme_pio_geth(piomap_t *, int, int, int, iopaddr_t, int);
extern int  unvme_dma_geth(dmamap_t *, int, int, int, int, int, int);

extern int pio_use_unfix(v_unvme_t *, piomap_t *, iopaddr_t, ulong);
extern int pio_chk_unfix(v_unvme_t *, piomap_t *, iopaddr_t, ulong);

extern void unvme_graph_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_UNIV_VMESTR_H */
