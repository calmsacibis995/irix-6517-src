/*
 *
 *	these structures define the linc address space
 *
 *	rev 1.0		2/12/96		cen
 */


struct linc_dma {
  int dtc;  int pad1;	/* */
  int dbma; int pad2;	/* */
  int dhpa; int pad3;	/* */
  int dlpa; int pad4;	/* */
  int drck; int pad5;	/* */
  int dck;  int pad6;	/* */
  int dcsr; int pad7;	/* */
  int ddwc; int pad8;	/* */

#define dma_transfer_control dtc
#define dma_bufmem_addr      dbma
#define dma_high_ppci_addr   dhpa
#define dma_low_ppci_addr    dlpa
#define dma_res_checksum     drck
#define dma_checksum         dck
#define dma_control_status   dcsr
#define dma_diag_wcount      ddwc

};

struct linc_misc_regs{
	int lcsr; int pad1;	/* */
	int cisr; int pad2;	/* */
	int cimr; int pad3;	/* */
	int hisr; int pad4;	/* */
	int himr; int pad5;	/* */
	int scsr; int pad6;	/* */
	int scea; int pad7[51];	/* */
	int icsr; int pad8;	/* */
	int iha; int pad9;	/* */
	int ila; int pad10;	/* */
	int iwdh; 		/* */
	int iwdl; 		/* */
	int irdp; int pad13[55];	/* */
	int ccsr; int pad14;	/* */
	int cerr; int pad15;	/* */
	int cea; int pad16;	/* */
	int cpcsr; int pad17[121];	/* */
	int mbb; int pad18[255];	/* */
	int bmc; int pad19;	/* */
	int pad33; int pad34;
	int bmo; int pad20;	/* */
	int bme; int pad21;	/* */
	int bmea; int pad22[1529];	/* */
	int led; int pad23;	/* */
	int gpio; int pad24[14340];	/* */
/* must reorder the remainder */
	int perr; int pad25;	/* */
	int peah; int pad26;	/* */
	int peal; int pad27;	/* */
	int pcvend; int pad28;	/* */
	int pccsr; int pad29;	/* */
	int pcrev; int pad30;	/* */
	int pchdr; int pad31;	/* */
	int pcbase; int pad32;	/* */
	};

struct linc{
	volatile long	sdram[8*1024*1024];	/* sdram space */
	char	rsvd1[4*1024*1024];	/* ssram/rsvd space */
	long	mbox[64][8*1024];	/* mailbox space */
	char    rsvd2[4*1024*1024];     /* rsvd space */
	int	cpci_conf[1*1024*1024];	/* cpci config space */
	int	cpci_mem[4*1024*1024];	/* cpci pio space (cast for diff sizes) */
	int	cpci_mem_swiz[4*1024*1024]; /* cpci swizzled pio space (cast for diff sizes) */
	struct linc_dma		dma0;		/* dma engine 0 */
	char	rsvd3[(4*1024*1024) - (sizeof(struct linc_dma))];
	struct linc_dma		dma1;		/* dma engine 1 */
	char	rsvd4[(4*1024*1024) - (sizeof(struct linc_dma))];
	struct linc_misc_regs	regs;	/* misc registers */
	char    rsvd5[4*1024*1024 - (sizeof(struct linc_misc_regs))];
	unsigned char prom[2*1024*1024];	/* prom  (cast for diff sizes) */
	unsigned char bbus[2*1024*1024];	/* bytebus (cast for diff sizes) */
	};
