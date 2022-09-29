static char *SccsId = "1.13";
/*
* Copyright 1998 - PsiTech, Inc.
* THIS DOCUMENT CONTAINS CONFIDENTIAL AND PROPRIETARY
* INFORMATION OF PSITECH.  Its receipt or possession does
* not create any right to reproduce, disclose its contents,
* or to manufacture, use or sell anything it may describe.
* Reproduction, disclosure, or use without specific written
* authorization of Psitech is strictly prohibited.
*
* 
* 3/16/98	PAG	created from rad4drv.c
*/
#define IRIX65	/* only build this for 6.5 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/unistd.h> 
#include <sys/mman.h>
#include <sys/invent.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/immu.h>
#include <sys/sema.h>
#include <sys/ksynch.h>
#include <sys/ddi.h>
#include <sys/kopt.h>	/* for arg_console & arg_gfx */
#ifdef IRIX63
void ddi_sv_wait(sv_t *svp, void *lkp, int rv);
#endif /*  IRIX63 */
#include <sys/kmem.h>
#include <stropts.h>
#include <sys/termio.h>
#include <sys/stermio.h>
#ifdef IRIX63
#include <sys/edt.h>
#include <sys/user.h>
#endif /*  IRIX63 */

#ifndef IRIX63
#include <sys/hwgraph.h>
#endif /*  IRIX63 */
#include <sys/PCI/pciio.h>
#include <sys/PCI/PCI_defs.h>
#ifdef IRIX65
#include <ksys/ddmap.h> /* */
#else /* IRIX65 */
#include <sys/region.h> /* */
#endif /* IRIX65 */
#include <sys/kmem.h>
#include "sys/htport.h"

#include "rad1hw.h"
#include "rad1.h"
#include "rad4pci.h"
#include "rad1sgidrv.h"

#define USING_HARDWARE_MASK /* */
#define USE_PER2_DMA /* do dma xfers */
#define RAD1_TEXTPORTONLY /* only work as the textport interface */
/*#define VB_LUT_WID_LOAD * load lut and wid in the v blank int */
/* #define FORCE_NO_DMATRANS	* use dmatrans or pio */
/* #define FORCE_NO_DMAMAP * use dmamap or pio */
#define ALLOC_TBUF /* if not defined ,tbuf is staticaly allocated */

#define	FALSE 0
#define	TRUE 1
#define RAD1_PFX "rad1pci"
#define PER2_FLAGS 0

#define PER2_MIN_DMA_TOTAL	32	/* words, less is faster to do pio */
/* measured on a o200 would be more like 2000 as cost to set up dma */

#define DMA_TIMEOUT_US	400000
#define VB_TIMEOUT_US	400000	/* 25 v blanks */
#define DAEMON_STALL_USEC	50000
#define DAEMON_STALL_LOOPS	5

#if defined (IRIX63)
#define PCI_CFG_BASE(c)        pciio_piotrans_addr(c,0,PCIIO_SPACE_CFG,0,256,0)
#define PCI_CFG_GET(c,b,o,t)   pciio_config_get(b,o)
#define PCI_CFG_SET(c,b,o,t,v) pciio_config_set(b,o,v)
#elif defined (IRIX64)	/* */
#	define PCIIO_PIOMAP_BIGEND PCIIO_PIO_BIGEND
#	define PCIIO_DMAMAP_BIGEND PCIIO_DMA_BIGEND
#define PCI_CFG_BASE(c)        pciio_piotrans_addr(c,0,PCIIO_SPACE_CFG,0,256,0)
#define PCI_CFG_GET(c,b,o,t)   ((*(t *)((char *)(b)+((o)&~3))))
#define PCI_CFG_SET(c,b,o,t,v) ((*(t *)((char *)(b)+((o)&~3))) = v)
#else /* IRIX 6.5 or higher i hope */
#define PCI_CFG_BASE(c)  NULL
#define PCI_CFG_GET(c,b,o,t)   pciio_config_get(c,o,sizeof(t))
#define PCI_CFG_SET(c,b,o,t,v) pciio_config_set(c,o,sizeof(t),v)
#endif /*  defined (IRIX64)*/

extern int maxdmasz;

int rad1pcidevflag = D_MP/* | D_WBACK*/;

static int number_of_per2s = 0;
static cfg_reg *halt_cfg_ptrs[MAX_RAD1S];/*used by halt to disable the boards*/
static int *halt_ram_ptrs[MAX_RAD1S];/*used by halt to disable the boards*/
static int found_per2 = FALSE;
static int per2_opened = 0;
#ifdef ALLOC_TBUF
static int *tbuf = NULL;
#else /*  ALLOC_TBUF */
static int tbuf[MAX_RAD1S * RAD1_SGRAM_SIZE/4];
#endif /*  ALLOC_TBUF */
static int bigx_x, bigx_y;
static per2_info *bigx_p2s[MAX_RAD1S];
static int pciio_dma_flags;
static struct
{
	int max, min, d_count, count;
} to_info;
void *rad1_info_pointer;
int *rad1_earlyinit_base = 0;

void rad1_to_func(sv_t *waitid, per2_info *p2);
static void rect_dma_move(per2_info *p2, rad4_rect_write *rw, int direction);
static void rect_pio_move(per2_info *p2, rad4_rect_write *rw, int direction);
static void rect_bigx_move(per2_info *p2, rad4_rect_write *rw, int direction);
static void rect_pio_fill(per2_info *p2, rad4_rect_fill *rf);
static void rect_patch_fill(per2_info *p2, rad4_rect_fill *rf);
static void rect_bigx_fill(per2_info *p2, rad4_rect_fill *rf);
static void load_wid( per2_info *p2, int cmd, int startaddr, int id, int mask);
static void rect_patch_copy(per2_info *p2, rad4_rect_copy *rw);
static int rad1_dma_patch_move(per2_info *p2, int dma_ptr, int startx, int starty, int sizex, int sizey, int direction, int mask, RadColorMode mode,int stride);
static int rad1_pio_move(per2_info *p2,uio_t *uio, uio_rw_t direction);
#ifdef RAD1_CONSOLE
int rad1_native_kybd_in(unsigned char data);
void rad1_tty_init(per2_info *p2);
void rad1_tty_close(per2_info *p2);
int rad1_read_tty(per2_info *p2, uio_t *uio);
int rad1_native_kybd_in(unsigned char data);
int rad1_write_tty(per2_info *p2, uio_t *uio);
void rad1_tty_fill_termio(struct termio *term_io_struct);
void rad1_tty_fill_termios(struct termios *term_io_struct);
void rad1_tty_fill_old_termio(struct __old_termio *term_io_struct);
void rad1_tty_fill_old_termios(struct __old_termios *term_io_struct);
void rad1_tty_set_termio(struct termio *term_io_struct, int cmd);
void rad1_tty_set_termios(struct termios *term_io_struct, int cmd);
void rad1_tty_set_old_termios(struct __old_termios *term_io_struct, int cmd);
void rad1_tty_fill_winsize(struct winsize *winsize_struct);
void rad1_tty_flush(int arg);
ulong_t rad1_tty_get_pgrp(void);
void rad1_tty_set_pgrp(ulong_t *pgrp);
ulong_t rad1_tty_get_psid(void);
void rad1_tty_set_psid(ulong_t *psid);
#endif /*  RAD1_CONSOLE */
static void daemon(per2_info * p2,int arg);
static void bigx_update(per2_info *p2, int x, int y, int w, int h, int direction);
static void fill_fb(per2_info *p2);
int program_eeprom_sector(per2_info *p2, unsigned int offset, unsigned char *buffer);
static void dump_per2v(per2_info *p2);

extern struct htp_fncs RAD1_htp_fncs;

#ifdef IRIX63
void rad1_inthandler(eframe_t *ef, per2_info *p2)
#else /* IRIX63 */
void rad1_inthandler(per2_info *p2, per2_info *p2_o2)
#endif  /* IRIX63 */
{
	int count, status;
	int i, temp;

#ifndef IRIX63
	if (p2 == 0) p2 = p2_o2;	/* workaround for sgi bug # 630965 */	
#endif  /* IRIX63 */
	
	if ((status = *(p2->registers + INTFLAGS_REG/4)) == 0)
		return;	/* it's not us */
	*(p2->registers + INTFLAGS_REG/4) = status;	/* clear interrupts */
	INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock */
	*(p2->registers + INTENABLE_REG/4) = 0; /* turn interrupts off */
	count = 0;
	while(count < 200) /* let's not get stuck forever */
	{
		count++;
		if ((status & SYNC_INT_FLAG) != 0)	/* synchronization interrupt? */
		{ 
			temp = *(p2->registers + OUT_FIFO/4) & ~SYNC_INT_EN;
			SV_SIGNAL(p2->sync_wait_on_it);	/* */
			untimeout(p2->sync_to_cookie); /* stop timeout */
		}

		if ((status & DMA_INT_FLAG) != 0)	/* DMA interrupt? */
		{ 
/* read or write? */
			SV_SIGNAL(p2->dma_rd_wait_on_it);
/*			untimeout(p2->dma_to_cookie); * stop timeout */
		}

		if ((status & BYPASS_DMA_INT_FLAG) != 0)	/* bypass DMA interrupt? */
		{ 
			SV_SIGNAL(p2->dma_wr_wait_on_it);
/*			untimeout(p2->dma_to_cookie); * stop timeout */
		}

#ifdef VB_LUT_WID_LOAD
		if ((status & VERT_RETRACE_INT_FLAG) != 0)	/* vertical retrace interrupt? */
		{
			p2->p2_intenable.bits.vert_retrace_int_en = RAD1_DISABLE;
				/* disable vertical retrace interrupt */
			if(p2->lut_flag)
			{
/*				rad1_load_lut(p2);	* */
				p2->lut_flag = FALSE;
				untimeout(p2->lut_to_cookie);
			}

/*			if(p2->wid_flag)
				SV_SIGNAL(p2->wid_wait_on_it);	* */
		}
#endif /* VB_LUT_WID_LOAD */
		continue; /* */
	}

	*(p2->registers + INTENABLE_REG/4) = p2->p2_intenable.all;
		/* re-enable interrupts */
	INT_LOCK_UNLOCK(&p2->per2_global_lock);	/* release the global lock */
}

void rad1pcihalt(void)
{
	/* cmn_err(CE_DEBUG,"RAD1 halt called\n"); * */
	while (number_of_per2s--)
	{
		/* disable the board */
	}
}

int rad1pciattach(vertex_hdl_t in_vtx)
{
	vertex_hdl_t vtx, vtx_bigx, vtx_tty, vtx_tty1;
	per2_info *p2;
	per2serial_info *p2bigx;
	per2serial_info *p2tty;
	char dev_name[32];
	char devt_name[32];
	int i, mapii;
	int ret;
	pciio_info_t pciinfo;
	pciio_slot_t slot = 0;
	int rc, temp;
	pciio_piomap_t cfg_pio_map, mem1_pio_map, mem2_pio_map, registers_pio_map;
	int *registers;
	int console_flag = 0;
	
/* 	cmn_err(CE_DEBUG,"in per2 attach\n"); * */

	if(number_of_per2s >= MAX_RAD1S) /* no more then MAX_RAD1S per2s please */
		return ENOMEM;

	p2 = (per2_info *)kmem_zalloc(sizeof(per2_info),KM_SLEEP);
	if(p2 == 0)
		return ENOMEM;

#ifdef ALLOC_TBUF
	if(tbuf == 0)
	{
		tbuf = (int *)kmem_zalloc(RAD1_SGRAM_SIZE,
				KM_SLEEP|KM_PHYSCONTIG|KM_CACHEALIGN); /* */
	}
	else
	{	kmem_free(tbuf,number_of_per2s * RAD1_SGRAM_SIZE);
		tbuf = (int *)kmem_zalloc((number_of_per2s+1) * RAD1_SGRAM_SIZE, 
				KM_SLEEP|KM_PHYSCONTIG|KM_CACHEALIGN); /* */
	}
	if(tbuf == 0)
	{	cmn_err(CE_DEBUG,"out of kmem on tbuf alloc for per2 %d\n",
			number_of_per2s+1);
		return ENOMEM;
	}
#endif /* ALLOC_TBUF */
	bigx_p2s[number_of_per2s] = p2;
	for(i=0;i<=number_of_per2s;i++)
		bigx_p2s[i]->tbuf = tbuf + (i*RAD1_SGRAM_SIZE/4);


	p2->board_number = number_of_per2s;
	p2->dma_enabled = !rad1pci_no_dma;
/*	if(rad1pci_max_dma_area < 0x2000)	* */
	if(rad1pci_max_dma_area < 0x80) /* pci cache line */
	{
		cmn_err(CE_DEBUG,
			"rad1pci_max_dma_area too small (0x%x). set to 0x80\n",
			rad1pci_max_dma_area);
		rad1pci_max_dma_area = 0x80;
	}

	p2->dev_desc.intr_swlevel = (ilvl_t)plhi; /* */

	INT_LOCK_INIT(&p2->per2_global_lock);
	SLP_LOCK_INIT(&p2->hardware_mutex_lock);
	SLP_LOCK_INIT(&p2->vblank_mutex_lock);
	SLP_LOCK_INIT(&p2->bigx_mutex_lock);
	SLP_LOCK_INIT(&p2->mask_mutex_lock);
	SLP_LOCK_INIT(&p2->dma_sleep_lock);
	p2->sync_wait_on_it = SV_ALLOC(SV_DEFAULT, KM_SLEEP, "GP sync");
	p2->dma_wr_wait_on_it = SV_ALLOC(SV_DEFAULT, KM_SLEEP, "dma write");
	p2->dma_rd_wait_on_it = SV_ALLOC(SV_DEFAULT, KM_SLEEP, "dma read");
	p2->lut_wait_on_it = SV_ALLOC(SV_DEFAULT, KM_SLEEP, "lut load");
	p2->wid_wait_on_it = SV_ALLOC(SV_DEFAULT, KM_SLEEP, "wid load");

#if !defined (IRIX63)
 	cfg_pio_map = pciio_piomap_alloc(in_vtx, NULL, PCIIO_SPACE_CFG,
		(iopaddr_t)0, (size_t)RAD1_CFG_SIZE,
		(size_t)RAD1_CFG_SIZE, 0); /* default endian */
#endif /*  IRIX63 */
 	registers_pio_map = pciio_piomap_alloc(in_vtx, &(p2->dev_desc),
		PCIIO_SPACE_WIN(0), (iopaddr_t)0, (size_t)RAD1_REGION_0_SIZE,
		(size_t)RAD1_REGION_0_SIZE, 0); /* default endian */

	p2->dma_map = pciio_dmamap_alloc(in_vtx,&(p2->dev_desc),
		(rad1pci_max_dma_area>0x2000)?rad1pci_max_dma_area:0x2000,
		pciio_dma_flags); /* map at least 8k */
	if(p2->dma_map == NULL)
	{	cmn_err(CE_DEBUG,
			"RAD1: pciio_dmamap_alloc failed, rad1pci_max_dma_area too big?\n");
	}
#ifdef IRIX63
	p2->cfg_ptr = (cfg_reg *) pciio_piotrans_addr(in_vtx, &p2->dev_desc,
		PCIIO_SPACE_CFG, (iopaddr_t)0, RAD1_CFG_SIZE, PCIIO_PIOMAP_BIGEND); /* */
/*	cmn_err(CE_DEBUG,"ftr pciio_piotrans_addr returned 0x%x\n",p2->cfg_ptr); *  */
# else /*  IRIX63 */
	p2->cfg_ptr = (cfg_reg *)pciio_piomap_addr(cfg_pio_map,
		(iopaddr_t)0, RAD1_CFG_SIZE);
	if(p2->cfg_ptr == 0)
	{	cmn_err(CE_DEBUG,"pciio_piomap_addr for cfg returned 0\n");
		return EIO;
	}
# endif /*  IRIX63 */

	/* map the card's address spaces */
	p2->registers = (int *)pciio_piomap_addr(registers_pio_map,
		(iopaddr_t)0, RAD1_REGION_0_SIZE);
	if(p2->registers == 0)
	{
		cmn_err(CE_DEBUG,"pciio_piomap_addr for rtr returned 0\n");
		return EIO;
	}
	registers = p2->registers;

	p2->ram1 = (int *)pciio_piotrans_addr(in_vtx, &(p2->dev_desc),
			PCIIO_SPACE_WIN(1), (iopaddr_t)0, (size_t)RAD1_REGION_1_SIZE, 0);
	if(p2->ram1 == NULL)
	{
	 	mem1_pio_map = pciio_piomap_alloc(in_vtx,&(p2->dev_desc), PCIIO_SPACE_WIN(1),
			(iopaddr_t)0, (size_t)RAD1_REGION_1_SIZE,
			(size_t)RAD1_REGION_1_SIZE, 0); /* default endian */
		p2->ram1 = (int *)pciio_piomap_addr(mem1_pio_map,
			(iopaddr_t)0, RAD1_REGION_1_SIZE);
	}
	if(p2->ram1 == NULL)
	{
		cmn_err(CE_DEBUG,"pciio_piomap_addr for mem1 returned 0\n");
		return EIO;
	}

	/* map aperture 2 to allow access to EEPROM */
	p2->ram2 = (int *)pciio_piotrans_addr(in_vtx, &(p2->dev_desc),
			PCIIO_SPACE_WIN(2), (iopaddr_t)0, (size_t)RAD1_EPROM_SIZE, 0);
	if(p2->ram2 == NULL)
	{
	 	mem2_pio_map = pciio_piomap_alloc(in_vtx,&(p2->dev_desc), PCIIO_SPACE_WIN(2),
			(iopaddr_t)0, (size_t)RAD1_EPROM_SIZE,
			(size_t)RAD1_EPROM_SIZE, 0); /* default endian */
		p2->ram2 = (int *)pciio_piomap_addr(mem2_pio_map,
			(iopaddr_t)0, RAD1_EPROM_SIZE);
	}
	if(p2->ram2 == NULL)
	{
		cmn_err(CE_DEBUG,"pciio_piomap_addr for mem2 returned 0\n");
		return EIO;
	}

#ifdef IRIX63
	/* enable the memory space and the master access */
	temp = PCI_CFG_GET(in_vtx, p2->cfg_ptr, PCI_CFG_COMMAND); /* get the cmd register */
	PCI_CFG_SET(in_vtx, p2->cfg_ptr, PCI_CFG_COMMAND, short, temp | 0x6);
		/* set the command register */
#endif  /*  IRIX63 */

 	p2->vendor_id = PCI_CFG_GET(in_vtx, p2->cfg_ptr, PCI_CFG_VENDOR_ID, short) & 0xFFFF;
#ifdef IRIX64
 	p2->device_id = PCI_CFG_GET(in_vtx, p2->cfg_ptr, PCI_CFG_DEVICE_ID, int) >> 16;
 	p2->revision = PCI_CFG_GET(in_vtx, p2->cfg_ptr, PCI_CFG_REV_ID, int) & 0xFF;
#else /* IRIX64 */
 	p2->device_id = PCI_CFG_GET(in_vtx, p2->cfg_ptr, PCI_CFG_DEVICE_ID, short);
 	p2->revision = PCI_CFG_GET(in_vtx, p2->cfg_ptr, PCI_CFG_REV_ID, char);
#endif /* IRIX64 */
#if 0
	temp = PCI_CFG_GET(in_vtx, p2->cfg_ptr, PCI_CFG_SUBSYS_VEND_ID,short);
			cmn_err(CE_DEBUG,"rad1drv.c ssvid = 0x%x\n",temp);

	temp = PCI_CFG_GET(in_vtx, p2->cfg_ptr, PCI_CFG_SUBSYS_ID,short);
			cmn_err(CE_DEBUG,"rad1drv.c ssid = 0x%x\n",temp);
#endif /* 0 */

	/* the next two lines go into syslog */
	halt_cfg_ptrs[number_of_per2s] = p2->cfg_ptr;	/*save this for the halt entry*/
	halt_ram_ptrs[number_of_per2s] = p2->ram1;	/*save this for the halt entry*/

	if(number_of_per2s == 0)	/* report the driver version on the first board */
		if(SccsId[0] == '%')
			cmn_err(CE_DEBUG,"RAD1 test driver %s %s\n", __DATE__, __TIME__);
		else
			cmn_err(CE_DEBUG,"RAD1 driver %s\n",SccsId);	/* these go into syslog */
 	cmn_err(CE_DEBUG,"RAD1 %d attached rev 0x%x\n",
			p2->board_number, p2->revision);
	if(SccsId[0] == '%')
	{
		cmn_err(CE_DEBUG,"RAD1 dma %s\n", p2->dma_enabled?"enabled":"disabled");
		cmn_err(CE_DEBUG,"RAD1 dma max size 0x%x bytes\n", rad1pci_max_dma_area);
		if(p2->dma_map == NULL)
			cmn_err(CE_DEBUG,"RAD1 dma_map NULL\n");
#ifdef FORCE_NO_DMATRANS
		cmn_err(CE_DEBUG,"RAD1 TEST pciio_dmatrans_addr not used\n");
#endif /* FORCE_NO_DMATRANS */
#ifdef FORCE_NO_DMAMAP
		cmn_err(CE_DEBUG,"RAD1 TEST pciio_dmamap_addr not used\n");
#endif /* FORCE_NO_DMAMAP */
	}

	found_per2 = TRUE;
 
    /* Setup All the valid modes. */
    p2->NumValidModes = 0;
 
    for (i=0; i < NumPER2Modes; i++)
    {
      PER2Modes[i].modeInformation.ModeIndex = i;
      p2->NumValidModes++;
    }
	
	/* initialize chip */
	if (*(registers + VCLKCTL_REG/4) != 0)
		console_flag = 1;	/* console was invoked */
	rad1_pinit(p2, console_flag);

	if (!console_flag)
	{
		/* clear the screen */
		fill_fb(p2);
	}
	p2->p2_videocontrol.bits.enable = RAD1_ENABLE;
	*(registers + VIDEOCONTROL_REG/4) = p2->p2_videocontrol.all;

	p2->bigx_daemon_stall = FALSE;
	p2->bigx_cookie = 0;
	bigx_x = (number_of_per2s + 1) * p2->v_info.x_size;
	bigx_y = p2->v_info.y_size;

# ifdef IRIX63
	p2->intobj = pciio_intr_alloc(in_vtx, &p2->dev_desc, PCIIO_INTR_LINE_A, in_vtx);
# else /* IRIX63 */
	p2->intobj = pciio_intr_alloc(in_vtx, 0, PCIIO_INTR_LINE_A, in_vtx);
# endif /* IRIX63 */
	if(p2->intobj == 0)
		cmn_err(CE_DEBUG,"pciio_intr_alloc returned NULL \n");
	else
	{
		rc = pciio_intr_connect(p2->intobj, (intr_func_t)rad1_inthandler,
				(intr_arg_t)p2, NULL); /* ready for ints */
		if(rc < 0)
			cmn_err(CE_DEBUG,"unable to connect interrupt\n");
		else
		{	
			/* enable the local intrs (PCI,dma 0)*/
		}
	}
/*	cmn_err(CE_DEBUG,"connected interrupt\n"); * */
	p2->p2_intenable.bits.bypass_dma_int_en = RAD1_ENABLE;
	p2->p2_intenable.bits.dma_int_en = RAD1_ENABLE;
	p2->p2_intenable.bits.sync_int_en = RAD1_ENABLE;
	*(registers + INTENABLE_REG/4) = p2->p2_intenable.all;
	
#ifndef IRIX63
	if(number_of_per2s > 0) 
	{
		sprintf(dev_name, "rad1.%d", number_of_per2s);
	}
	else
	{
		sprintf(dev_name, "rad1");
	}
	ret = hwgraph_char_device_add(in_vtx,dev_name,"rad1pci",&vtx);
	if(ret != GRAPH_SUCCESS)
	{
		kmem_free(p2,sizeof(per2_info));
		cmn_err(CE_DEBUG,"hwgraph_char_device_add failed ret = 0x%x\n",ret);
		return ret;
	}
	p2->type = RAD1_TYPE;
	
	vtx_tty = hwgraph_path_to_vertex("/hw/ttys");
	for (i = 0; i < MAX_RAD1S; i++)
	{
		sprintf(devt_name, "/hw/ttys/ttyrd%d", i);
		vtx_tty1 = hwgraph_path_to_vertex(devt_name);
		if (vtx_tty1 == -1)	/* the vertex does not exist */
		{
			sprintf(devt_name, "ttyrd%d", i);
			ret = hwgraph_char_device_add(vtx_tty, devt_name, "rad1pci", &vtx_tty1);
			if (ret == GRAPH_SUCCESS)
				break;	/* if vertex successfully created, ok, else give up */
			kmem_free(p2, sizeof(per2_info));
			cmn_err(CE_DEBUG, "hwgraph_char_device_add %s failed %d\n", devt_name, ret);
			return ENOMEM;
		}
	}
	if (i == MAX_RAD1S)
	{ 
		cmn_err(CE_DEBUG,"unable to create ttyrd vertex\n");
	}
	else
	{
		p2tty = (per2serial_info *)kmem_zalloc(sizeof(per2serial_info), KM_SLEEP);
		if (p2tty == 0)
		{	
			kmem_free(p2, sizeof(per2_info));
			cmn_err(CE_DEBUG, "p2tty alloc failed\n");
			return ENOMEM;
		}
		p2tty->type = RAD1TTY_TYPE;
		p2tty->p2 = p2;
		device_info_set(vtx_tty1, (void *)p2tty);
	}

	if(number_of_per2s == 0)
	{
		ret = hwgraph_char_device_add(in_vtx,"rad1_b","rad1pci",&vtx_bigx);
		if(ret != GRAPH_SUCCESS)
		{ 
			kmem_free(p2tty,sizeof(per2serial_info));
			kmem_free(p2,sizeof(per2_info));
			cmn_err(CE_DEBUG,"hwgraph_char_device_add bigx failed ret = 0x%x\n",ret);
			return ret;
		}
		
		p2bigx = (per2serial_info *)kmem_zalloc(sizeof(per2serial_info),KM_SLEEP);
		if(p2bigx == 0)
		{	
			kmem_free(p2tty,sizeof(per2serial_info));
			kmem_free(p2,sizeof(per2_info));
			cmn_err(CE_DEBUG,"p2bigx alloc failed\n");
			return ENOMEM;
		}
		p2bigx->type = RAD1BIG_TYPE;
		p2bigx->p2 = p2;
	}
#else /* defined IRIX63 */
	vtx = in_vtx;
	vtx_bigx = in_vtx;
	vtx_tty = in_vtx;
#endif /* defined IRIX63 */
	number_of_per2s++;
	p2->dev = in_vtx;
#if defined(IRIX63)
 	device_info_set(vtx, (arbitrary_info_t)p2);
#else /*  defined(IRIX63) */
	device_info_set(vtx, (void *)p2);
	if (number_of_per2s == 1)
		device_info_set(vtx_bigx, (void *)p2bigx);
#endif /* defined IRIX63 */
	pciinfo = pciio_info_get(in_vtx); /*  */
 	slot =  pciio_info_slot_get(pciinfo); /*  */

#if !defined(IRIX63) 
	device_inventory_add(vhdl_to_dev(vtx), INV_PCI, p2->vendor_id, 
		0, (int)slot, p2->device_id);
#endif /* defined IRIX63 */

#define TEXTPORT_INCLUDED
#ifdef TEXTPORT_INCLUDED
	if(number_of_per2s == 1) /* only set up the tport on the first */
	{
	/* register textport */
		RAD1BoardTPInit(p2, number_of_per2s - 1);
		rad1_info_pointer = p2;
		if(rad1_earlyinit_base != 0)
		{ int x,y;
			int *temp_p;
			temp_p = p2->tbuf;
			p2->tbuf = rad1_earlyinit_base;
			bigx_update(p2, 0, 0, p2->v_info.x_size, p2->v_info.y_size, UIO_WRITE);/* */
			p2->tbuf = temp_p;
			kmem_free(rad1_earlyinit_base,RAD1_SGRAM_SIZE);
			rad1_earlyinit_base = 0;
		}

	}
#endif /* TEXTPORT_INCLUDED */

/*	cmn_err(CE_DEBUG,"rad1pciattach complete\n"); * */

	return 0;
}

#ifndef IRIX63
int rad1pcidetach(vertex_hdl_t vtx)
{ 
	int num,ii;
	per2_info *p2;
	int type;
	
	p2 = (per2_info *)device_info_get(vtx);
		/* take the global lock to make sure any dma is finished*/
	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
	INT_LOCK_LOCK(&p2->per2_global_lock);

	num = p2->board_number;	/* we need this after we let the structure go */

	/* abort any dma */

	if(p2->dma_map)
		pciio_dmamap_free(p2->dma_map);

	if((--number_of_per2s) <= 0)
	{
		pciio_intr_disconnect(p2->intobj);
		pciio_intr_free(p2->intobj);
#ifdef ALLOC_TBUF
		if(tbuf != 0)
			kmem_free(tbuf, RAD1_SGRAM_SIZE);
		tbuf = NULL;
#endif /* ALLOC_TBUF */
	}
	INT_LOCK_UNLOCK(&p2->per2_global_lock); /* release the global lock */
	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
	if (p2->bigx_daemon_stall)
		untimeout(p2->bigx_cookie);
	
	kmem_free(p2, sizeof(per2_info));
	hwgraph_vertex_destroy(vtx);
	cmn_err(CE_DEBUG,"rad1 %d detached\n",num);
	return 0;
}
#endif /*  IRIX63 */

int rad1pciinit()
{
	int ret, ret2;	

/*	cmn_err(CE_DEBUG,"in rad1pciinit\n"); * */
#if defined IRIX63
	ret = pciio_add_attach(rad1pciattach, NULL, NULL, RAD1_PFX, rad1pci_major);
	if(ret == -1)
		return EIO;
#endif /*  defined IRIX63 */
	ret = pciio_driver_register(RAD1_VENDOR_ID, RAD1_DEVICE_ID,
		RAD1_PFX, PER2_FLAGS);
	ret2 = pciio_driver_register(RAD1_2V_VENDOR_ID, RAD1_2V_DEVICE_ID,
		RAD1_PFX, PER2_FLAGS);
	if(ret && ret2)
	{
		cmn_err(CE_DEBUG,"error 0x%x registering RAD1\n", ret);
		return ret;
	}
	/*	set up the type of dma we will use */
#ifdef IRIX63
	pciio_dma_flags = PCIIO_DMAMAP_BIGEND;
#else /* IRIX63 */
	if(rad1pci_enable_prefetch == 1)
		pciio_dma_flags = PCIIO_DMA_DATA|PCIIO_PREFETCH;
	else
		pciio_dma_flags = PCIIO_DMA_DATA|PCIIO_NOPREFETCH;
#endif /* IRIX63 */
	return 0;
}

#ifdef COMMENT /* IP30 for loadable drivers */
int rad1pcireg (struct edt *e)
{	
	int ret;	

	ret = pciio_driver_register(RAD1_VENDOR_ID, RAD1_DEVICE_ID,
		RAD1_PFX, PER2_FLAGS);
	if(ret == -1)
		return EIO;
	return 0;
}
#endif /* COMMENT */

void per2_bigx_to_func(int *flag, per2_info *p2)
{
	*flag = FALSE;
}

void rad1_to_func(sv_t *waitid, per2_info *p2)
{
	int rc,tmp;
	if(waitid == p2->lut_wait_on_it)
	{
		cmn_err(CE_DEBUG,"lut load no vblank timeout\n");	 /* */
		tmp = *(p2->registers + INTFLAGS_REG/4);
		*(p2->registers + INTFLAGS_REG/4) = tmp;
		INT_LOCK_LOCK(&p2->per2_global_lock);
		if (p2->lut_flag)
		{
			p2->p2_intenable.bits.vert_retrace_int_en = RAD1_DISABLE;
				/* disable vertical retrace interrupt */
			*(p2->registers + INTENABLE_REG/4) = p2->p2_intenable.all;
			rad1_load_lut(p2);
			p2->lut_flag = FALSE;
		}
		INT_LOCK_UNLOCK(&p2->per2_global_lock);
	}
	else if(waitid == p2->wid_wait_on_it)
	{
		cmn_err(CE_DEBUG,"wid load no vblank timeout\n");	 /* */
		SV_SIGNAL(waitid);
	}
	else if(waitid == p2->dma_wr_wait_on_it)
	{
		cmn_err(CE_DEBUG,"write dma int timed out \n");	 /* */
		INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock */
		p2->time_happened++;
	
#ifndef COMMENT
		if(p2->intobj == 0)
		{	
			cmn_err(CE_DEBUG,"pciio_intr_alloc returned NULL \n");
		}
		else
		{
			pciio_intr_disconnect(p2->intobj);
			cmn_err(CE_DEBUG,"ftr pciio_intr_disconnect\n");
			rc = pciio_intr_connect(p2->intobj, (intr_func_t)rad1_inthandler,
				(intr_arg_t)p2, NULL); /* ready for ints */
			cmn_err(CE_DEBUG,"ftr pciio_intr_connect\n");
			if(rc == -1)
			{
				cmn_err(CE_DEBUG,"unable to connect interrupt\n");
			}
		}
#endif /*  COMMENT */
		SV_SIGNAL(waitid);
		INT_LOCK_UNLOCK(&p2->per2_global_lock);	/* release the global lock */
	}
	else if(waitid == p2->dma_rd_wait_on_it)
	{
		cmn_err(CE_DEBUG,"read dma int timed out \n");	 /* */
		p2->time_happened++;
		SV_SIGNAL(waitid);
	}
	else if(waitid == p2->sync_wait_on_it)
	{
		cmn_err(CE_DEBUG,"sync int timed out \n");	 /* */
		p2->time_happened++;
		SV_SIGNAL(waitid);
	}
}

static int rad1_pio_move(per2_info *p2, uio_t *uio, uio_rw_t direction)
{
	int page_mask, page_left;
	int count;
	int i;
	caddr_t to, from;

	page_mask = NBPP - 1;
	while(uio->uio_resid > 0)
	{
		if(uio->uio_offset & 0x3)
			break;	/* not on 4 byte boundary */

		if(*(__psint_t *)&(uio->uio_iov->iov_base) & 0x3)
			break;	/* not on 4 byte boundary */

		page_left = NBPP - (*(__psint_t *)&(uio->uio_iov->iov_base) & page_mask);	
		count = (page_left > uio->uio_iov->iov_len)?
				uio->uio_iov->iov_len:page_left;
		if(count & 0x3)
			break;	/* not on 4 byte boundary */

		/* now move the data */
		if(direction == UIO_WRITE)
		{
			to = ((caddr_t)p2->ram1) + uio->uio_offset;
			from = uio->uio_iov->iov_base;
		}
		else /* (direction == UIO_READ) */
		{
			from = ((caddr_t)p2->ram1) + uio->uio_offset;
			to = uio->uio_iov->iov_base;
		}
		for(i=0; i<count/sizeof(int); i++)
			((int *)to)[i] = ((int *)from)[i];

		/* now clean up and leave */
		uio->uio_resid -= count;
		uio->uio_offset += count;
		uio->uio_iov->iov_len -= count;
		if (uio->uio_iov->iov_len <= 0)
		{
			if(uio->uio_iovcnt <=0)
				break;
			uio->uio_iovcnt--;	/* move to the next vector */
			uio->uio_iov++;	/* move to the next vector */
		}
		else
			*(__psint_t *)&(uio->uio_iov->iov_base) += count;

		delay(0);	/* let other work get done */
	} /* while */
	return 0;
}

int
rad1pciwrite(dev_t dev, uio_t *uio, cred_t *crp)
{
	int count;
	int offset;
	int type;
	per2_info *p2;
	per2serial_info *tempp2;
	vertex_hdl_t vrtxh;
	int rtv;
	unsigned int major,minor;
	dev_t maj_dev;
#ifdef IRIX63
	minor = dev & 0xf;	/* its said to be wrong but it works */
	dev = dev & ~0xf;	/* its said to be wrong but it works */
#endif /* IRIX63 */
	vrtxh = dev_to_vhdl(dev);
	if(vrtxh ==NULL)
	{	cmn_err(CE_DEBUG,"dev_to_vhdl in write returned 0x%x\n",vrtxh);
		return ENXIO;
	}
	p2 = (per2_info *)device_info_get(vrtxh);
	if(p2 == NULL) return ENXIO;

#ifdef IRIX63
	type = minor;
#else /*  IRIX63 */
	type = p2->type;
	if(type != RAD1_TYPE)
	{
		tempp2 = (per2serial_info *)p2;
		p2 = tempp2->p2;
	}
#endif /*  IRIX63 */

	switch (type)
	{
		case RAD1_TYPE:
			count = uio->uio_resid;
			offset = (int)(uio->uio_offset);
			/* ram is always left enabled */
			if ((count+offset) >= RAD1_SGRAM_SIZE) return EFAULT;

			rtv = rad1_pio_move(p2, uio, UIO_WRITE);
			return rtv;
#ifdef RAD1_CONSOLE
		case RAD1TTY_TYPE:
			rtv = rad1_write_tty(p2, uio);
			return rtv;
#endif /*  RAD1_CONSOLE */

		default:
			return ENXIO;
	}
}

int
rad1pciread(dev_t dev, uio_t *uio, cred_t *crp)
{
	int i;
	size_t count;
	int offset;
	per2_info *p2;
	per2serial_info *tempp2;
	vertex_hdl_t vrtxh;
	int rtv = 0;
	int type;
	unsigned int major,minor;
	dev_t maj_dev;
#ifdef IRIX63
	minor = dev & 0xf;	/* its said to be wrong but it works */
	dev = dev & ~0xf;	/* its said to be wrong but it works */
#endif /* IRIX63 */
		
	vrtxh = dev_to_vhdl(dev);
	if(vrtxh ==NULL)
	{	cmn_err(CE_DEBUG,"dev_to_vhdl in read returned 0x%x\n",vrtxh);
		return ENXIO;
	}
	p2 = (per2_info *)device_info_get(vrtxh);
	if(p2 == NULL) return ENXIO;
#ifdef IRIX63
	type = minor;
#else /*  IRIX63 */
	type = p2->type;
	if(type != RAD1_TYPE)
	{
		tempp2 = (per2serial_info *)p2;
		p2 = tempp2->p2;
	}
#endif /*  IRIX63 */

	switch (type)
	{
		case RAD1_TYPE:
			count = uio->uio_resid;
			offset = (int)(uio->uio_offset);

			if ((count+offset) >= RAD1_SGRAM_SIZE)
				return EFAULT;

			rtv = rad1_pio_move(p2,uio, UIO_READ);
			return rtv;
#ifdef RAD1_CONSOLE
		case RAD1TTY_TYPE:
			rtv = rad1_read_tty(p2, uio);
			return rtv;
#endif /*  RAD1_CONSOLE */

		default:
			return ENXIO;
	}
}

int
rad1pcimap(dev_t dev, vhandl_t *vt,off_t off, size_t len, int prot)
{
	int i;
	caddr_t ptr;
	int rtv;
	int type;
	vertex_hdl_t vrtxh;
	per2_info *p2;
	per2serial_info *tempp2;
	unsigned int major, minor;

#ifdef IRIX63
	minor = dev & 0xf;  /* its said to be wrong but it works */
	dev = dev & ~0xf; /* its said to be wrong but it works */
#endif /* IRIX63 */

	vrtxh = dev_to_vhdl(dev);
	if(vrtxh ==NULL)
	{	cmn_err(CE_DEBUG,"dev_to_vhdl map returned 0x%x\n",vrtxh);
		return ENXIO;
	}
	p2 = (per2_info *)device_info_get(vrtxh);
	if(p2 == NULL) return ENXIO;
#ifdef IRIX63
	type = minor;
#else /*  IRIX63 */
	type = p2->type;
	if(type != RAD1_TYPE)
	{	if(type != RAD1BIG_TYPE)
			return ENXIO;
		tempp2 = (per2serial_info *)p2;
		p2 = tempp2->p2;
	}
#endif /*  IRIX63 */
	if(type == RAD1BIG_TYPE)
	{	ptr = (caddr_t)p2->tbuf;
		if((off+len) > (bigx_x * bigx_y *4))
			return ENOSPC;
	}
	else
	{
		ptr = (caddr_t)p2->ram1;
		if( (off+len) > RAD1_SGRAM_SIZE ) return ENOSPC;
	}

/* 	cmn_err(CE_DEBUG,"map off = 0x%lx len = 0x%lx NBPC = 0x%lx\n",
					(long)off,(long)len,(long)NBPC); * */
	/* Don't allow non page-aligned offsets */
	if ((off % NBPC) != 0)
		return EIO;
	/* Only allow mapping of entire pages */
	if ((len % NBPC) != 0)
		return EIO;
/*	cmn_err(CE_DEBUG,"b4 map pyhs vt->v_preg = 0x%x vt->v_addr = 0x%x\n",
		vt->v_preg , vt->v_addr); * */
	rtv = v_mapphys(vt, ptr + off, len); /* */
	if(rtv != 0)
	{
/*		cmn_err(CE_DEBUG,"aftr map pyhs vt->v_preg = 0x%x vt->v_addr = 0x%x\n",
			vt->v_preg , vt->v_addr); * */
		cmn_err(CE_DEBUG,"about to return 0x%x from map\n",rtv); /* */
	}
	return rtv;
}


int rad1pciunmap(dev_t dev, vhandl_t *vt)
{
	return 0;
}

int rad1pciioctl (dev_t dev, int cmd, int *arg, int mode,
  cred_t *crp, int *rvalp)
{
	per2_info *p2, *lp2;
	per2serial_info *tempp2;
	int i, tmp;
	int reg;
	toid_t to_cookie;
	rad4_rect_write d_rrws;
	rad4_eeprom_sector sector;
	__userabi_t user_info;
	int type;
	unsigned int major, minor;
	int board;
	int *a_ptr;
	int *cursor_ptr;

	if(ESRCH == userabi(&user_info))
	{	cmn_err(CE_DEBUG,"userabi returned ESRCH\n");
		return ENXIO;
	}

#ifdef IRIX63
	minor = dev & 0xf;	/* its said to be wrong but it works */
	dev = dev & ~0xf;	/* its said to be wrong but it works */
#endif /* IRIX63 */
	p2 = (per2_info *)device_info_get(dev_to_vhdl(dev));
	if(p2 == NULL)
	{
		cmn_err(CE_DEBUG,
			"rad1pciioctl:  device_info_get return NULL dev = 0x%x d2v = 0x%x\n",  
			dev,dev_to_vhdl(dev));	/* */
		return ENXIO;
	}
#ifdef IRIX63
	type = minor;
#else /*  IRIX63 */
	type = p2->type;
	if(type != RAD1_TYPE)
	{
		tempp2 = (per2serial_info *)p2;
		p2 = tempp2->p2;
	}
#endif /*  IRIX63 */
/*	cmn_err(CE_DEBUG,"rad1pciioctl: %d, 0x%x\n", type, cmd);	* */

	switch (type)
	{
		case RAD1_TYPE:
		case RAD1BIG_TYPE:
			switch ( cmd )
			{
				case RAD4_SETLUT: /* map the full lut, arg is an int pointer */
					if (p2->device_id == RAD1_DEVICE_ID)
						break;
					if(type == RAD1BIG_TYPE)
					{
						for(board = 0; board < number_of_per2s; board++)
						{
							lp2 = bigx_p2s[board];
#ifdef VB_LUT_WID_LOAD
							SLP_LOCK_LOCK(&lp2->vblank_mutex_lock); 
							INT_LOCK_LOCK(&lp2->per2_global_lock);
							for (i = 0; i < MAX_LUT_ENTRIES; i++)
								lp2->lut_shadow[i] = arg[i];	/* copy to shadow */

							if (!lp2->lut_flag)
							{
								lp2->lut_flag = TRUE;
								lp2->p2_intenable.bits.vert_retrace_int_en =
									RAD1_ENABLE;
									/* enable vertical retrace interrupt */
								*(lp2->registers + INTENABLE_REG/4) = lp2->p2_intenable.all;
								lp2->lut_to_cookie = itimeout(rad1_to_func,
									lp2->lut_wait_on_it,
									drv_usectohz(VB_TIMEOUT_US), pltimeout, lp2);
							}
							INT_LOCK_UNLOCK(&lp2->per2_global_lock);
							SLP_LOCK_UNLOCK(&lp2->vblank_mutex_lock); 
#else /* VB_LUT_WID_LOAD */
							for (i = 0; i < MAX_LUT_ENTRIES; i++)
								lp2->lut_shadow[i] = arg[i];	/* copy to shadow */

							SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);	/* lock the hardware */
							rad1_load_lut(lp2);	/* */
							SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);	/* release the hardware */
#endif /* VB_LUT_WID_LOAD */
						}
					}
					else
					{
#ifdef VB_LUT_WID_LOAD
						SLP_LOCK_LOCK(&p2->vblank_mutex_lock); 
						INT_LOCK_LOCK(&p2->per2_global_lock);
						for (i = 0; i < MAX_LUT_ENTRIES; i++)
							p2->lut_shadow[i] = arg[i];	/* copy to shadow */

						if (!p2->lut_flag)
						{
							p2->lut_flag = TRUE;
							p2->p2_intenable.bits.vert_retrace_int_en = RAD1_ENABLE;
								/* enable vertical retrace interrupt */
							*(p2->registers + INTENABLE_REG/4) = p2->p2_intenable.all;
							p2->lut_to_cookie = itimeout(rad1_to_func,
								p2->lut_wait_on_it,
								drv_usectohz(VB_TIMEOUT_US), pltimeout, p2);
						}
						INT_LOCK_UNLOCK(&p2->per2_global_lock);
						SLP_LOCK_UNLOCK(&p2->vblank_mutex_lock); 
#else /* VB_LUT_WID_LOAD */
						for (i = 0; i < MAX_LUT_ENTRIES; i++)
							p2->lut_shadow[i] = arg[i];	/* copy to shadow */

						SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
						rad1_load_lut(p2);	/* */
						SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
#endif /* VB_LUT_WID_LOAD */
					}
					break;
				case RAD4_SETLUTSS: /* map a sub set of the lut as described in arg */
				{
					int start = ((rad4_lut_t *)arg)->start;
					int count = ((rad4_lut_t *)arg)->count;
					int *colors = &((rad4_lut_t *)arg)->colors[0];

					if (p2->device_id == RAD1_DEVICE_ID)
						break;
					if(start > MAX_LUT_ENTRIES - 1)
						start = MAX_LUT_ENTRIES - 1;  /* force it inbounds */
					if(count+start > MAX_LUT_ENTRIES)
						count = MAX_LUT_ENTRIES - start;	/* force it inbounds */

					if(type == RAD1BIG_TYPE)
					{
						for(board = 0; board < number_of_per2s; board++)
						{
							lp2 = bigx_p2s[board];
#ifdef VB_LUT_WID_LOAD
							SLP_LOCK_LOCK(&lp2->vblank_mutex_lock); 
							INT_LOCK_LOCK(&lp2->per2_global_lock);
							for (i = start; i < start + count; i++)
								lp2->lut_shadow[i] = colors[i];	/* copy to shadow */

							if (!lp2->lut_flag)
							{
								lp2->lut_flag = TRUE;
								lp2->p2_intenable.bits.vert_retrace_int_en = RAD1_ENABLE;
									/* enable vertical retrace interrupt */
								*(lp2->registers + INTENABLE_REG/4) = lp2->p2_intenable.all;
								lp2->lut_to_cookie = itimeout(rad1_to_func,
									lp2->lut_wait_on_it,
									drv_usectohz(VB_TIMEOUT_US), pltimeout, lp2);
							}
							INT_LOCK_UNLOCK(&lp2->per2_global_lock);
							SLP_LOCK_UNLOCK(&lp2->vblank_mutex_lock); 
#else /* VB_LUT_WID_LOAD */
							for (i = start; i < start + count; i++)
								lp2->lut_shadow[i] = colors[i];	/* copy to shadow */

							SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);	/* lock the hardware */
							rad1_load_lut(lp2);	/* */
							SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);	/* release the hardware */
#endif /* VB_LUT_WID_LOAD */
						}
					}
					else
					{
#ifdef VB_LUT_WID_LOAD
						SLP_LOCK_LOCK(&p2->vblank_mutex_lock); 
						INT_LOCK_LOCK(&p2->per2_global_lock);
						for (i = start; i < start + count; i++)
							p2->lut_shadow[i] = colors[i];	/* copy to shadow */

						if (!p2->lut_flag)
						{
							p2->lut_flag = TRUE;
							p2->p2_intenable.bits.vert_retrace_int_en = RAD1_ENABLE;
								/* enable vertical retrace interrupt */
							*(p2->registers + INTERRUPTLINE_REG/4) = 256;
							*(p2->registers + INTENABLE_REG/4) = p2->p2_intenable.all;
							p2->lut_to_cookie = itimeout(rad1_to_func,
								p2->lut_wait_on_it,
								drv_usectohz(VB_TIMEOUT_US), pltimeout, p2);
						}
						INT_LOCK_UNLOCK(&p2->per2_global_lock);
						SLP_LOCK_UNLOCK(&p2->vblank_mutex_lock); 
#else /* VB_LUT_WID_LOAD */
						for (i = start; i < start + count; i++)
						{ p2->lut_shadow[i] = colors[i];	/* copy to shadow */
						}

						SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
						rad1_load_lut(p2);	/* */
						SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
#endif /* VB_LUT_WID_LOAD */
					}
					break;
				}
#ifdef GET_TO_THIS_EVENTUALLY
				case RAD4_SETDEPTH8_INDEX:
				case RAD4_SETDEPTH8_GREY:
				case RAD4_SETDEPTH8_TRUE:
				case RAD4_SETDEPTH8_DIRECT:
				case RAD4_SETDEPTH12_TRUE:
				case RAD4_SETDEPTH12_DIRECT:
				case RAD4_SETDEPTH16_TRUE:
				case RAD4_SETDEPTH16_DIRECT:
				case RAD4_SETDEPTH24_TRUE:
				case RAD4_SETDEPTH24_DIRECT:
#ifdef VB_LUT_WID_LOAD
					SLP_LOCK_LOCK(&p2->vblank_mutex_lock);
					INT_LOCK_LOCK(&p2->per2_global_lock);
					to_cookie = itimeout(rad1_to_func, p2->wid_wait_on_it,
						drv_usectohz(VB_TIMEOUT_US), pltimeout, p2);/* wait 8 vblanks */
					p2->wid_flag = TRUE;
					p2->p2_intenable.bits.vert_retrace_int_en = RAD1_ENABLE;
						/* enable vertical retrace interrupt */
					*(p2->registers + INTENABLE_REG/4) = p2->p2_intenable.all;
					INT_LOCK_SV_WAIT(p2->wid_wait_on_it, &p2->per2_global_lock);
					INT_LOCK_LOCK(&p2->per2_global_lock);
					p2->p2_intenable.bits.vert_retrace_int_en = RAD1_DISABLE;
						/* disable vertical retrace interrupt */
					*(p2->registers + INTENABLE_REG/4) = p2->p2_intenable.all;
					p2->wid_flag = FALSE;
					untimeout(to_cookie);
					INT_LOCK_UNLOCK(&p2->per2_global_lock);
					SLP_LOCK_UNLOCK(&p2->vblank_mutex_lock); 
#endif /*  VB_LUT_WID_LOAD */
					if(type == RAD1BIG_TYPE)
					{
						for(board = 0; board < number_of_per2s; board++)
						{
							load_wid(bigx_p2s[board], cmd, ((rad4_lut_t *)arg)->start_addr,
								((rad4_lut_t *)arg)->id, ((rad4_lut_t *)arg)->mask);
						}
					}
					else
						load_wid(p2, cmd, ((rad4_lut_t *)arg)->start_addr,
							((rad4_lut_t *)arg)->id, ((rad4_lut_t *)arg)->mask);
					break;
#endif /* GET_TO_THIS_EVENTUALLY */

				case RAD4_GETTIMING:
				{
					rad4_video_info *video_info = (rad4_video_info *)arg;
					
					/* return current settings */
					if(type == RAD1BIG_TYPE)
					{
						video_info->x_size = bigx_x;
						video_info->y_size = bigx_y;
					}
					else
					{
						video_info->x_size = p2->v_info.x_size;
						video_info->y_size = p2->v_info.y_size;
					}
					video_info->v_rate = p2->v_info.v_rate;
					break;
				}
				case RAD4_GETTIMING_EX:
				{
					rad4_video_info_ex *video_info = (rad4_video_info_ex *)arg;
					int tv = 0x7f & video_info->index;
					
					if (video_info->index < 0)
					{
						/* return current settings */
						if (type == RAD1BIG_TYPE)
						{
							video_info->x_size = bigx_x;
							video_info->y_size = bigx_y;
						}
						else
						{
							video_info->x_size = p2->v_info.x_size;
							video_info->y_size = p2->v_info.y_size;
						}
						video_info->v_rate = p2->v_info.v_rate;
						video_info->index = p2->v_info.index;
					}
					else
					{
						/* return settings for requested index */
						if (tv >= NumPER2Modes)
						{
							video_info->index = -1;	/* bad index */
							break;
						}
						if (type == RAD1BIG_TYPE)
						{
							video_info->x_size = (number_of_per2s + 1) * 
								PER2Modes[tv].modeInformation.VisScreenWidth;
							video_info->y_size = 
								PER2Modes[tv].modeInformation.VisScreenHeight;
						}
						else
						{
							video_info->x_size =
								PER2Modes[tv].modeInformation.VisScreenWidth;
							video_info->y_size =
								PER2Modes[tv].modeInformation.VisScreenHeight;
						}
						video_info->v_rate = PER2Modes[tv].modeInformation.Frequency;
					}
					break;
				}
				case RAD4_SETTIMING:
				{
					int *registers = p2->registers;
					int tv = 0x7f & (__psint_t)arg;
					int hsp = (0x100 & (__psint_t)arg) >> 8;
					int vsp = (0x400 & (__psint_t)arg) >> 10;
					int sync = (0x3000 & (__psint_t)arg) >> 12;
					if (tv >= NumPER2Modes)
						return ENXIO;	/* bad video value */

					rad1_set_video_timing(p2, tv, sync, hsp, vsp, 0);

					SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
					INT_LOCK_LOCK(&p2->per2_global_lock);
					p2->p2_videocontrol.bits.enable = RAD1_DISABLE;
						/* turn off display refresh */
					*(registers + VIDEOCONTROL_REG/4) = p2->p2_videocontrol.all;
					rad1_init_RAMDAC(p2);
					INT_LOCK_UNLOCK(&p2->per2_global_lock);
					SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
					*(registers + BYPASSWRITEMASK_REG/4) = RAD1_WRITEMASK_ALL;
						/* enable all VRAM bits */
					fill_fb(p2);	/* */
					*(registers + BYPASSWRITEMASK_REG/4) = p2->mask;
					SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
					p2->p2_videocontrol.bits.enable = RAD1_ENABLE;
					*(registers + VIDEOCONTROL_REG/4) = p2->p2_videocontrol.all;
					SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */

					if (p2->saved_info.item[RESOLUTION_INDEX] != (__psint_t)arg)
					{
						p2->saved_info.item[RESOLUTION_INDEX] = (__psint_t)arg;
						/* write info into EEPROM */
						program_eeprom_sector(p2, SAVED_INFO_OFFSET,
							(unsigned char *)(&p2->saved_info));
					}
					break;
				}

				case RAD4_RECT_WRITE_MODE: /* set color LUT for CI8 mode */
				{
					int *registers = p2->registers;
					int count = ((rad4_write_mode_t *)arg)->count;
					int *colors = &((rad4_write_mode_t *)arg)->colors[0];

					if(count > 256)
						count = 256;	/* force it inbounds */

					WAIT_IN_FIFO(2);	/* wait for room in register FIFO */

					SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
					*(registers + TEXELLUTADDRESS_REG/4) =
						(RAD1_SGRAM_SIZE / 4) - 256;	/* put LUT at end of SGRAM */
					*(registers + TEXELLUTINDEX_REG/4) = 0;
					for (i = 0; i < count; i++)
					{
						WAIT_IN_FIFO(1);	/* wait for room in register FIFO */
						*(registers + TEXELLUTDATA_REG/4) = colors[i];
					}
					SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
					break;
				}

				case RAD4_SETCURSOR_POS:
				{
					int lx, ly;
					int bigx_step = bigx_x / number_of_per2s;
					rad4_cursor_pos *temp = (rad4_cursor_pos *)arg;
					ly = temp->y + p2->cursor_y_offset;
					if (type == RAD1BIG_TYPE)
					{
						for (board=0; board < number_of_per2s; board++)
						{
							lp2 = bigx_p2s[board];
							a_ptr = lp2->registers;
							SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);
							if (board==temp->x / bigx_step)/* turn on the cursor and place it*/
							{
								lx = temp->x % bigx_step;
								if (lp2->device_id == RAD1_2V_DEVICE_ID)
								{
									WR_INDEXED_REGISTER_2V(a_ptr, RDCURSORXLOW_2V_REG,
										lx & 0xFF);
									WR_INDEXED_REGISTER_2V(a_ptr, RDCURSORXHIGH_2V_REG,
										lx >> 8);
									WR_INDEXED_REGISTER_2V(a_ptr, RDCURSORYLOW_2V_REG,
										ly & 0xFF);
									WR_INDEXED_REGISTER_2V(a_ptr, RDCURSORYHIGH_2V_REG,
										ly >> 8);
									lp2->p2_rdcursormode_2v.bits.cursorEnable = 
										RAD1_ENABLE;
									WR_INDEXED_REGISTER_2V(lp2->registers,
										RDCURSORMODE_2V_REG,
										lp2->p2_rdcursormode_2v.all);
								}
								else if (lp2->device_id == RAD1_DEVICE_ID)
								{
									lx = (temp->x % bigx_step) + lp2->cursor_x_offset;
									*(a_ptr + RDCURSORXLOW_REG/4) = lx & 0xFF;
									*(a_ptr + RDCURSORXHIGH_REG/4) = lx >> 8;
									*(a_ptr + RDCURSORYLOW_REG/4) = ly & 0xFF;
									*(a_ptr + RDCURSORYHIGH_REG/4) = ly >> 8;
									lp2->p2_rdcursorcontrol.bits.cursorMode =
										CURSORMODE_XWINDOW;
									WR_INDEXED_REGISTER(a_ptr, RDCURSORCONTROL_REG,
										lp2->p2_rdcursorcontrol.all);
										/* enable cursor */
								}
							}
							else 	/* turn it off */
							{
								if (lp2->device_id == RAD1_2V_DEVICE_ID)
								{
									lp2->p2_rdcursormode_2v.bits.cursorEnable =
										RAD1_DISABLE;
									WR_INDEXED_REGISTER_2V(lp2->registers,
										RDCURSORMODE_2V_REG,
										lp2->p2_rdcursormode_2v.all);
								}
								else if (lp2->device_id == RAD1_DEVICE_ID)
								{
									lp2->p2_rdcursorcontrol.bits.cursorMode =
										CURSORMODE_DIS;
									WR_INDEXED_REGISTER(lp2->registers,
										RDCURSORCONTROL_REG, 
										lp2->p2_rdcursorcontrol.all);
								}
							}
							SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);
						}
					}
					else
					{
						lx = temp->x;
						a_ptr = p2->registers;
						SLP_LOCK_LOCK(&p2->hardware_mutex_lock);
						if (p2->device_id == RAD1_2V_DEVICE_ID)
						{
							ly = temp->y;
							WR_INDEXED_REGISTER_2V(a_ptr, RDCURSORXLOW_2V_REG, lx & 0xFF);
							WR_INDEXED_REGISTER_2V(a_ptr, RDCURSORXHIGH_2V_REG, lx >> 8);
							WR_INDEXED_REGISTER_2V(a_ptr, RDCURSORYLOW_2V_REG, ly & 0xFF);
							WR_INDEXED_REGISTER_2V(a_ptr, RDCURSORYHIGH_2V_REG, ly >> 8);
						}
						else if (p2->device_id == RAD1_DEVICE_ID)
						{
							lx = temp->x + p2->cursor_x_offset;
							*(a_ptr + RDCURSORXLOW_REG/4) = lx & 0xFF;
							*(a_ptr + RDCURSORXHIGH_REG/4) = lx >> 8;
							*(a_ptr + RDCURSORYLOW_REG/4) = ly & 0xFF;
							*(a_ptr + RDCURSORYHIGH_REG/4) = ly >> 8;
						}
						SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);
					}
					break;
				}
				case RAD4_SETCURSOR_CLUT:
				{
					rad4_cursor_clut *temp = (rad4_cursor_clut *)arg;
					
					if (type == RAD1BIG_TYPE)
					{
						for (board = 0; board < number_of_per2s; board++)
						{
							lp2 = bigx_p2s[board];
							a_ptr = lp2->registers;
							SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);
							if (lp2->device_id == RAD1_2V_DEVICE_ID)
							{
								cursor_ptr = a_ptr + RDINDEXEDDATA_2V_REG/4;
								*(a_ptr + RDINDEXLOW_2V_REG/4) =
									RDCURSORPALETTE_2V_REG;
								*(a_ptr + RDINDEXHIGH_2V_REG/4) =
									RDCURSORPALETTE_2V_REG >> 8;
							}
							else if (lp2->device_id == RAD1_DEVICE_ID)
							{
								cursor_ptr = a_ptr + RDCURSORCOLORDATA_REG/4;
								*(a_ptr + RDCURSORCOLORADDRESS_REG/4) = 1;
									/* write cursor color index */
							}
							*cursor_ptr = temp->b_red;
							*cursor_ptr = temp->b_green;
							*cursor_ptr = temp->b_blue;
							*cursor_ptr = temp->f_red;
							*cursor_ptr = temp->f_green;
							*cursor_ptr = temp->f_blue;
							SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);
						}
					}
					else
					{
						a_ptr = p2->registers;
						SLP_LOCK_LOCK(&p2->hardware_mutex_lock);
						if (p2->device_id == RAD1_2V_DEVICE_ID)
						{
							cursor_ptr = a_ptr + RDINDEXEDDATA_2V_REG/4;
							*(a_ptr + RDINDEXLOW_2V_REG/4) =
								RDCURSORPALETTE_2V_REG;
							*(a_ptr + RDINDEXHIGH_2V_REG/4) =
								RDCURSORPALETTE_2V_REG >> 8;
						}
						else if (p2->device_id == RAD1_DEVICE_ID)
						{
							cursor_ptr = a_ptr + RDCURSORCOLORDATA_REG/4;
							*(a_ptr + RDCURSORCOLORADDRESS_REG/4) = 1;
								/* write cursor color index */
						}
						*cursor_ptr = temp->b_red;
						*cursor_ptr = temp->b_green;
						*cursor_ptr = temp->b_blue;
						*cursor_ptr = temp->f_red;
						*cursor_ptr = temp->f_green;
						*cursor_ptr = temp->f_blue;
#if 1
	{	int i;
		for(i=7;i<(15*3);i++)
		{	
			*(a_ptr + RDINDEXLOW_2V_REG/4) = (RDCURSORPALETTE_2V_REG+i) &0xff;
			*cursor_ptr = 0x7f;
		}
							*(a_ptr + RDINDEXHIGH_2V_REG/4) =
								RDCURSORPALETTE_2V_REG >> 8;
	}
#endif
						SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);
					}
					break;
				}
				case RAD4_SETCURSOR_MAP:
				{
					rad4_cursor_map *temp = (rad4_cursor_map *)arg;
					unsigned char *bp = (unsigned char *)temp->bits;
					unsigned char *mp = (unsigned char *)temp->mask;
					unsigned int bitmask, tv;
					int i;
					if (type == RAD1BIG_TYPE)
					{
						for (board = 0;board < number_of_per2s; board++)
						{
							lp2 = bigx_p2s[board];
							a_ptr = lp2->registers;
							SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);
							if (lp2->device_id == RAD1_2V_DEVICE_ID)
							{
								cursor_ptr = a_ptr + RDINDEXEDDATA_2V_REG/4;
								WR_INDEXED_REGISTER_2V(a_ptr,
									RDCURSORHOTSPOTX_2V_REG, temp->xhot & 0x3f);
								WR_INDEXED_REGISTER_2V(a_ptr,
									RDCURSORHOTSPOTY_2V_REG, temp->yhot & 0x3f);
								*(a_ptr + RDINDEXLOW_2V_REG/4) =
									RDCURSORPATTERN_2V_REG;
								*(a_ptr + RDINDEXHIGH_2V_REG/4) =
									RDCURSORPATTERN_2V_REG >> 8;
							}
							else if (lp2->device_id == RAD1_DEVICE_ID)
							{
								cursor_ptr = a_ptr + RDCURSORRAMDATA_REG/4;
								lp2->cursor_x_offset = 64 - (temp->xhot & 0xff);
								lp2->cursor_y_offset = 64 - (temp->yhot & 0xff);
								lp2->p2_rdcursorcontrol.bits.RAMAddr = 0;
								WR_INDEXED_REGISTER(a_ptr, RDCURSORCONTROL_REG,
									lp2->p2_rdcursorcontrol.all);
								*(a_ptr + RDPALETTEWRITEADDRESS_REG/4) = 0;
							}
							/* write cursor RAM index */
							bp = (unsigned char *)temp->bits;
							mp = (unsigned char *)temp->mask;
							for(i=0; i<64*64*2/8; i+= 2)
							{	
								for(bitmask = 0x80, tv = 0; bitmask > 8; bitmask >>= 1)
								{
									tv |= (*mp&bitmask)?((*bp&bitmask)+bitmask):0;
									tv <<= 1;
								}
								*cursor_ptr = tv >> 5;
								for(bitmask = 0x08, tv = 0; bitmask > 0; bitmask >>= 1)
								{
									tv |= (*mp&bitmask)?((*bp&bitmask)+bitmask):0;
									tv <<= 1;
								}
								*cursor_ptr = tv >> 1;
								mp++;
								bp++;
							}
							SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);
						}
					}
					else
					{
						a_ptr = p2->registers;
						SLP_LOCK_LOCK(&p2->hardware_mutex_lock);
						if (p2->device_id == RAD1_2V_DEVICE_ID)
						{
							cursor_ptr = a_ptr + RDINDEXEDDATA_2V_REG/4;
							WR_INDEXED_REGISTER_2V(a_ptr,
								RDCURSORHOTSPOTX_2V_REG, temp->xhot & 0x3f);
							WR_INDEXED_REGISTER_2V(a_ptr,
								RDCURSORHOTSPOTY_2V_REG, temp->yhot & 0x3f);
							*(a_ptr + RDINDEXLOW_2V_REG/4) =
								RDCURSORPATTERN_2V_REG;
							*(a_ptr + RDINDEXHIGH_2V_REG/4) =
								RDCURSORPATTERN_2V_REG >> 8;
							for(i=0; i<64*64*2/8; i+= 2)
							{	
								for(bitmask = 0x10,tv = 0; bitmask <= 0x80;bitmask <<= 1)
								{
									tv <<= 2;
									tv |= (*mp&bitmask)?((*bp&bitmask)?3:2):0;
								}
								*cursor_ptr = tv; /* */
								for(bitmask = 0x01,tv = 0; bitmask <= 0x08;bitmask <<= 1)
								{
									tv <<= 2;
									tv |= (*mp&bitmask)?((*bp&bitmask)?3:2):0;
								}
								*cursor_ptr = tv; /* */
								mp++;
								bp++;
							}
						}
						else if (p2->device_id == RAD1_DEVICE_ID)
						{
							cursor_ptr = a_ptr + RDCURSORRAMDATA_REG/4;
							p2->cursor_x_offset = 64 - (temp->xhot & 0xff);
							p2->cursor_y_offset = 64 - (temp->yhot & 0xff);
							p2->p2_rdcursorcontrol.bits.RAMAddr = 0;
							WR_INDEXED_REGISTER(a_ptr, RDCURSORCONTROL_REG,
								p2->p2_rdcursorcontrol.all);
							*(a_ptr + RDPALETTEWRITEADDRESS_REG/4) = 0;
							for(i=0; i<64*64/8; i++)
							{	
								*cursor_ptr = *bp; /* */
								bp++;
							}
							for(i=0; i<64*64/8; i++)
							{
								*cursor_ptr = *mp; /* */
								mp++;
								bp++;
							}
						}
						SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);
					}
					break;
				}
				case RAD4_SETCURSOR_ON:
				{	
					SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
					if (p2->device_id == RAD1_2V_DEVICE_ID)
					{
						p2->p2_rdcursormode_2v.bits.cursorEnable = RAD1_ENABLE;
						WR_INDEXED_REGISTER_2V(p2->registers, RDCURSORMODE_2V_REG,
							p2->p2_rdcursormode_2v.all);
					}
					else if (p2->device_id == RAD1_DEVICE_ID)
					{
						p2->p2_rdcursorcontrol.bits.cursorMode = CURSORMODE_XWINDOW;
						WR_INDEXED_REGISTER(p2->registers, RDCURSORCONTROL_REG, 
							p2->p2_rdcursorcontrol.all);
					}
					SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
					break;
				}
				case RAD4_SETCURSOR_OFF:
				{	
					if(type == RAD1BIG_TYPE)
					{
						for(board=0; board < number_of_per2s; board++)
						{
							lp2 = bigx_p2s[board];
							SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);
							if (lp2->device_id == RAD1_2V_DEVICE_ID)
							{
								lp2->p2_rdcursormode_2v.bits.cursorEnable =
									RAD1_DISABLE;
								WR_INDEXED_REGISTER_2V(lp2->registers,
									RDCURSORMODE_2V_REG,
									lp2->p2_rdcursormode_2v.all);
							}
							else if (lp2->device_id == RAD1_DEVICE_ID)
							{
								lp2->p2_rdcursorcontrol.bits.cursorMode =
									CURSORMODE_DIS;
								WR_INDEXED_REGISTER(lp2->registers,
									RDCURSORCONTROL_REG, 
									lp2->p2_rdcursorcontrol.all);
							}
							SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);
						}
					}
					else
					{
						SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
						if (p2->device_id == RAD1_2V_DEVICE_ID)
						{
							p2->p2_rdcursormode_2v.bits.cursorEnable =
								RAD1_DISABLE;
							WR_INDEXED_REGISTER_2V(p2->registers, RDCURSORMODE_2V_REG,
								p2->p2_rdcursormode_2v.all);
						}
						else if (p2->device_id == RAD1_DEVICE_ID)
						{
							p2->p2_rdcursorcontrol.bits.cursorMode =
								CURSORMODE_DIS;
							WR_INDEXED_REGISTER(p2->registers, RDCURSORCONTROL_REG, 
								p2->p2_rdcursorcontrol.all);
						}
						SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
					}
					break;
				}

				case RAD4_SCREEN_ON:
				{	
					if (type == RAD1BIG_TYPE)
					{
						for (board=0; board < number_of_per2s; board++)
						{
							lp2 = bigx_p2s[board];
							SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);
							if (lp2->device_id == RAD1_2V_DEVICE_ID)
							{
								lp2->p2_rddaccontrol_2v.bits.DACPowerCtl = 
									NORMAL_OPERATION_2V;
								WR_INDEXED_REGISTER_2V(lp2->registers,
									RDDACCONTROL_2V_REG,
									lp2->p2_rddaccontrol_2v.all);
							}
							else if (lp2->device_id == RAD1_DEVICE_ID)
							{
								lp2->p2_rdmisccontrol.bits.DACPowerDown =
									RAD1_DISABLE;
								WR_INDEXED_REGISTER(lp2->registers,
									RDMISCCONTROL_REG, lp2->p2_rdmisccontrol.all);
							}
							SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);
						}
					}
					else
					{
						SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
						if (p2->device_id == RAD1_2V_DEVICE_ID)
						{
							p2->p2_rddaccontrol_2v.bits.DACPowerCtl = 
								NORMAL_OPERATION_2V;
							WR_INDEXED_REGISTER_2V(p2->registers,
								RDDACCONTROL_2V_REG, p2->p2_rddaccontrol_2v.all);
						}
						else if (p2->device_id == RAD1_DEVICE_ID)
						{
							p2->p2_rdmisccontrol.bits.DACPowerDown =
								RAD1_DISABLE;
							WR_INDEXED_REGISTER(p2->registers, RDMISCCONTROL_REG,
								p2->p2_rdmisccontrol.all);
						}
						SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
					}
					break;
				}
				case RAD4_SCREEN_OFF:
				{	
					if (type == RAD1BIG_TYPE)
					{
						for (board=0; board < number_of_per2s; board++)
						{
							lp2 = bigx_p2s[board];
							SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);
							if (lp2->device_id == RAD1_2V_DEVICE_ID)
							{
								lp2->p2_rddaccontrol_2v.bits.DACPowerCtl = 
									LOWPOWER_2V;
								WR_INDEXED_REGISTER_2V(lp2->registers,
									RDDACCONTROL_2V_REG,
									lp2->p2_rddaccontrol_2v.all);
							}
							else if (lp2->device_id == RAD1_DEVICE_ID)
							{
								lp2->p2_rdmisccontrol.bits.DACPowerDown =
									RAD1_ENABLE;
								WR_INDEXED_REGISTER(lp2->registers,
									RDMISCCONTROL_REG, lp2->p2_rdmisccontrol.all);
							}
							SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);
						}
					}
					else
					{
						SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
						if (p2->device_id == RAD1_2V_DEVICE_ID)
						{
							p2->p2_rddaccontrol_2v.bits.DACPowerCtl = LOWPOWER_2V;
							WR_INDEXED_REGISTER_2V(p2->registers,
								RDDACCONTROL_2V_REG, p2->p2_rddaccontrol_2v.all);
						}
						else if (p2->device_id == RAD1_DEVICE_ID)
						{
							p2->p2_rdmisccontrol.bits.DACPowerDown =
								RAD1_ENABLE;
							WR_INDEXED_REGISTER(p2->registers, RDMISCCONTROL_REG,
								p2->p2_rdmisccontrol.all);
						}
						SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
					}
					break;
				}

				case RAD4_RECT_READ:
					copyin(arg, &d_rrws, sizeof(rad4_rect_write));
#if !defined IRIX63
        			if (user_info.uabi_szptr == 4)  /* he is a 32 bit user */
			        {
						d_rrws.buf = (unsigned int *)*(((int *)&d_rrws) + 6);
					 	/* the pointer is in the 6th int position in 32 bit land */
						/* the pointer is in the 6th and 7th int position in 64 bit land*/
						/* so we have to get it out of the pad space */
			        }
#endif /*!defined IRIX63 */

#ifdef GET_TO_THIS_EVENTUALLY
					if (type == RAD1BIG_TYPE)
						rect_bigx_move(p2, &d_rrws, UIO_READ);
					else
#endif /* GET_TO_THIS_EVENTUALLY */
#ifdef USE_PER2_DMA
						rect_dma_move(p2, &d_rrws, UIO_READ);	/* */
#else /*  USE_PER2_DMA */
						rect_pio_move(p2, &d_rrws, UIO_READ);
#endif /*  USE_PER2_DMA */
					break;
				case RAD4_RECT_WRITE:
				{
					copyin(arg, &d_rrws, sizeof(rad4_rect_write));
#if !defined IRIX63
					if (user_info.uabi_szptr == 4)	 /* he is a 32 bit user */
					{	d_rrws.buf = (unsigned int *)*(((int *)&d_rrws) + 6);
						/* the pointer is in the 6th int position in 32 bit land */
						/* the pointer is in the 6th and 7th int position in 64 bit land*/
						/* so we have to get it out of the pad space */
					}
#endif /*!defined IRIX63 */
#ifdef GET_TO_THIS_EVENTUALLY
					if (type == RAD1BIG_TYPE)
						rect_bigx_move(p2, &d_rrws, UIO_WRITE);
					else
#endif /* GET_TO_THIS_EVENTUALLY */
#ifdef USE_PER2_DMA /* */
						rect_dma_move(p2, &d_rrws, UIO_WRITE);
#else /*  USE_PER2_DMA */
						rect_pio_move(p2, &d_rrws, UIO_WRITE);
#endif /*  USE_PER2_DMA */
					break;
				}
				case RAD4_RECT_FILL:
				{
					if (type == RAD1BIG_TYPE)
						rect_bigx_fill(p2, (rad4_rect_fill *)arg);
					else
						rect_patch_fill(p2, (rad4_rect_fill *)arg);
					break;
				}

				case RAD4_RECT_COPY:
				{
#ifdef GET_TO_THIS_EVENTUALLY
					if (type == RAD1BIG_TYPE)
						rect_bigx_copy(p2, (rad4_rect_copy *)arg);
					else
#endif /* GET_TO_THIS_EVENTUALLY */
						rect_patch_copy(p2, (rad4_rect_copy *)arg);
					break;
				}

				case RAD4_SET_MASK:
					if ((__psint_t)arg != p2->mask)	/* do only if mask is changing */
					{
						if(type != RAD1BIG_TYPE)
						{
							/* set the bits to write */
							SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
							SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
							p2->mask = (__psint_t)arg;
							*(p2->registers + BYPASSWRITEMASK_REG/4) = (__psint_t)arg;
							SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
							SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
						}
						else
						{
							SLP_LOCK_LOCK(&p2->bigx_mutex_lock); 	/* take lock */
							bigx_update(p2, 0, 0, bigx_x, bigx_y, UIO_WRITE);
								/* refresh VRAM from buffer */
							bigx_update(p2, 0, 0, bigx_x, bigx_y, UIO_READ);
								/* copy VRAM back to buffer */
							/* change masks */
							SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
							for (i = 0; i < number_of_per2s; i++)
							{
								lp2 = bigx_p2s[i];
								SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);	/* lock the hardware */
								*(lp2->registers + BYPASSWRITEMASK_REG/4) = 
									(__psint_t)arg;
								lp2->mask = (__psint_t)arg;
								SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);	/* release the hardware */
							}
							p2->mask = (__psint_t)arg;
							SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
							SLP_LOCK_UNLOCK(&p2->bigx_mutex_lock);	/* release lock */
						}
					}
					break;

				case RAD4_WRITE_EEPROM_SECTOR:
				{
					unsigned char buffer[RAD1_SECTOR_SIZE]; 

					copyin(arg, &sector, sizeof(rad4_eeprom_sector));
#if !defined IRIX63
					if(user_info.uabi_szptr == 4)	 /* 32 bit user */
						sector.data = (unsigned char *)*(((int *)&sector) + 1);
#endif /*!defined IRIX63 */

					copyin((caddr_t)sector.data, (caddr_t)buffer, RAD1_SECTOR_SIZE);

					return program_eeprom_sector(p2, sector.offset, buffer);
				}

				case RAD4_READ_EEPROM_SECTOR:
				{
					unsigned int offset;
					unsigned char buffer[RAD1_SECTOR_SIZE]; 
					unsigned char *eeprom_base = (unsigned char *)p2->ram2;

					copyin(arg, &sector, sizeof(rad4_eeprom_sector));
#if !defined IRIX63
					if(user_info.uabi_szptr == 4)	 /* 32 bit user */
						sector.data = (unsigned char *)*(((int *)&sector) + 1);
#endif /*!defined IRIX63 */

					offset = sector.offset;
					if (offset & (RAD1_SECTOR_SIZE - 1) != 0)
					{
						cmn_err(CE_DEBUG,"Bad EEPROM offset %d\n", offset);
						return EINVAL;
					}

					SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
					INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock */
					for (i = 0; i < RAD1_SECTOR_SIZE; i++)
						buffer[i ^ 3] = eeprom_base[offset++];

					INT_LOCK_UNLOCK(&p2->per2_global_lock);	/* release the global lock */
					SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
					if (copyout((caddr_t)buffer, (caddr_t)sector.data, RAD1_SECTOR_SIZE))
						return EFAULT;
					break;
				}

				case RAD4_DAEMON: /* if we are the bigx buffer call the daemon */
					if (type == RAD1BIG_TYPE)
					{	daemon(p2, (__psint_t)arg);
					}
					break;

#ifdef GET_TO_THIS_EVENTUALLY
				case RAD4_SET_GAMMA_LUT: /* map the full lut, arg is an int pointer */
					SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
					/* set RGB561 to start at red gamma LUT */
					*p2->HwDeviceExtension->RGB561_Addr_L = RED_GAMMA_LUT;
					*p2->HwDeviceExtension->RGB561_Addr_H = RED_GAMMA_LUT >> 8;
					for(i=0;i < 256;i++)
					{
						*p2->HwDeviceExtension->RGB561_Tables = 
							0x0ff & (arg[i] >> 2);  /* msB */
						*p2->HwDeviceExtension->RGB561_Tables = 
							0x0ff & (arg[i] << 6);  /* lsb */
					}

					/* set RGB561 to start at green gamma LUT */
					*p2->HwDeviceExtension->RGB561_Addr_L = GREEN_GAMMA_LUT;
					*p2->HwDeviceExtension->RGB561_Addr_H = GREEN_GAMMA_LUT >> 8;
					for(i=0;i < 256;i++)
					{
						*p2->HwDeviceExtension->RGB561_Tables = 
							0x0ff & (arg[i] >> 2);  /* msB */
						*p2->HwDeviceExtension->RGB561_Tables = 
							0x0ff & (arg[i] << 6);  /* lsb */
					}

					/* set RGB561 to start at blue gamma LUT */
					*p2->HwDeviceExtension->RGB561_Addr_L = BLUE_GAMMA_LUT;
					*p2->HwDeviceExtension->RGB561_Addr_H = BLUE_GAMMA_LUT >> 8;
					for(i=0;i < 256;i++)
					{
						*p2->HwDeviceExtension->RGB561_Tables = 
							0x0ff & (arg[i] >> 2);  /* msB */
						*p2->HwDeviceExtension->RGB561_Tables = 
							0x0ff & (arg[i] << 6);  /* lsb */
					}
					SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
					break;
#endif /* GET_TO_THIS_EVENTUALLY */
			}
			break;
#ifdef RAD1_CONSOLE
			
		case RAD1TTY_TYPE:
/*cmn_err(CE_DEBUG,"rad1pciioctl: 0x%x\n", cmd);	* */
			switch (cmd)
			{
				case TCGETA:
					rad1_tty_fill_termio((struct termio *)arg);
					break;
				case __OLD_TCGETA:	/* */
					rad1_tty_fill_old_termio((struct __old_termio *)arg);
					break;
				case TCGETS:
					rad1_tty_fill_termios((struct termios *)arg);
					break;
				case __OLD_TCGETS:	/* */
					rad1_tty_fill_old_termios((struct __old_termios *)arg);
					break;
				case TCSETA:
				case TCSETAW:
				case TCSETAF:
					rad1_tty_set_termio((struct termio *)arg, cmd);
					break;
				case TCSETS:
				case TCSETSW:
				case TCSETSF:
					rad1_tty_set_termios((struct termios *)arg, cmd);
					break;
				case TCSANOW:
				case TCSADRAIN:
				case TCSAFLUSH:
					rad1_tty_set_old_termios((struct __old_termios *)arg, cmd);
					break;
				case STGET:
					return -1;
				case TIOCGWINSZ:
					rad1_tty_fill_winsize((struct winsize *)arg);
					break;
				case TIOCSPGRP:
					rad1_tty_set_pgrp((ulong_t *)arg);
					break;
				case TIOCGPGRP:
					*((ulong_t *)arg) = rad1_tty_get_pgrp();
					break;
				case TIOCSSID:
					rad1_tty_set_psid((ulong_t *)arg);
					break;
				case TIOCGSID:
					*((ulong_t *)arg) = rad1_tty_get_psid();
					break;
				case TCFLSH:
					rad1_tty_flush((int)arg);
					break;
				case LDSETT:
					break;
				default:
	cmn_err(CE_DEBUG,"rad1pciioctl: 0x%x unknown\n", cmd);	/* */
/*					*rvalp = EINVAL;
					return EINVAL;	* */
			}
			break;
#endif /*  RAD1_CONSOLE */
	}
	return 0;
}

int rad1pciclose (dev_t dev, int flag, int otyp, cred_t *crp)
{
	per2_info *p2;
	per2serial_info *tempp2;
	vertex_hdl_t vrtxh;
	int type, ret;
	unsigned int major,minor;

#ifdef COMMENT 
	if(per2_opened == 0)
	{ cmn_err(CE_DEBUG,"close: rad1 not open\n");
		 return EIO;
	}
	else 
	{	per2_opened--;
		return 0;
	}
#endif /*  COMMENT  */

#ifdef IRIX63
	minor = dev & 0xf;	/* its said to be wrong but it works */
	dev = dev & ~0xf;	/* its said to be wrong but it works */
#endif /* IRIX63 */
	if(!(vrtxh = dev_to_vhdl(dev)))
	{
		cmn_err(CE_DEBUG,"dev_to_vhdl in close returned 0\n");
		return ENXIO;
	}
	p2 = (per2_info *)device_info_get(vrtxh);
	if(p2 == NULL) return ENXIO;

#ifdef IRIX63
	type = minor;
#else /*  IRIX63 */
	type = p2->type;
	if(type != RAD1_TYPE)
	{
		tempp2 = (per2serial_info *)p2;
		p2 = tempp2->p2;
	}
#endif /*  IRIX63 */
	if(type == RAD1_TYPE)
	{	rad1_info_pointer = p2; /* let the textport work */
/*		tp_init(); * */
		return 0;
	}
#ifdef RAD1_CONSOLE
	if(type == RAD1TTY_TYPE)
	{
cmn_err(CE_DEBUG,"rad1pciclose: tty device\n"); /* */
		rad1_tty_close(p2);
cmn_err(CE_DEBUG,"rad1pciclose: call gl_setporthandler 0\n"); /* */
		gl_setporthandler(-1, NULL);
	}
#endif /*  RAD1_CONSOLE */

	return 0;
}

int rad1pciopen (dev_t *devp, int oflag, int otyp, cred_t *crp)
{
	per2_info *p2;
	per2serial_info *tempp2;
	vertex_hdl_t vrtxh;
	int type, ret;
	unsigned int major,minor;
	dev_t maj_dev;
#ifdef RAD1_TEXTPORTONLY
	return EIO;
#else /* RAD1_TEXTPORTONLY */

	if(!found_per2)
		return EIO;

#ifdef   COMMENT 
	if(per2_opened != 0)
	{
		cmn_err(CE_DEBUG,"open: rad1 already open rad1opened = 0x%x\n",per2_opened);
/*		return EIO; * */
		return 0; /* */
	}
	else 
	{	per2_opened++;
		return 0;
	}
#endif /*    COMMENT  */
#ifdef IRIX63
	major = getmajor(*devp); /* this does not work ?!?! */
	minor = getminor(*devp);
	maj_dev = makedevice(major,0); /* make the ,0 device */
/*	cmn_err(CE_DEBUG,"open dev = 0x%x major = 0x%x minor = 0x%x makedev = 0x%x\n",
		*devp,major,minor,maj_dev); * */
	minor = *devp & 0xf;	/* its said to be wrong but it works */
	maj_dev = (*devp) & ~0xf;	/* its said to be wrong but it works */
#else /* IRIX63 */
	maj_dev = *devp;
#endif /* IRIX63 */
	if(!(vrtxh = dev_to_vhdl(maj_dev)))
	{
		cmn_err(CE_DEBUG,"dev_to_vhdl in open returned 0\n");
		return ENXIO;
	}
	p2 = (per2_info *)device_info_get(vrtxh);
	if(p2 == NULL)
	{	cmn_err(CE_DEBUG,"device_info_get in open returned NULL\n");
		return ENXIO;
	}

#ifdef IRIX63
	type = minor;
#else /*  IRIX63 */
	type = p2->type;
	if(type != RAD1_TYPE)
	{
		tempp2 = (per2serial_info *)p2;
		p2 = tempp2->p2;
	}
#endif /*  IRIX63 */
/*	cmn_err(CE_DEBUG,"rad1pciopen: %d\n", type); * */
	if(type == RAD1_TYPE)
	{
		p2->tty_initted = FALSE;
/* 		cmn_err(CE_DEBUG,"DMA to %d\n",
					2 + drv_usectohz((DMA_TIMEOUT_US + rad1pci_max_dma_area/20))); * */
		p2->tbuf = tbuf + (p2->board_number * RAD1_SGRAM_SIZE/4);
/*		cmn_err(CE_DEBUG,"rad1pciopen: %d tbuf = 0x%x\n", type,p2->tbuf); * */
		rad1_info_pointer = NULL; /* dont let the textport work */
		return 0;
	}
	if(type == RAD1BIG_TYPE)
	{
		p2->tty_initted = FALSE;
/* 		cmn_err(CE_DEBUG,"DMA to %d\n",
					2 + drv_usectohz((DMA_TIMEOUT_US + rad1pci_max_dma_area/20))); * */
		p2->tbuf = tbuf;
/*		cmn_err(CE_DEBUG,"rad1pciopen: %d tbuf = 0x%x\n", type,p2->tbuf); * */
		return 0;
	}
#ifdef RAD1_CONSOLE
	if(type == RAD1TTY_TYPE)
	{
cmn_err(CE_DEBUG,"rad1pciopen: tty device\n"); /* */
		rad1_tty_init(p2);	/* initialize tty */
cmn_err(CE_DEBUG,"rad1pciopen: call gl_setporthandler, 0x%x\n", rad1_native_kybd_in); /* */
		gl_setporthandler(-1, rad1_native_kybd_in);
	}
#endif /*  RAD1_CONSOLE */

	return 0;
#endif /* RAD1_TEXTPORTONLY */
}

void rect_pio_move(per2_info *p2, rad4_rect_write *rw, int direction)
{
	int i, j;
	int offset;
	int lcount;
	caddr_t base;
	int t_status;
	int t_count;
	unsigned int *rbuf;
	int *registers = p2->registers;

	if (((rw->startx + rw->sizex) > p2->v_info.x_size) ||
		((rw->starty + rw->sizey) > p2->v_info.y_size) ||
		(rw->sizex <= 0) || (rw->sizey <= 0) ||
		(rw->startx < 0) || (rw->starty < 0))
	{
		cmn_err(CE_DEBUG,"rect_pio_move sx = 0x%x sy = 0x%x d = %d\n",
			rw->startx,rw->starty,direction);
		cmn_err(CE_DEBUG," x = 0x%x y = 0x%x\n",rw->sizex,rw->sizey);
		return;
	}
	rbuf = (unsigned int *)p2->tbuf;
	t_count = rw->sizex * rw->sizey * 4;
	if(direction == UIO_WRITE)
	{
		if(0 != copyin(rw->buf, rbuf, t_count))
		{
			cmn_err(CE_DEBUG,"pio rect move copyin error\n");
			return;
		}
	}
	SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
	if(direction == UIO_WRITE)
	{
		WAIT_IN_FIFO(10);	/* wait for room in register FIFO */
#ifdef USING_HARDWARE_MASK
		*(registers + FBHARDWAREWRITEMASK_REG/4) = rw->mask;
		p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
#else /* USING_HARDWARE_MASK */
		*(registers + FBSOFTWAREWRITEMASK_REG/4) = rw->mask;
		p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_ENABLE;
#endif /* USING_HARDWARE_MASK */
		*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
		*(registers + RECTANGLEORIGIN_REG/4) =
			RECTORIGIN_YX(rw->starty, rw->startx);
		*(registers + RECTANGLESIZE_REG/4) = RECTSIZE_HW(rw->sizey, rw->sizex);
		*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE |
			RENDER_INCREASEX | RENDER_INCREASEY | RENDER_SYNCONHOSTDATA;

		for (i = 0; i < rw->sizey * rw->sizex; i++)
		{
			WAIT_IN_FIFO(1);	/* wait for room in register FIFO */
			*(registers + COLOR_REG/4) = rbuf[i];
		}

		WAIT_IN_FIFO(3);	/* wait for room in register FIFO */
#ifdef USING_HARDWARE_MASK
		*(registers + FBHARDWAREWRITEMASK_REG/4) = p2->hard_mask;
#else /* USING_HARDWARE_MASK */
		*(registers + FBSOFTWAREWRITEMASK_REG/4) = p2->hard_mask;
		p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
		*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
#endif /* USING_HARDWARE_MASK */
	}
	else	/* if(direction == UIO_READ) */
	{ 
		WAIT_IN_FIFO(20);	/* wait for room in register FIFO */
		p2->p2_alphablendmode.bits.operation = OPERATION_FORMAT;
		p2->p2_alphablendmode.bits.alphaBlendEnable = RAD1_ENABLE;
		*(registers + ALPHABLENDMODE_REG/4) = p2->p2_alphablendmode.all;	/* */
		p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_ENABLE;
		p2->p2_logicalopmode.bits.logicOp = LOGICOP_COPY;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
		p2->p2_fbwritemode.bits.writeEnable = RAD1_DISABLE;
		p2->p2_fbwritemode.bits.upLoadData = UPLOADCOLORTOHOST;
		*(registers + FBWRITEMODE_REG/4) = p2->p2_fbwritemode.all;
		p2->p2_fbreadmode.bits.readSourceEnable = RAD1_DISABLE;
		p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_ENABLE;
		p2->p2_fbreadmode.bits.dataType = DATATYPE_FBDEFAULT;
/*		p2->p2_fbreadmode.bits.dataType = DATATYPE_FBCOLOR;	* */
		p2->p2_fbreadmode.bits.packedData = RAD1_ENABLE;	/* */
/*		p2->p2_fbreadmode.bits.packedData = RAD1_DISABLE;	* */
		*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
		/* set output filter for color data only */
		p2->p2_filtermode.all = 0;
		p2->p2_filtermode.bits.colorData = RAD1_ENABLE;
		*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;

		p2->time_happened = 0;
		p2->dma_to_cookie = itimeout(rad1_to_func, p2->dma_rd_wait_on_it,
			2 + drv_usectohz(DMA_TIMEOUT_US), pltimeout, p2);

		/* tell GC to start reading out rectangle */
		*(registers + RECTANGLEORIGIN_REG/4) =
			RECTORIGIN_YX(rw->starty, rw->startx);
		*(registers + RECTANGLESIZE_REG/4) = RECTSIZE_HW(rw->sizey, rw->sizex);
delay(1);	/* */
		*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE |
			RENDER_INCREASEX | RENDER_INCREASEY;

		for (j = 0; j < 3; j++)
		{
			while (*(registers + OUTFIFOWORDS_REG/4) == 0)
			{
				delay(0);
				if (p2->time_happened > j)
					break;
/*				cmn_err(CE_DEBUG,"rect_pio_move waiting for time out\n");	* */
			}
			if (*(registers + OUTFIFOWORDS_REG/4))
			{
				for (i = 0; i < rw->sizey * rw->sizex; i++)
				{
					while ((lcount = *(registers + OUTFIFOWORDS_REG/4)) == 0)
					{
						if (p2->time_happened > j)
						{
							cmn_err(CE_DEBUG,"rect_pio_move ran out of data ft4 %d of %d\n",
								i,rw->sizey * rw->sizex);
							i = rw->sizey * rw->sizex +1;
							break;
						}
						delay(0);
					}
					if (p2->time_happened > j)
						break;
					rbuf[i] = *(p2->registers + OUT_FIFO/4);
				}
				break;
			}
			else
			{
				if (rw->sizex > 1)
					cmn_err(CE_DEBUG,"rect_pio_move timed out %d\n", j);
			}
/*			p2->time_happened = 0;	* */
			p2->dma_to_cookie = itimeout(rad1_to_func, p2->dma_rd_wait_on_it,
				2 + drv_usectohz(DMA_TIMEOUT_US), pltimeout, p2);
			*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE |
				RENDER_INCREASEX | RENDER_INCREASEY;
		}

		untimeout(p2->dma_to_cookie);
		if (*(registers + OUTFIFOWORDS_REG/4))
		{
			i = 0;
			while (*(registers + OUTFIFOWORDS_REG/4))
			{
				lcount = *(p2->registers + OUT_FIFO/4);
				if ((i < 8) && (rw->sizex > 1))
					cmn_err(CE_DEBUG,"%x ", lcount);
				i++;
			}
			if (rw->sizex > 1)
				cmn_err(CE_DEBUG,"\nrect_pio_move h,w=%d,%d flushed %d words last was 0x%x\n",
					rw->sizey, rw->sizex, i, lcount);
		}

		p2->p2_alphablendmode.bits.alphaBlendEnable = RAD1_DISABLE;
		*(registers + ALPHABLENDMODE_REG/4) = p2->p2_alphablendmode.all;
		p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
		p2->p2_fbwritemode.bits.upLoadData = NOUPLOAD;
		p2->p2_fbwritemode.bits.writeEnable = RAD1_ENABLE;
		*(registers + FBWRITEMODE_REG/4) = p2->p2_fbwritemode.all;
		p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
		p2->p2_fbreadmode.bits.dataType = DATATYPE_FBDEFAULT;
		*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
		p2->p2_filtermode.all = 0;
		*(p2->registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;
	}
	if (direction != UIO_WRITE)
	{
		if (0 != copyout(rbuf, rw->buf, t_count))
		{
			cmn_err(CE_DEBUG,
"rect_pio_move:error in copyout r_buf = 0x%x rw->buf = 0x%x t_count = 0x%x\n",
				rbuf,rw->buf,t_count);
		}
	}
	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
	SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
	return ;
}


#ifdef GET_TO_THIS_EVENTUALLY
void rect_bigx_move(per2_info *p2, rad4_rect_write *rw, int direction)
{
	int i, j;
	unsigned int *offset;
	int board, bigx_step, lx, lw;
	int x, y, w, h;
	unsigned int *base, *source, *buf, *fb;
	int mask, not_mask;
	per2_info *lp2;
	rad4_rect_write rect;

	x = rw->startx;
	y = rw->starty;
	w = rw->sizex;
	h = rw->sizey;

	if (!p2->bigx_daemon_stall)
	{
		/* hold off daemon for a while */
		SLP_LOCK_LOCK(&p2->bigx_mutex_lock);  	/* take lock */
		p2->bigx_daemon_stall = TRUE;
		p2->bigx_cookie = itimeout(per2_bigx_to_func, &p2->bigx_daemon_stall,
			drv_usectohz(DAEMON_STALL_USEC * DAEMON_STALL_LOOPS), pltimeout, p2);
		SLP_LOCK_UNLOCK(&p2->bigx_mutex_lock); 	/* release lock */
	}

	if(direction == UIO_WRITE)
	{
		mask = rw->mask;

		if (mask == p2->mask)
		{
			/* mask is the same */
	 		offset = (unsigned int *)(p2->tbuf + x + y * bigx_x);
			base = rw->buf;
			for(i = 0; i < h; i++)
			{
				fb = offset;
				for(j = 0; j < w; j++)
				{
					*fb = *base;
					base++;
					fb++;
				}
				offset += bigx_x;
			}
		}
		else
		{
			bigx_step = bigx_x / number_of_per2s;
			buf = rw->buf;
			rect.mask = mask;
			rect.starty = y;
			rect.sizey = h;
			rect.mode = RGB32;

			for (board = 0; board < number_of_per2s; board++)
			{
				if ((x + w) < board*bigx_step)
					break;	/* no more to do */
				if (x >= (board+1)*bigx_step)
					continue; /* region is past this board */
				if (x < board*bigx_step)
				{
					/* region overlaps left edge of this board */
					lx = 0;
					lw = w - (board*bigx_step - x);
				}
				else
				{
					lx = x - board*bigx_step;
					lw = w;
				}

				if ((x + w) > (board+1)*bigx_step)
 					lw -= (x + w) - (board+1)*bigx_step;

				if (lw == 0 || h == 0)
					continue;

				lp2 = bigx_p2s[board];

				if (lw == w)
				{
					/* region fits entirely within this board, do DMA */
					rect.startx = lx;
					rect.sizex = lw;
					rect.buf = buf;
					SLP_LOCK_LOCK(&lp2->dma_sleep_lock);  	/* take lock */
					rect_dma_move(lp2, &rect, direction);	/* use DMA to fill VRAM */
					SLP_LOCK_UNLOCK(&lp2->dma_sleep_lock); 	/* release lock */
				}
/* else what? */
				buf += lw;
			}

			if (mask & p2->mask != 0)
			{
				/* new mask and old mask not completely different */
				not_mask = ~mask;
 				offset = (unsigned int *)(p2->tbuf + x + y * bigx_x);
				base = rw->buf;

				for(i = 0; i < h; i++)
				{
					fb = offset;
					for(j = 0; j < w; j++)
					{
						*fb = (not_mask & *fb) | (mask & *base);
						base++;
						fb++;
					}
					offset += bigx_x;
				}
			}
		}
	}
	else	/* direction == UIO_READ */
	{
		/* update VRAM before reading */
		SLP_LOCK_LOCK(&p2->bigx_mutex_lock); 	/* take lock */
		bigx_update(p2, x, y, w, h, UIO_WRITE);
			/* refresh VRAM from buffer */
		SLP_LOCK_UNLOCK(&p2->bigx_mutex_lock);	/* release lock */

		bigx_step = bigx_x / number_of_per2s;
		buf = rw->buf;

		for (board = 0; board < number_of_per2s; board++)
		{
			if ((x + w) < board*bigx_step)
				break;	/* no more to do */
			if (x >= (board+1)*bigx_step)
				continue; /* region is past this board */
			if (x < board*bigx_step)
			{
				/* region overlaps left edge of this board */
				lx = 0;
				lw = w - (board*bigx_step - x);
			}
			else
			{
				lx = x - board*bigx_step;
				lw = w;
			}

			if (x + w > (board+1)*bigx_step)
 				lw -= (x + w) - (board+1)*bigx_step;

			if (lw == 0 || h == 0)
				continue;

			lp2 = bigx_p2s[board];

			if (lw == w)
			{
				/* region fits entirely within this board, do DMA */
				rect.mask = mask;
				rect.starty = y;
				rect.sizey = h;
				rect.startx = lx;
				rect.sizex = w;
				rect.buf = buf;
				SLP_LOCK_LOCK(&lp2->dma_sleep_lock);  	/* take lock */
				rect_dma_move(lp2, &rect, direction);	/* use DMA to read VRAM */
				SLP_LOCK_UNLOCK(&lp2->dma_sleep_lock); 	/* release lock */
				break;
			}

			offset = (unsigned int *)(lp2->ram1 + lx + y * lp2->v_info.x_size);
			base = buf;
			SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);	/* lock the hardware */
			for(i = 0; i < h; i++)
			{
				fb = base;
				source = offset;
				for(j = 0; j < lw; j++)
				{
					*fb = *source;
					source++;
					fb++;
				}
				offset += lp2->v_info.x_size;
				base += w;
			}
			SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);	/* release the hardware */
			buf += lw;
		}
	}

	return;
}
#endif /* GET_TO_THIS_EVENTUALLY */

void rect_pio_fill(per2_info *p2, rad4_rect_fill *rf)
{	
	int i,j;
	int *offset, *fb;

	SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
	*(p2->registers + BYPASSWRITEMASK_REG/4) = rf->mask;	/* set the bits to write */

 	offset = (int *)(p2->ram1 + rf->starty*p2->v_info.x_size + rf->startx);

	for(i=0; i<rf->sizey; i++)
	{
		fb = offset;
		for(j=0; j<rf->sizex; j++)
		{
			*fb = rf->value;
			fb++;
		}
		offset += p2->v_info.x_size;
	} /* for */
	*(p2->registers + BYPASSWRITEMASK_REG/4) = p2->mask ; /* restore the mask */
	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
	SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
}

void rect_patch_fill(per2_info *p2, rad4_rect_fill *rf)
{	
	int *registers = p2->registers;

	if (((rf->startx + rf->sizex) > p2->v_info.x_size) ||
		((rf->starty + rf->sizey) > p2->v_info.y_size) ||
		(rf->sizex <= 0) || (rf->sizey <= 0) ||
		(rf->startx < 0) || (rf->starty < 0))
		return;

	WAIT_IN_FIFO(14);	/* wait for room in register FIFO */
		
	SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
	/* set output filter for sync data only */
	p2->p2_filtermode.all = 0;
	p2->p2_filtermode.bits.synchronizationData = RAD1_ENABLE;
	*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;

	INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock (disables interrupts) */
	p2->time_happened = 0; /* so we can tell if this is a new time out */
	p2->sync_to_cookie = itimeout(rad1_to_func, p2->sync_wait_on_it,
		2 + drv_usectohz(DMA_TIMEOUT_US), pltimeout, p2);

#ifdef USING_HARDWARE_MASK
	*(registers + FBHARDWAREWRITEMASK_REG/4) = rf->mask;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
#else /* USING_HARDWARE_MASK */
	*(registers + FBSOFTWAREWRITEMASK_REG/4) = rf->mask;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_ENABLE;
#endif /* USING_HARDWARE_MASK */
	p2->p2_fbreadmode.bits.readSourceEnable = RAD1_DISABLE;
	*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;

	p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
	p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_ENABLE;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
	*(registers + FBWRITEDATA_REG/4) = rf->value;
	*(registers + FBBLOCKCOLOR_REG/4) = rf->value;

	*(registers + RECTANGLEORIGIN_REG/4) = RECTORIGIN_YX(rf->starty, rf->startx);
	*(registers + RECTANGLESIZE_REG/4) = RECTSIZE_HW(rf->sizey, rf->sizex);
	*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEX |
		RENDER_INCREASEY | RENDER_FASTFILLENABLE;	/* */

	*(registers + WAITFORCOMPLETION_REG/4) = 0;
	*(registers + SYNC_REG/4) = SYNC_INT_EN | 0x55aa33cc;
	/* wait for render to complete */
	INT_LOCK_SV_WAIT(p2->sync_wait_on_it, &p2->per2_global_lock);
		/* releases per2_global_lock and enables interrupts */
	untimeout(p2->sync_to_cookie);
	if (p2->time_happened)
	{ int temp;
			temp = *(p2->registers + OUT_FIFO/4);
		cmn_err(CE_DEBUG,"rect_patch_fill timed out %d:%d %d by %d mask 0x%x value 0x%x of 0x%x\n",
			rf->startx, rf->starty, rf->sizex, rf->sizey, rf->mask, rf->value,temp);
		cmn_err(CE_DEBUG,"if 0x%x ie 0x%x iff 0x%x off 0x%x\n",
			*(p2->registers + INTFLAGS_REG/4),*(p2->registers + INTENABLE_REG/4),
			*(p2->registers + INFIFOSPACE_REG/4),*(p2->registers + OUTFIFOWORDS_REG/4));
	}

#ifdef USING_HARDWARE_MASK
	*(registers + FBHARDWAREWRITEMASK_REG/4) = p2->hard_mask;
#else /* USING_HARDWARE_MASK */
	*(registers + FBSOFTWAREWRITEMASK_REG/4) = p2->hard_mask;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
	*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
#endif /* USING_HARDWARE_MASK */
	p2->p2_filtermode.all = 0;
	*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;
	p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_DISABLE;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
	SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
}

void rect_bigx_fill(per2_info *p2, rad4_rect_fill *rf)
{	
	int i, j;
	int *offset;
	int board, bigx_step, lx, lw;
	int x, y, w, h;
	int mask, not_mask, value;
	int *fb;
	per2_info *lp2;
	rad4_rect_fill fill;

	mask = rf->mask;
	x = rf->startx;
	y = rf->starty;
	w = rf->sizex;
	h = rf->sizey;
	value = rf->value;
	offset = p2->tbuf + x + y * bigx_x;	/* initial buffer offset */

	if (!p2->bigx_daemon_stall)
	{
		/* hold off daemon for a while */
		SLP_LOCK_LOCK(&p2->bigx_mutex_lock);  	/* take lock */
		p2->bigx_daemon_stall = TRUE;
		p2->bigx_cookie = itimeout(per2_bigx_to_func, &p2->bigx_daemon_stall,
			drv_usectohz(DAEMON_STALL_USEC * DAEMON_STALL_LOOPS), pltimeout, p2);
		SLP_LOCK_UNLOCK(&p2->bigx_mutex_lock); 	/* release lock */
	}

	if (mask == p2->mask)
	{
		/* mask is the same */
		for(i = 0; i < h; i++)
		{
			fb = offset;
			for(j = 0; j < w; j++)
			{
				*fb = value;
				fb++;
			}
			offset += bigx_x;
		}
	}
	else
	{
		/* mask is different */
		fill.mask = mask;
		fill.value = value;
		fill.starty = y;
		fill.sizey = h;
		bigx_step = bigx_x / number_of_per2s;

		for (board = 0; board < number_of_per2s; board++)
		{
			if ((x + w) < board*bigx_step)
				break;	/* no more to do */
			if (x >= (board+1)*bigx_step)
				continue; /* region is past this board */
			if (x < board*bigx_step)
			{
				/* region overlaps left edge of this board */
				lx = 0;
				lw = w - (board*bigx_step - x);
			}
			else
			{
				lx = x - board*bigx_step;
				lw = w;
			}
			if (x + w > (board+1)*bigx_step)
 				lw -= (x + w) - (board+1)*bigx_step;

			if (lw == 0 || h == 0)
				continue;

			lp2 = bigx_p2s[board];

			fill.startx = lx;
			fill.sizex = lw;
			SLP_LOCK_LOCK(&lp2->dma_sleep_lock);  	/* take lock */
			rect_patch_fill(lp2, &fill);
			SLP_LOCK_UNLOCK(&lp2->dma_sleep_lock); 	/* release lock */
		}

		if (mask & p2->mask != 0)
		{
			/* new mask and old mask not completely different */
			not_mask = ~mask;
			value &= mask;

			for(i = 0; i < h; i++)
			{
				fb = offset;
				for(j = 0; j < w; j++)
				{
					*fb = (not_mask & *fb) | value;
					fb++;
				}
				offset += bigx_x;
			}
		}
	}
}

#ifdef USE_PER2_DMA
void rect_dma_move(per2_info *p2, rad4_rect_write *rw, int direction)
{
	int i;
	int lcount;
	paddr_t phys_ptr, dma_ptr;
	paddr_t n_phys_ptr;
	paddr_t base;
	int t_count, ii;
	int t_sizey, l_sizey, m_sizey;
	int t_sizex, l_sizex, m_sizex;
	int userdma_rv;
	caddr_t r_buf;
	int tw, ltw;
	int map_used = FALSE;
	int b_per_p,stride;

/*	if ((direction == UIO_WRITE) && ((rw->mask & 0x00ffffff) != 0x00ffffff))
		return;	* */
	if (((rw->startx + rw->sizex) > p2->v_info.x_size) ||
		((rw->starty + rw->sizey) > p2->v_info.y_size) ||
		(rw->sizex <= 0) || (rw->sizey <= 0) ||
		(rw->startx < 0) || (rw->starty < 0))
		return;
	if ( !((rw->mode == CI8) || (rw->mode == RGB15) ||
		(rw->mode == RGB24) || (rw->mode == RGB32)) )
	{	
		cmn_err(CE_DEBUG, "startx = %d starty = 0x%x\n",rw->startx,rw->starty);/* */
		cmn_err(CE_DEBUG, "sizex = %d sizey = 0x%x\n",rw->sizex,rw->sizey);/* */
		cmn_err(CE_DEBUG, "mode = %d mask = 0x%x buf = 0x%x\n",rw->mode,rw->mask,rw->buf);/* */
		return;
	}
#if 0
	if (((rw->sizex*rw->sizey) <= PER2_MIN_DMA_TOTAL)||
		(!p2->dma_enabled))	
	{	/* it takes this many accesses to set up the dma */
		rect_pio_move(p2, rw, direction);
		return;
	}
	if (direction != UIO_WRITE)
	{ rect_pio_move(p2, rw, direction);
		return;
	}
#endif

	t_count = rw->sizex * rw->sizey * 4;
	if(
#if 1 /* for now we will use our buffer */
			FALSE &&
#endif
			rad1pci_use_userbuffer == 1) /* config option to not use maputokv */
	{
		/* dont believe the man page, userdma does not do this on the 10k */
		dki_dcache_wbinval(rw->buf,t_count);
		userdma_rv = userdma(rw->buf, t_count,
				((direction == UIO_WRITE)?B_READ:B_WRITE)
#ifdef IRIX63
		);
		/* cmn_err(CE_DEBUG, "userdma returned 0x%x \n",userdma_rv);* */
		if( userdma_rv != 0)
		{ userdma_rv = 0; /* 6.3 and 6.4  are oppisit */
		}
		else
		{	if(u.u_error==0)
			{
				/* cmn_err(CE_DEBUG,"u.u_error = 0,userdma_rv = 0\n"); * */
				userdma_rv = -1;
			}
			else
			{
				userdma_rv = u.u_error;
				u.u_error = 0;	/* dont let this get back to the user */
			}
		}
#else /* IRIX63 */
		,NULL);
#endif /* IRIX63 */
	}
	else /* rad1pci_use_userbuffer */
	{
		userdma_rv = -1;
	}
	if(userdma_rv == 0) 	/* we might be able to use the users buffer */
	{
		r_buf = maputokv((caddr_t)rw->buf,t_count,
				((direction == UIO_WRITE)?B_READ:B_WRITE)); /* remap to k space */
		if(r_buf == NULL)
		{
			cmn_err(CE_DEBUG,"maputokv returned 0\n");
			undma((caddr_t)rw->buf,t_count,((direction == UIO_WRITE)?B_READ:B_WRITE));
			userdma_rv = -1; /* say we are not using the users buffer */
		}
	}
	if(userdma_rv != 0)	/* we can't use the users buffer */
	{	 /* cmn_err(CE_DEBUG,
			"userdma failed 0x%x copying the buffer\n",userdma_rv);* */
		r_buf = (caddr_t)p2->tbuf;
		if(direction == UIO_WRITE)
		{
			if(0 != copyin(rw->buf, r_buf, t_count))
			{
				rect_pio_move(p2, rw, direction);
				return;
			}
			dki_dcache_wb(r_buf, t_count); /* */
		}
		else
			dki_dcache_inval(r_buf, t_count);
	}
	n_phys_ptr = kvtophys(r_buf); /* n_phys_ptr is checked b4 its used */
	if(n_phys_ptr == NULL)
		cmn_err(CE_DEBUG,"kvtophys line %d returned NULL for kv 0x%x\n",__LINE__,r_buf);
#if 0
	dma_ptr = pciio_dmatrans_addr(p2->dev, &p2->dev_desc,
			n_phys_ptr, rw->sizex * rw->sizey * 4, pciio_dma_flags);
#else
		dma_ptr = NULL;
#endif
/*	cmn_err(CE_DEBUG,
			"dma rect write c = 0x%x y = 0x%x\n",rw->sizex,rw->sizey); * */
	switch(rw->mode)
	{	case RGB32:
		case RGB24:
			b_per_p = 4;
			stride = rw->sizex * 4;
			break;
		case RGB15:
			b_per_p = 2;
			stride = (((rw->sizex * 2) + 3) & ~3);	/* round up to 4 bytes */
			break;
		case CI8:
			b_per_p = 1;
			stride = ((rw->sizex + 3) & ~3);	/* round up to 4 bytes */
			break;
		default:
			return;	/* bad mode */
	}
	t_sizey = 0;
	m_sizey = (rad1pci_max_dma_area)/stride;	/*how many lines fit in a dma map*/
/* #define LIMIT_RUN * limit the run to a cache line */
#ifdef LIMIT_RUN
	m_sizex = (128/b_per_p);
	m_sizey = (rad1pci_max_dma_area)/stride;	/*how many lines fit in a dma map*/
		while(t_sizey < rw->sizey)
		{
			l_sizey = (m_sizey>(rw->sizey-t_sizey)) ? 
					(rw->sizey-t_sizey) : m_sizey;/* min */
			if(dma_ptr == NULL)
			{ if(p2->dma_map != NULL)
					if((dma_ptr = pciio_dmamap_addr(p2->dma_map, 
							n_phys_ptr+(t_sizey * stride),
							m_sizey * stride))
								!= NULL)
					{	map_used = TRUE;
					}
			}
			if(dma_ptr == NULL)
			{	cmn_err(CE_DEBUG,"rect_dma_move: trans and map failed\n"); /* */
				rect_pio_move(p2, rw, direction);
			}
			else
			{
				for(t_sizex = 0;t_sizex < rw->sizex;t_sizex += l_sizex)
				{
					l_sizex = (m_sizex>(rw->sizex-t_sizex)) ? 
						(rw->sizex-t_sizex) : m_sizex;/* min */

					rad1_dma_patch_move(p2, (int)dma_ptr+(t_sizex*b_per_p),
						rw->startx+t_sizex, rw->starty+t_sizey, 
						l_sizex, l_sizey, direction, rw->mask, rw->mode,stride);
				}
			}
			if(map_used == TRUE)
			{
				pciio_dmamap_done(p2->dma_map);
				map_used = FALSE;
				dma_ptr = NULL;
			}
			else
			{	dma_ptr += (l_sizey * stride);
			}
			t_sizey += l_sizey;
		}
#else	/* LIMIT_RUN */
	if(m_sizey == 0 )
	{	cmn_err(CE_DEBUG,"wont fit rad1pci_max_dma_area = 0x%x\n",
			rad1pci_max_dma_area);
		m_sizex = (rad1pci_max_dma_area/b_per_p) & ~0x3;
		for(;t_sizey < rw->sizey;t_sizey++)
		{
			if(dma_ptr == NULL)
			{/*	 cmn_err(CE_DEBUG,
						" rect_dma_move: pciio_dmatrans_addr failed\n"); * */
				if(p2->dma_map != NULL)
					if((dma_ptr = pciio_dmamap_addr(p2->dma_map, 
							n_phys_ptr+(t_sizey * stride),
							rw->sizex * b_per_p))
								!= NULL)
					{	map_used = TRUE;
		/*				cmn_err(CE_DEBUG,
									"rect_dma_move: dma_ptr = 0x%lx\n", dma_ptr); * */
					}
			}
			if(dma_ptr == NULL)
			{	cmn_err(CE_DEBUG,"rect_dma_move: trans and map failed\n"); /* */
				rect_pio_move(p2, rw, direction);
			}
			else
			{
				for(t_sizex = 0;t_sizex < rw->sizex;t_sizex += l_sizex)
				{
					l_sizex = (m_sizex>(rw->sizex-t_sizex)) ? 
						(rw->sizex-t_sizex) : m_sizex;/* min */


					rad1_dma_patch_move(p2, (int)dma_ptr+(t_sizex*b_per_p),
						rw->startx+t_sizex, rw->starty+t_sizey, 
						l_sizex, 1, direction, rw->mask, rw->mode,0);
				}
			}
			if(map_used == TRUE)
			{
				pciio_dmamap_done(p2->dma_map);
				map_used = FALSE;
				dma_ptr = NULL;
			}
			else
			{	dma_ptr +=  stride;
				/* round up the x size to 4 byte boundry */
			}
		}
	}
	else
	{
		while(t_sizey < rw->sizey)
		{
			l_sizey = (m_sizey>(rw->sizey-t_sizey)) ? 
					(rw->sizey-t_sizey) : m_sizey;/* min */

			if(dma_ptr == NULL)
			{/*	 cmn_err(CE_DEBUG,
						" rect_dma_move: pciio_dmatrans_addr failed\n"); * */
				if(p2->dma_map != NULL)
					if((dma_ptr = pciio_dmamap_addr(p2->dma_map, 
							n_phys_ptr+(t_sizey * stride),
							rw->sizex * l_sizey * 4))
								!= NULL)
					{	map_used = TRUE;
		/*				cmn_err(CE_DEBUG,
									"rect_dma_move: dma_ptr = 0x%lx\n", dma_ptr); * */
					}
			}
			if(dma_ptr == NULL)
			{	cmn_err(CE_DEBUG,"rect_dma_move: trans and map failed\n"); /* */
				rect_pio_move(p2, rw, direction);
			}
			else
			{
				rad1_dma_patch_move(p2, (int)dma_ptr,
					rw->startx, rw->starty+t_sizey, 
					rw->sizex, l_sizey, direction, rw->mask, rw->mode,0);
			}
			if(map_used == TRUE)
			{
				pciio_dmamap_done(p2->dma_map);
				map_used = FALSE;
				dma_ptr = NULL;
			}
			else
			{	dma_ptr += (l_sizey * stride);
				/* round up the x size to 4 byte boundry */
			}
			t_sizey += l_sizey;
		}	/* while(t_sizey < rw->sizey) */
	}
#endif	/* LIMIT_RUN */

#if 0 
	SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
	*p2->HwDeviceExtension->VRAM_Mask = p2->mask ; /* restore VRAM mask */
	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);
	SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
#endif

	if(userdma_rv != 0)
	{
		if(direction != UIO_WRITE)
		{
			if(0 != copyout(r_buf, rw->buf, t_count))
			{	cmn_err(CE_DEBUG,
	"rect_dma_move:error in copyout r_buf= 0x%x rw->buf= 0x%x t_count= 0x%x\n",
					r_buf,rw->buf,t_count);
			}
		}
	}
	else /* unlock the users buffer */
	{
		unmaputokv(r_buf,t_count);
		undma((caddr_t)rw->buf, t_count, 
			((direction == UIO_WRITE) ? B_READ : B_WRITE));
	}
	return;
}

#endif /*  USE_PER2_DMA */


#ifdef GET_TO_THIS_EVENTUALLY
void load_wid( per2_info *p2,int cmd,int start_addr,int id,int mask)
{
	int i,reg;
/*	cmn_err(CE_DEBUG,"lw cmd = 0x%x sa = 0x%x id = 0x%x mask = 0x%x\n",
		cmd,start_addr,id,mask); * */
	if((cmd == PER2_SETDEPTH8_INDEX) || (cmd == PER2_SETDEPTH8_GREY) || 
		(cmd == PER2_SETDEPTH8_TRUE) || (cmd == PER2_SETDEPTH8_DIRECT))
	{ reg = FB_WAT_PIXFORM_8BPP;
	}
	else if((cmd == PER2_SETDEPTH12_TRUE) || (cmd == PER2_SETDEPTH12_DIRECT))
	{ reg = FB_WAT_PIXFORM_12BPP;
	}
	else if((cmd == PER2_SETDEPTH16_TRUE) || (cmd == PER2_SETDEPTH16_DIRECT))
	{ reg = FB_WAT_PIXFORM_16BPP;
	}
	else/*if((cmd==PER2_SETDEPTH24_TRUE)||(cmd==PER2_SETDEPTH24_DIRECT)) */
	{ reg = FB_WAT_PIXFORM_24BPP;
	}
	if(cmd == PER2_SETDEPTH8_INDEX)
	{ reg |= FB_WAT_MODE_INDEX;
	}
	else if(cmd == PER2_SETDEPTH8_GREY)
	{ reg |= FB_WAT_MODE_GREYSCALE;
	}
	else if((cmd == PER2_SETDEPTH8_TRUE)||(cmd==PER2_SETDEPTH12_TRUE)||
		(cmd == PER2_SETDEPTH16_TRUE)||(cmd==PER2_SETDEPTH24_TRUE))
	{ reg |= FB_WAT_MODE_TRUE;
	}
	else /*if((cmd==PER2_SETDEPTH8_DIRECT)||(cmd==PER2_SETDEPTH12_DIRECT)||
		(cmd == PER2_SETDEPTH16_DIRECT)||(cmd==PER2_SETDEPTH24_DIRECT)) */
	{ reg |= FB_WAT_MODE_DIRECTRGB;
	}

	reg |= (start_addr<<FB_WAT_START_ADDR_SHIFT) |
					FB_WAT_BS_FBA | FB_WAT_TR_OPAQUE;

	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
	for(i=0; i<256; i++)
	{
		if(id == (i & ~mask))
		{
			/* set internal address */
			*p2->HwDeviceExtension->RGB561_Addr_L = (FRAME_BUF_WAT + i);
			*p2->HwDeviceExtension->RGB561_Addr_H = (FRAME_BUF_WAT + i)>>8;
				/*bits 2-9 into bits 0-7*/
			*p2->HwDeviceExtension->RGB561_Tables = reg>>2;
				/*bits 0-1 into bits 6-7*/
			*p2->HwDeviceExtension->RGB561_Tables = reg<<6;
		}
	}
	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);
}
#endif /* GET_TO_THIS_EVENTUALLY */


#ifdef USE_PER2_DMA
static void rect_patch_copy(per2_info *p2, rad4_rect_copy *rw)
{
	int *registers = p2->registers;
	int render_cmd = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEX | RENDER_INCREASEY;
	
	if (((rw->startx + rw->sizex) > p2->v_info.x_size) ||
		((rw->starty + rw->sizey) > p2->v_info.y_size) ||
		(rw->sizex <= 0) || (rw->sizey <= 0) ||
		(rw->startx < 0) || (rw->starty < 0))
		return;
	if ((rw->offsety == 0) && (rw->offsetx == 0))
		return;

	p2->time_happened = 0;
	WAIT_IN_FIFO(10);	/* wait for room in register FIFO */
	SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
	INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock (disables interrupts) */
	p2->sync_to_cookie = itimeout(rad1_to_func, p2->sync_wait_on_it,
		2 + drv_usectohz(DMA_TIMEOUT_US), pltimeout, p2);

	/* set output filter for sync data only */
	p2->p2_filtermode.all = 0;
	p2->p2_filtermode.bits.synchronizationData = RAD1_ENABLE;
	*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;

	p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
	p2->p2_logicalopmode.bits.logicOp = LOGICOP_COPY;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
#ifdef USING_HARDWARE_MASK
	*(registers + FBHARDWAREWRITEMASK_REG/4) = rw->mask;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
#else /* USING_HARDWARE_MASK */
	*(registers + FBSOFTWAREWRITEMASK_REG/4) = rw->mask;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_ENABLE;
#endif /* USING_HARDWARE_MASK */
	p2->p2_fbreadmode.bits.readSourceEnable = RAD1_ENABLE;
	p2->p2_fbreadmode.bits.packedData = RAD1_DISABLE;
	*(registers + FBSOURCEDELTA_REG/4) =
		SOURCEDELTA_YX(-(rw->offsety), -(rw->offsetx));

	if (rw->offsetx & 1)
	{
		if (rw->offsetx > 0)
			p2->p2_fbreadmode.bits.relativeOffset = 1;
		else
			p2->p2_fbreadmode.bits.relativeOffset = -1;
	}
	*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
	
	if ((rw->offsety > 0) && (rw->offsetx < rw->sizex) && (rw->offsety < rw->sizey))
	{
		render_cmd = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEX;
	}
	else if ((rw->offsety == 0) && (rw->offsetx > 0) && (rw->offsetx < rw->sizex))
	{
		render_cmd = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEY;
	}
	
	/* tell GC to copy rectangle */
	*(registers + RECTANGLEORIGIN_REG/4) =
		RECTORIGIN_YX(rw->starty + rw->offsety, rw->startx + rw->offsetx);
	*(registers + RECTANGLESIZE_REG/4) = RECTSIZE_HW(rw->sizey, rw->sizex);
	*(registers + RENDER_REG/4) = render_cmd;

	*(registers + WAITFORCOMPLETION_REG/4) = 0;
	*(registers + SYNC_REG/4) = SYNC_INT_EN | 0x0DDB0B;
	/* wait for render to complete */
	INT_LOCK_SV_WAIT(p2->sync_wait_on_it, &p2->per2_global_lock);
		/* releases per2_global_lock and enables interrupts */
	untimeout(p2->sync_to_cookie);
	if (p2->time_happened)
	{
		cmn_err(CE_DEBUG,"rect_patch_copy timed out\n");
	}
			
#ifdef USING_HARDWARE_MASK
	*(registers + FBHARDWAREWRITEMASK_REG/4) = p2->hard_mask;
#else /* USING_HARDWARE_MASK */
	*(registers + FBSOFTWAREWRITEMASK_REG/4) = p2->hard_mask;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
#endif /* USING_HARDWARE_MASK */
	p2->p2_fbreadmode.bits.relativeOffset = 0;
	p2->p2_fbreadmode.bits.readSourceEnable = RAD1_DISABLE;
	*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
	p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;

	p2->p2_filtermode.all = 0;
	*(p2->registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;

	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
	SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
}


static int rad1_dma_patch_move(per2_info *p2, int dma_ptr, int startx, int starty,
	int sizex, int sizey, int direction, int mask, RadColorMode mode,int stride)
{
	int *registers = p2->registers;
	int i;
	int cfgstatus;
	int new_sizex = sizex;
	int words_to_xfr;
	int words_left;
	int dest_ptr = dma_ptr;
	int dma_mem_addr;
	p2_texturereadmode_reg texturereadmode;
	p2_texturecolormode_reg texturecolormode;
	p2_texturedataformat_reg texturedataformat;

/*#define PRINT_TIMEOUT_INFO	* */
#ifdef PRINT_TIMEOUT_INFO
	if(p2->time_happened >0)
		cmn_err(CE_DEBUG,"dma timed out rad1_dma_patch_move entered\n");
#endif /* PRINT_TIMEOUT_INFO */
	p2->time_happened = 0;

	WAIT_IN_FIFO(20);	/* wait for room in register FIFO */
	SLP_LOCK_LOCK(&p2->mask_mutex_lock);	/* take the mask lock */
	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */

	if (direction == UIO_WRITE)
	{
/* 		cmn_err(CE_DEBUG,"rad1_dma_patch_move write mode = %d sx %d sy = d\n",
				mode,sizex,sizey); * */
/*		cmn_err(CE_DEBUG,"x = 0x%x y = 0x%x \n",startx,starty); * */
		INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock (disables interrupts) */

		/* set up to wait on the interrupt when dma done */
		p2->dma_to_cookie = itimeout(rad1_to_func, p2->dma_wr_wait_on_it,
			2 + drv_usectohz(DMA_TIMEOUT_US), pltimeout, p2);

		p2->p2_bydmacontrol.bits.mode = BYDMA_MODE_IMPLICIT;
		p2->p2_bydmacontrol.bits.restartGP = RAD1_ENABLE;
		p2->p2_bydmacontrol.bits.patchType = BYDMA_PATCH_DIS;
		p2->p2_bydmacontrol.bits.offsetX = 0;
		p2->p2_bydmacontrol.bits.offsetY = 0;
		p2->p2_bydmacontrol.bits.byteSwapControl = BYDMA_BYTE_SWAP_STD;
		switch (mode)
		{
			case RGB32:
			case RGB24:
				p2->p2_bydmacontrol.bits.dataFormat = BYDMA_FMT_32BITS;
				*(registers + BYDMASTRIDE_REG/4) = stride?stride:(sizex * 4);
				/* transfer 32 bit images directly to screen */
				dma_mem_addr = (p2->screen_base * 2) +
					(starty * p2->v_info.x_size + startx);
				*(registers + BYPASSWRITEMASK_REG/4) = mask; 
				*(registers + BYDMASIZE_REG/4) = 
						BYDMASIZE_HW(sizey,sizex);
				break;
			case RGB15:
				p2->p2_bydmacontrol.bits.dataFormat = BYDMA_FMT_16BITS;	/* */
				*(registers + BYDMASTRIDE_REG/4) = stride?stride:((sizex * 2 + 3)& ~3);
				/* transfer 15 bit images to offscreen memory just past display */
				dma_mem_addr = 2 * ((p2->screen_base * 2) +
					(p2->v_info.y_size * p2->v_info.x_size));	/* */
				*(registers + BYPASSWRITEMASK_REG/4) = RAD1_WRITEMASK_ALL;
				*(registers + BYDMASIZE_REG/4) = BYDMASIZE_HW(sizey, (sizex + 1) & ~1);
				break;
			case CI8:
				p2->p2_bydmacontrol.bits.dataFormat = BYDMA_FMT_8BITS;	/* */
				*(registers + BYDMASTRIDE_REG/4) = stride?stride:((sizex + 3) & ~3);
				/* transfer 8 bit images to offscreen memory just past display */
				dma_mem_addr = 4 * ((p2->screen_base * 2) +
					(p2->v_info.y_size * p2->v_info.x_size));	/* */
				*(registers + BYPASSWRITEMASK_REG/4) = RAD1_WRITEMASK_ALL;
				*(registers + BYDMASIZE_REG/4) = BYDMASIZE_HW(sizey, (sizex + 3) & ~3);
				break;
		}
		*(registers + BYDMAMEMADDR_REG/4) = dma_mem_addr;
		*(registers + BYDMAADDRESS_REG/4) = dma_ptr;
		*(registers + BYDMABYTEMASK_REG/4) = 0xf000f;
		*(registers + BYDMACONTROL_REG/4) = p2->p2_bydmacontrol.all; /* start dma */

		INT_LOCK_SV_WAIT(p2->dma_wr_wait_on_it, &p2->per2_global_lock);
			/* releases per2_global_lock and enables interrupts */
		untimeout(p2->dma_to_cookie);
#ifdef CAN_ABORT_DMA
		INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock */

		if (p2->time_happened > 0)	/* DMA timed out */
		{
			delay(drv_usectohz(200)); /* wait for done */
			/* abort the dma??? */
#ifdef PRINT_TIMEOUT_INFO
			cmn_err(CE_DEBUG,"ftr dma abort\n");
#endif /* PRINT_TIMEOUT_INFO */
		}
		INT_LOCK_UNLOCK(&p2->per2_global_lock);	/* release the global lock */
#endif /* CAN_ABORT_DMA */

		*(registers + BYPASSWRITEMASK_REG/4) = p2->mask; /* restore the mask */
		
		if (((mode == CI8) || (mode == RGB15)) && (p2->time_happened == 0))
		{
			/* non 32 bit images are offscreen at this point */
			/* they must now be copied to the screen with color mode translation */
			WAIT_IN_FIFO(20);	/* wait for room in register FIFO */
				
			INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock (disables interrupts) */
			p2->sync_to_cookie = itimeout(rad1_to_func, p2->sync_wait_on_it,
				2 + drv_usectohz(DMA_TIMEOUT_US), pltimeout, p2);

			*(registers + WAITFORCOMPLETION_REG/4) = 0;	/* make sure writes have completed */
			/* set output filter for sync data only */
			p2->p2_filtermode.all = 0;
			p2->p2_filtermode.bits.synchronizationData = RAD1_ENABLE;
			*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;

#ifdef USING_HARDWARE_MASK
			*(registers + FBHARDWAREWRITEMASK_REG/4) = RAD1_WRITEMASK_RGB;
			p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
#else /* USING_HARDWARE_MASK */
			*(registers + FBSOFTWAREWRITEMASK_REG/4) = RAD1_WRITEMASK_RGB;
			p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_ENABLE;
#endif /* USING_HARDWARE_MASK */
			p2->p2_fbreadmode.bits.readSourceEnable = RAD1_DISABLE;
			*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
			p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
			*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;

			/* set color translation format */
			*(registers + TEXTUREADDRESSMODE_REG/4) = 
				TEXTUREADDRESSMODE_TEXTURE_ADDRESS_ENABLE;
			switch (mode)
			{
				case RGB15:
					i = COLORFORMAT_565FRONT_2V;
					p2->p2_texturemapformat.bits.texelSize = TEXELSIZE_16BITS;
					break;
				case CI8:
					i = COLORFORMAT_CI8_2V;
					*(registers + TEXELLUTMODE_REG/4) = RAD1_ENABLE;
					p2->p2_texturemapformat.bits.texelSize = TEXELSIZE_8BITS;
					break;
			}
			*(registers + TEXTUREBASEADDRESS_REG/4) = dma_mem_addr;
			texturecolormode.all = 0;
			texturecolormode.bits.textureEnable = RAD1_ENABLE;
			texturecolormode.bits.applicationMode = APPLICATIONMODE_RGB_COPY;
			*(registers + TEXTURECOLORMODE_REG/4) = texturecolormode.all;
			texturedataformat.all = 0;
			texturedataformat.bits.textureFormat = i;
			texturedataformat.bits.textureFormatExtension = (i >> 4) & 1;
			texturedataformat.bits.noAlphaBuffer = RAD1_TRUE;
			*(registers + TEXTUREDATAFORMAT_REG/4) = texturedataformat.all;
			p2->p2_texturemapformat.bits.windowOrigin = WINDOWORIGIN_TOPLEFT;
			p2->p2_texturemapformat.bits.subpatchMode = RAD1_DISABLE;
			*(registers + TEXTUREMAPFORMAT_REG/4) = p2->p2_texturemapformat.all;
			texturereadmode.all = 0;
			texturereadmode.bits.enable = RAD1_ENABLE;
			texturereadmode.bits.height = 11;
			texturereadmode.bits.width = 11;
			texturereadmode.bits.packedData = RAD1_DISABLE;
			*(registers + TEXTUREREADMODE_REG/4) = texturereadmode.all;
			*(registers + SSTART_REG/4) = 0;
			*(registers + DSDX_REG/4) = INTtoFIXED12_18(1);
			*(registers + DSDYDOM_REG/4) = 0;
			*(registers + TSTART_REG/4) = 0;
			*(registers + DTDX_REG/4) = 0;
			*(registers + DTDYDOM_REG/4) = INTtoFIXED12_18(1);

			/* tell GC to copy rectangle */
			*(registers + RECTANGLEORIGIN_REG/4) = RECTORIGIN_YX(starty , startx);	/* */
			*(registers + RECTANGLESIZE_REG/4) = RECTSIZE_HW(sizey, sizex);
			*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE | 
				RENDER_TEXTUREENABLE | RENDER_INCREASEX | RENDER_INCREASEY;

			*(registers + WAITFORCOMPLETION_REG/4) = 0;
			*(registers + SYNC_REG/4) = SYNC_INT_EN | 0xB00B00;
			/* wait for render to complete */
			INT_LOCK_SV_WAIT(p2->sync_wait_on_it, &p2->per2_global_lock);
				/* releases per2_global_lock and enables interrupts */
			untimeout(p2->sync_to_cookie);
			if (p2->time_happened)
			{
				cmn_err(CE_DEBUG,"rad1_dma_patch_move write timed out, mode %s\n",
					(mode == RGB15) ? "RGB15" : "CI8");
			}
			
			p2->p2_filtermode.all = 0;
			*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;
			*(registers + FBPIXELOFFSET_REG/4) = 0;
			*(registers + TEXTURECOLORMODE_REG/4) = 0;
			*(registers + TEXTUREDATAFORMAT_REG/4) = 0;
			*(registers + TEXTUREREADMODE_REG/4) = 0;
			*(registers + TEXTUREADDRESSMODE_REG/4) = 0;
			*(registers + TEXELLUTMODE_REG/4) = 0;
#ifdef USING_HARDWARE_MASK
			*(registers + FBHARDWAREWRITEMASK_REG/4) = p2->hard_mask;
#else /* USING_HARDWARE_MASK */
			*(registers + FBSOFTWAREWRITEMASK_REG/4) = p2->hard_mask;
			p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
			*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
#endif /* USING_HARDWARE_MASK */
		}

#ifdef PRINT_TIMEOUT_INFO
		if (p2->time_happened >0)
			cmn_err(CE_DEBUG,"dma timed out b4 return\n");
#endif /* PRINT_TIMEOUT_INFO */
	}
	else	/* (direction == UIO_READ) */
	{		
/* 		cmn_err(CE_DEBUG,"rad1_dma_patch_move read %d:%d\n",sizex,sizey); * */
		/* initialize appropriate units */
		p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_ENABLE;
		p2->p2_logicalopmode.bits.logicOp = LOGICOP_COPY;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
		p2->p2_fbwritemode.bits.writeEnable = RAD1_DISABLE;
		p2->p2_fbwritemode.bits.upLoadData = UPLOADCOLORTOHOST;
		*(registers + FBWRITEMODE_REG/4) = p2->p2_fbwritemode.all;
		p2->p2_fbreadmode.bits.readSourceEnable = RAD1_DISABLE;
		p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_ENABLE;
		p2->p2_fbreadmode.bits.dataType = DATATYPE_FBDEFAULT;
/*		p2->p2_fbreadmode.bits.dataType = DATATYPE_FBCOLOR;	* */
		p2->p2_fbreadmode.bits.packedData = RAD1_ENABLE;	/* */
/*		p2->p2_fbreadmode.bits.packedData = RAD1_DISABLE;	* */
		*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
		p2->p2_alphablendmode.bits.operation = OPERATION_FORMAT;
		p2->p2_alphablendmode.bits.alphaBlendEnable = RAD1_ENABLE;
#ifdef TRANSLATE_ON_READBACK_INCOMPLETE
		switch (mode)
		{
			case RGB15:
				alphablendmode.bits.operation = OPERATION_FORMAT;
				alphablendmode.bits.alphaBlendEnable = RAD1_ENABLE;
				dithermode.bits.colorFormatExtension =
					(COLORFORMAT_565FRONT_2V >> 4) & 1;
				dithermode.bits.colorFormat = COLORFORMAT_565FRONT_2V;
				dithermode.bits.unitEnable = RAD1_ENABLE;
				new_sizex = (sizex + 1) / 2;	/* number of words per line */
				break;
			case CI8:
				alphablendmode.bits.operation = OPERATION_FORMAT;
				alphablendmode.bits.alphaBlendEnable = RAD1_ENABLE;
				dithermode.bits.colorFormatExtension = 
					(COLORFORMAT_CI8_2V >> 4) & 1;
				dithermode.bits.colorFormat = COLORFORMAT_CI8_2V;
				dithermode.bits.unitEnable = RAD1_ENABLE;
				new_sizex = (sizex + 3) / 4;	/* number of words per line */
				break;
		}
#endif /* TRANSLATE_ON_READBACK_INCOMPLETE */
		*(registers + ALPHABLENDMODE_REG/4) = p2->p2_alphablendmode.all;	/* */
		words_left = sizey * new_sizex;
		
		/* set output filter for color data only */
		p2->p2_filtermode.all = 0;
		p2->p2_filtermode.bits.colorData = RAD1_ENABLE;
		*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;

		/* tell GC to start reading out rectangle */
		*(registers + RECTANGLEORIGIN_REG/4) = RECTORIGIN_YX(starty, startx);
		*(registers + RECTANGLESIZE_REG/4) = RECTSIZE_HW(sizey, sizex);
delay(1);
		*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE |
			RENDER_INCREASEX | RENDER_INCREASEY;

		while (words_left > 0)
		{
			/* figure how much we can transfer in one operation */
			words_to_xfr = (words_left > MAX_OUTDMA_SIZE) ?
				MAX_OUTDMA_SIZE : words_left;
			
			/* set up to wait on dma done */
			INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock (disables interrupts) */

			/* start DMA to buffer */
			*(registers + OUTDMAADDRESS_REG/4) = dest_ptr;

			p2->dma_to_cookie = itimeout(rad1_to_func, p2->dma_rd_wait_on_it,
				2 + drv_usectohz(DMA_TIMEOUT_US), pltimeout, p2);

			cfgstatus = *(registers + ERRORFLAGS_REG/4);
			*(registers + ERRORFLAGS_REG/4) = cfgstatus;

			*(registers + OUTDMACOUNT_REG/4) = words_to_xfr; /* start the DMA */

			/* wait for DMA to complete */
			INT_LOCK_SV_WAIT(p2->dma_rd_wait_on_it, &p2->per2_global_lock);
				/* releases per2_global_lock and enables interrupts */
				
			untimeout(p2->dma_to_cookie);

			if (p2->time_happened)	/* */
			{
				cmn_err(CE_DEBUG,"rad1_dma_patch_move read timed out %d:%d %d:%d\n",
					startx, starty, sizex, sizey);
				cmn_err(CE_DEBUG,"OUTDMACOUNT_REG = %d, OUTFIFOWORDS_REG = %d\n",
					*(registers + OUTDMACOUNT_REG/4), *(registers + OUTFIFOWORDS_REG/4));
				cfgstatus = *(registers + ERRORFLAGS_REG/4);
				cmn_err(CE_DEBUG,"ERRORFLAGS_REG = 0x%x\n", cfgstatus);
				*(registers + ERRORFLAGS_REG/4) = cfgstatus;
				cmn_err(CE_DEBUG,"words_to_xfr = %d\n", words_to_xfr);
				if (*(registers + OUTFIFOWORDS_REG/4))
				{
					i = 0;
					while (*(registers + OUTFIFOWORDS_REG/4))
					{
						i++;
						cfgstatus = *(p2->registers + OUT_FIFO/4);
					}
					cmn_err(CE_DEBUG,"flushed %d words, last was 0x%x\n", i, cfgstatus);
				}
				break;	/* */
			}

			words_left -= words_to_xfr;
			dest_ptr += words_to_xfr * 4;
		}

		p2->p2_alphablendmode.bits.alphaBlendEnable = RAD1_DISABLE;
		*(registers + ALPHABLENDMODE_REG/4) = p2->p2_alphablendmode.all;
		p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
		p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
		p2->p2_fbreadmode.bits.dataType = DATATYPE_FBDEFAULT;
		*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
		p2->p2_filtermode.all = 0;
		*(p2->registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;
		p2->p2_fbwritemode.bits.upLoadData = NOUPLOAD;
		p2->p2_fbwritemode.bits.writeEnable = RAD1_ENABLE;
		*(registers + FBWRITEMODE_REG/4) = p2->p2_fbwritemode.all;

		if ( 0 != (cfgstatus = *(registers + ERRORFLAGS_REG/4)))
		{
			cmn_err(CE_DEBUG,"ERRORFLAGS_REG = 0x%x\n", cfgstatus);
			*(registers + ERRORFLAGS_REG/4) = cfgstatus;
		}
		if (*(registers + OUTFIFOWORDS_REG/4))
		{ i = 0;
			while (*(registers + OUTFIFOWORDS_REG/4))
			{
				i++;
				cfgstatus = *(p2->registers + OUT_FIFO/4);
			}
		if(sizex > 1)
			cmn_err(CE_DEBUG,"\nflushed %d words, last was 0x%x\n", i, cfgstatus);
		}
	}
	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
	SLP_LOCK_UNLOCK(&p2->mask_mutex_lock);	/* release the mask lock */
	return 0;
}
#endif /*  USE_PER2_DMA */

static void daemon(per2_info *p2, int arg)
{
	switch(arg)
	{
		case RAD4_D_OFF:
			break;
		case RAD4_D_ON:
			break;
		case RAD4_D_FORCE:
			SLP_LOCK_LOCK(&p2->bigx_mutex_lock);  	/* take lock */
			if (p2->bigx_daemon_stall)
			{
				SLP_LOCK_UNLOCK(&p2->bigx_mutex_lock); 	/* release lock */
				while (p2->bigx_daemon_stall)
					delay(drv_usectohz(DAEMON_STALL_USEC));
				SLP_LOCK_LOCK(&p2->bigx_mutex_lock);  	/* retake lock */
			}
			bigx_update(p2, 0, 0, bigx_x, bigx_y, UIO_WRITE);	/* do the update */
			SLP_LOCK_UNLOCK(&p2->bigx_mutex_lock); 	/* release lock */
			break;
		default:
			break;
	}
}

static void bigx_update(per2_info *p2, int x, int y, int w, int h, int direction)
{
	int i, j;
	paddr_t phys_ptr, dma_ptr;
	int dma_ptr_so;
	int l_sizey, t_sizey, m_sizey;
	int map_used = FALSE;
	int board;
	int lx, lw;
	int ltw, tw;
	int bigx_step;
	per2_info *lp2;
	int mapii = 0;
	int *source, *dest, *fb, *base;

	phys_ptr = kvtophys(p2->tbuf);	/* */
	if (phys_ptr == NULL)
	{
		cmn_err(CE_DEBUG,"bigx_update: kvtophys returned NULL for kv 0x%x \n",
			p2->tbuf);
/* do pio move? */
		return;
	}
	dki_dcache_wb(p2->tbuf, bigx_x * bigx_y * 4);
#ifdef FORCE_NO_DMATRANS
	dma_ptr = NULL;
#else /* FORCE_NO_DMATRANS */
	dma_ptr = pciio_dmatrans_addr(p2->dev, &p2->dev_desc,
		phys_ptr, bigx_x * bigx_y * 4, pciio_dma_flags);
#endif /* FORCE_NO_DMATRANS */
	if (dma_ptr == NULL)
	{
		cmn_err(CE_DEBUG," bigx_update: pciio_dmatrans_addr failed\n"); /* */
		if (p2->dma_map != NULL)
		{
			if ((dma_ptr = pciio_dmamap_addr(p2->dma_map, phys_ptr, bigx_x * bigx_y * 4))
				!= NULL)
			{
				map_used = TRUE;
/*				cmn_err(CE_DEBUG,"bigx_update: dma_ptr = 0x%lx\n", dma_ptr); * */
			}
		}
	}
	if(dma_ptr == NULL)
	{
		cmn_err(CE_DEBUG,"bigx_update: trans and map failed\n"); /* */
		return;
	}

	bigx_step = bigx_x / number_of_per2s;
	for(board = 0; board < number_of_per2s; board++)
	{
/*		cmn_err(CE_DEBUG,"in update b %d %d %d %d %d\n",board,x,y,w,h); * */
		lp2 = bigx_p2s[board];
		if ((x + w) < board*bigx_step)
			break; /* no more to do */
		if (x >= (board+1)*bigx_step) 
			continue; /* region is past this board */
		if (x < board*bigx_step)
		{
			/* region overlaps left edge of this board */
			lx = 0;
			lw = w - (board*bigx_step - x);
		}
		else
		{
			lx = x - board*bigx_step;
			lw = w;
		}
		if (x + w > (board+1)*bigx_step)
			lw -= (x + w) - (board+1)*bigx_step;

		if (lw == 0 || h == 0)
			continue;
#ifdef BIGX_WITH_PIO
		if ((lw*h) <= PER2_MIN_DMA_TOTAL)
		{	
 			base = p2->tbuf + board*bigx_step + lx + y * bigx_x;	/* ??? */
/* 			base = lp2->tbuf + lx + y * bigx_x;	* */
			fb = lp2->ram1 + lx + y * lp2->v_info.x_size;
			SLP_LOCK_LOCK(&lp2->hardware_mutex_lock);	/* lock the hardware */
			if(direction == UIO_WRITE)
			{
				/* do pio copy to VRAM, using mask already set */
				for(i = 0; i < h; i++)
				{
					source = base;
					dest = fb;
					for(j = 0; j < w; j++)
					{
						*dest = *source;
						dest++;
						source++;
					}
					base += bigx_x;
					fb += lp2->v_info.x_size;
				}
			}
			else
			{
				/* do pio copy to buffer */
				for(i = 0; i < h; i++)
				{
					dest = base;
					source = fb;
					for(j = 0; j < w; j++)
					{
						*dest = *source;
						dest++;
						source++;
					}
					base += bigx_x;
					fb += lp2->v_info.x_size;
				}
			}
			SLP_LOCK_UNLOCK(&lp2->hardware_mutex_lock);	/* release the hardware */
			continue;
		}
#endif /* BIGX_WITH_PIO */

		t_sizey = 0;
		m_sizey = (rad1pci_max_dma_area)/(4*lw);	/* how many lines fit in a dma map */
		if (m_sizey == 0)
			m_sizey = 1; /* there is always room for one */
		dma_ptr_so = (bigx_x * y + (lx + (board * bigx_step))) * 4;
		while(t_sizey < h)
		{
			l_sizey = (m_sizey > (h-t_sizey)) ? (h-t_sizey) : m_sizey;	/* min */

			rad1_dma_patch_move(lp2,
				(int)dma_ptr + dma_ptr_so + (t_sizey * lp2->v_info.x_size * 4),
				lx, y + t_sizey, lw, l_sizey, direction, lp2->mask, RGB32,0);

			t_sizey += l_sizey;
		} /* while(t_sizey < h) */

	} /* for(board = 0; board < number_of_per2s; board++) */

	if (map_used)
	{	/*	pciio_dmamap_drain(p2->dma_map); * */
		pciio_dmamap_done(p2->dma_map);
	}
/*	cmn_err(CE_DEBUG,"leaving bigx_update %d %d %d %d\n",lx,y,lw,h); * */
}


#define NO_PSITECH_LOGO /* no psitech logo or name */
#ifndef NO_PSITECH_LOGO
#include "psilogo.h"
#include "psitech.h"
#endif /*  NO_PSITECH_LOGO */

static void
fill_bits(int *registers, char *bits, int *pixels, int color,
		int x_st, int y_st,	/* where to start in the pixels */
		int x_sz, int y_sz,	/* size of the bit map */
		int s_width)		/* width of the pixmap */
{
	int src_ix, src_iy, dst_ix, dst_iy;
	int b_mask;

	WAIT_IN_FIFO(1);	/* wait for room in register FIFO */
	*(registers + FBWRITEDATA_REG/4) = color;
	b_mask = 0x01;
	for (src_iy = 0, dst_iy = y_st; src_iy < y_sz; src_iy++, dst_iy++)
	{
		WAIT_IN_FIFO(1);	/* wait for room in register FIFO */
		*(registers + STARTY_REG/4) = INTtoFIXED16_16(dst_iy);
		for (src_ix = 0, dst_ix = x_st; src_ix < x_sz; src_ix++, dst_ix++)
		{
			if ((*bits & b_mask) != 0)
			{
				WAIT_IN_FIFO(2);	/* wait for room in register FIFO */
				*(registers + STARTXDOM_REG/4) = INTtoFIXED16_16(dst_ix);
				*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_POINT;
			}
			b_mask <<= 1;
			if (b_mask > 0x080)
			{
				b_mask = 0x01;
				bits++;
			}
		}
		if (b_mask != 0x01)
		{
			b_mask = 0x01;
			bits++;
		}
	}
}

static void
shade_bits(int *registers, char *bits, int *pixels, int color,
		int x_st, int y_st,	/* where to start in the pixels */
		int x_sz, int y_sz,	/* size of the bit map */
		int s_width)		/* width of the pixmap */
{	
	int src_ix, src_iy, dst_ix, dst_iy;
	int b_mask;
	int t_ps, t_pd;

#ifndef SHADE_BITS_PIO
	WAIT_IN_FIFO(1);	/* wait for room in register FIFO */
	*(registers + FBWRITEDATA_REG/4) = color;
#endif /* SHADE_BITS_PIO */
	b_mask = 0x01;
	for(src_iy = 0, dst_iy = y_st; src_iy < y_sz; src_iy++, dst_iy++)
	{
#ifndef SHADE_BITS_PIO
		WAIT_IN_FIFO(1);	/* wait for room in register FIFO */
		*(registers + STARTY_REG/4) = INTtoFIXED16_16(dst_iy);
#endif /* SHADE_BITS_PIO */
		for(src_ix = 0, dst_ix = x_st; src_ix < x_sz; src_ix++, dst_ix++)
		{
			if((*bits & b_mask) != 0)
			{
#ifdef SHADE_BITS_PIO
				t_pd = 0;
				t_ps = pixels[dst_iy * s_width + dst_ix];
				t_pd |= (t_ps + (color & 0x0ff0000)) & 0x0ff0000;
				t_pd |= (t_ps + (color & 0x0ff00)) & 0x0ff00;
				t_pd |= (t_ps + (color & 0x0ff)) & 0x0ff;
				pixels[dst_iy * s_width + dst_ix] = t_pd;
#else /* SHADE_BITS_PIO */
				WAIT_IN_FIFO(2);	/* wait for room in register FIFO */
				*(registers + STARTXDOM_REG/4) = INTtoFIXED16_16(dst_ix);
				*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_POINT;
#endif /* SHADE_BITS_PIO */
			}
			b_mask <<= 1;
			if(b_mask >0x080)
			{
				b_mask = 0x01;
				bits++;
			}
		}
		if(b_mask != 0x01)
		{
			b_mask = 0x01;
			bits++;
		}
	}
}

static void
fill_fb(per2_info *p2)
{
	int xi, yi;
	int *pixels = p2->ram1;
	int *registers = p2->registers;
	int sx = p2->v_info.x_size;
	int sy = p2->v_info.y_size;

	WAIT_IN_FIFO(40);	/* wait for room in register FIFO */
		
	/* clear all bits */
	*(registers + FBHARDWAREWRITEMASK_REG/4) = RAD1_WRITEMASK_ALL;
	p2->p2_logicalopmode.all = 0;
	p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
	p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_ENABLE;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
	*(registers + FBWRITEDATA_REG/4) = 0;
	*(registers + FBBLOCKCOLOR_REG/4) = 0;
	*(registers + RECTANGLEORIGIN_REG/4) = RECTORIGIN_YX(0, 0);
	*(registers + RECTANGLESIZE_REG/4) = RECTSIZE_HW(sy, sx);
	*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEX |
		RENDER_INCREASEY | RENDER_FASTFILLENABLE;	/* */
	*(registers + FBHARDWAREWRITEMASK_REG/4) = p2->hard_mask;
	p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_DISABLE;

	/* fill screen with shaded color */
	p2->p2_filtermode.all = 0;
	p2->p2_filtermode.bits.synchronizationData = RAD1_ENABLE;
	*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;

	p2->p2_colorddamode.bits.unitEnable = RAD1_ENABLE;
	p2->p2_colorddamode.bits.shadingMode = SHADINGMODE_GOURAUD;
	*(registers + COLORDDAMODE_REG/4) = p2->p2_colorddamode.all;

	p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_ENABLE;
	p2->p2_logicalopmode.bits.logicOp = LOGICOP_COPY;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;

#ifdef FILL_FB_GOURAUD_RGB
	*(registers + RSTART_REG/4) = INTtoFIXED9_11(255);
	*(registers + DRDX_REG/4) = INTtoFIXED9_11(-256)/sx;
	*(registers + DRDYDOM_REG/4) = INTtoFIXED9_11(-256)/sy;
	*(registers + GSTART_REG/4) = 0;
	*(registers + DGDX_REG/4) = INTtoFIXED9_11(256)/sx;
	*(registers + DGDYDOM_REG/4) = 0;
	*(registers + BSTART_REG/4) = INTtoFIXED9_11(127);
	*(registers + DBDX_REG/4) = 0;
	*(registers + DBDYDOM_REG/4) = INTtoFIXED9_11(256)/sy;
#else /* FILL_FB_GOURAUD_RGB */
	*(registers + RSTART_REG/4) = INTtoFIXED9_11(255);
	*(registers + DRDX_REG/4) = 0;
	*(registers + DRDYDOM_REG/4) = INTtoFIXED9_11(-256)/sy;
	*(registers + GSTART_REG/4) = 0;
	*(registers + DGDX_REG/4) = INTtoFIXED9_11(256)/sx;
	*(registers + DGDYDOM_REG/4) = 0;
	*(registers + BSTART_REG/4) = 0;
	*(registers + DBDX_REG/4) = 0;
	*(registers + DBDYDOM_REG/4) = 0;
#endif /* FILL_FB_GOURAUD_RGB */
	*(registers + ASTART_REG/4) = 0;

	*(registers + STARTXDOM_REG/4) = 0;
	*(registers + DXDOM_REG/4) = 0;
	*(registers + STARTXSUB_REG/4) = INTtoFIXED16_16(sx);
	*(registers + DXSUB_REG/4) = 0;
	*(registers + STARTY_REG/4) = 0;
	*(registers + DY_REG/4) = INTtoFIXED16_16(1);
	*(registers + GP_COUNT_REG/4) = sy;
	*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_TRAPEZOID;

	*(registers + WAITFORCOMPLETION_REG/4) = 0;
	*(registers + SYNC_REG/4) = 0xBAABAA;
		/* wait for render to complete */
	while(!(*(registers + OUTFIFOWORDS_REG/4)))
		delay(0);
	xi = *(p2->registers + OUT_FIFO/4);

	p2->p2_filtermode.all = 0;
	*(registers + FILTERMODE_REG/4) = p2->p2_filtermode.all;

	p2->p2_colorddamode.bits.unitEnable = RAD1_DISABLE;
	*(registers + COLORDDAMODE_REG/4) = p2->p2_colorddamode.all;
#ifndef NO_PSITECH_LOGO

#ifdef SHADE_BITS_OK
	p2->p2_fbreadmode.bits.readSourceEnable = RAD1_DISABLE;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_ENABLE;
	p2->p2_fbreadmode.bits.dataType = DATATYPE_FBDEFAULT;
	p2->p2_fbreadmode.bits.packedData = RAD1_DISABLE;
	*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
	p2->p2_logicalopmode.bits.logicOp = LOGICOP_XOR;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;

	shade_bits(registers, psilogo_bits, pixels, 0x0000d0,
			45, sy - 95, /* where to start in the pixels */
			psilogo_width, psilogo_height, /* size of the bit map */
			sx);
	shade_bits(registers, psitech_bits, pixels, 0x0000d0,
			125, sy - 95, /* where to start in the pixels */
			psitech_width, psitech_height, /* size of the bit map */
			sx);
#endif /* SHADE_BITS_OK */

	p2->p2_fbreadmode.bits.readSourceEnable = RAD1_DISABLE;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
	*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
	p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
	p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_ENABLE;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
#ifndef SHADE_BITS_OK
	fill_bits(registers, psilogo_bits, pixels, 0,
			45, sy - 95, /* where to start in the pixels */
			psilogo_width, psilogo_height, /* size of the bit map */
			sx);
	fill_bits(registers, psitech_bits, pixels, 0,
			125, sy - 95, /* where to start in the pixels */
			psitech_width, psitech_height, /* size of the bit map */
			sx);

#endif /* SHADE_BITS_OK */
	fill_bits(registers, psilogo_bits, pixels, 0x404040,
			50, sy - 100, /* where to start in the pixels */
			psilogo_width, psilogo_height, /* size of the bit map */
			sx);
	fill_bits(registers, psitech_bits, pixels, 0x804040,
			130, sy - 100, /* where to start in the pixels */
			psitech_width, psitech_height, /* size of the bit map */
			sx);
#endif /* NO_PSITECH_LOGO */
	p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
	p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_DISABLE;
	*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
}


int program_eeprom_sector(per2_info *p2, unsigned int offset,
	unsigned char *buffer)
{
	int i;
	volatile unsigned char *eeprom_base = (unsigned char *)p2->ram2;

	if (offset & (RAD1_SECTOR_SIZE - 1) != 0)
	{
		cmn_err(CE_DEBUG,"Bad EEPROM offset %d\n", offset);
		return EINVAL;
	}
					
	SLP_LOCK_LOCK(&p2->hardware_mutex_lock);	/* lock the hardware */
	INT_LOCK_LOCK(&p2->per2_global_lock);	/* take the global lock */
	eeprom_base[0x5556] = 0xAA;
	eeprom_base[0x2AA9] = 0x55;
	eeprom_base[0x5556] = 0xA0;	/* unlock EEPROM for sector write */
	/* write out a sector's worth of data */
	for (i = 0; i < RAD1_SECTOR_SIZE; i++)
		eeprom_base[i + offset] = buffer[i ^ 3];	/* convert endianness */

	/* poll for completion */
	for (i = 0x80000; i > 0; i--)
	{
		if (eeprom_base[offset + RAD1_SECTOR_SIZE - 1] ==
			buffer[RAD1_SECTOR_SIZE - 4])	/* */
		break;
	}
	if (i == 0)
	{
		INT_LOCK_UNLOCK(&p2->per2_global_lock);	/* release the global lock */
		SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
		cmn_err(CE_DEBUG,"EEPROM program timed-out\n");
		return EFAULT;
	}

	/* verify EEPROM */
	for (i = 0; i < RAD1_SECTOR_SIZE; i++)
	{
		if (eeprom_base[i + offset] != buffer[i ^ 3])	/* */
		{
			INT_LOCK_UNLOCK(&p2->per2_global_lock);	/* release the global lock */
			SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
			cmn_err(CE_DEBUG,"EEPROM program failed\n");
			return EFAULT;
		}
	}

	INT_LOCK_UNLOCK(&p2->per2_global_lock);	/* release the global lock */
	SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);	/* release the hardware */
	return 0;
}


#define RD_INDEXED_REGISTER_2V(register_base, register) \
	(*((register_base) + RDINDEXLOW_2V_REG/4) = register, \
	*((register_base) + RDINDEXHIGH_2V_REG/4) = (register) >> 8, \
	delay(drv_usectohz(100)), \
	*((register_base) + RDINDEXEDDATA_2V_REG/4))

#define show_it(offset) \
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, offset);\
		cmn_err(CE_DEBUG, "(0x%x)  = 0x%x\n",offset,tmp);

static void dump_per2v(per2_info *p2)
{	int tmp;
/* set the position and mode */

		tmp = *(p2->registers + SCREENBASE_REG/4);
		cmn_err(CE_DEBUG,
			"SCREENBASE_REG (0x%x)  = 0x%x\n",SCREENBASE_REG,tmp);
		tmp = *(p2->registers + SCREENBASERIGHT_REG/4);
		cmn_err(CE_DEBUG,
			"SCREENBASERIGHT_REG (0x%x)  = 0x%x\n",SCREENBASERIGHT_REG,tmp);
		tmp = *(p2->registers + SCREENSTRIDE_REG/4);
		cmn_err(CE_DEBUG,
			"SCREENSTRIDE_REG (0x%x)  = 0x%x\n",SCREENSTRIDE_REG,tmp);
		tmp = *(p2->registers + CHIPCONFIG_REG/4);
		cmn_err(CE_DEBUG,
			"CHIPCONFIG_REG (0x%x)  = 0x%x\n",CHIPCONFIG_REG,tmp);

		*(p2->registers + RDINDEXCONTROL_2V_REG/4) =  0x01;
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCURSORXLOW_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCURSORXLOW_2V_REG (0x%x)  = 0x%x\n",RDCURSORXLOW_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCURSORXHIGH_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCURSORXHIGH_2V_REG (0x%x)  = 0x%x\n",RDCURSORXHIGH_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCURSORYLOW_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCURSORYLOW_2V_REG (0x%x)  = 0x%x\n",RDCURSORYLOW_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCURSORYHIGH_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCURSORYHIGH_2V_REG (0x%x)  = 0x%x\n",RDCURSORYHIGH_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCURSORMODE_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCURSORMODE_2V_REG (0x%x)  = 0x%x\n",RDCURSORMODE_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCURSORCONTROL_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCURSORCONTROL_2V_REG (0x%x)  = 0x%x\n",RDCURSORCONTROL_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCURSORHOTSPOTX_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCURSORHOTSPOTX_2V_REG (0x%x)  = 0x%x\n",RDCURSORHOTSPOTX_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCURSORHOTSPOTY_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCURSORHOTSPOTY_2V_REG (0x%x)  = 0x%x\n",RDCURSORHOTSPOTY_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDPAN_2V_REG);
		cmn_err(CE_DEBUG,
			"RDPAN_2V_REG (0x%x)  = 0x%x\n",RDPAN_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDMISCCONTROL_2V_REG);
		cmn_err(CE_DEBUG,
			"RDMISCCONTROL_2V_REG (0x%x)  = 0x%x\n",RDMISCCONTROL_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDSYNCCONTROL_2V_REG);
		cmn_err(CE_DEBUG,
			"RDSYNCCONTROL_2V_REG (0x%x)  = 0x%x\n",RDSYNCCONTROL_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDDACCONTROL_2V_REG);
		cmn_err(CE_DEBUG,
			"RDDACCONTROL_2V_REG (0x%x)  = 0x%x\n",RDDACCONTROL_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDDCLKCONTROL_2V_REG);
		cmn_err(CE_DEBUG,
			"RDDCLKCONTROL_2V_REG (0x%x)  = 0x%x\n",RDDCLKCONTROL_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDMCLKCONTROL_2V_REG);
		cmn_err(CE_DEBUG,
			"RDMCLKCONTROL_2V_REG (0x%x)  = 0x%x\n",RDMCLKCONTROL_2V_REG,tmp);
		tmp = RD_INDEXED_REGISTER_2V(p2->registers, RDCOLORFORMAT_2V_REG);
		cmn_err(CE_DEBUG,
			"RDCOLORFORMAT_2V_REG (0x%x)  = 0x%x\n",RDCOLORFORMAT_2V_REG,tmp);
/* set the cursor pallette */
  *(p2->registers + RDINDEXLOW_2V_REG/4) = RDCURSORPALETTE_2V_REG;
  *(p2->registers + RDINDEXHIGH_2V_REG/4) = RDCURSORPALETTE_2V_REG >> 8;
		cmn_err(CE_DEBUG,
			"RDCURSORPALETTE_2V_REG (0x%x)  =", RDCURSORPALETTE_2V_REG);
{ int i;
  for (i=0;i< 6;i++)
  { 
		cmn_err(CE_DEBUG," 0x%x", 0xff & *(p2->registers +RDINDEXEDDATA_2V_REG/4));
	}
}
		cmn_err(CE_DEBUG,"\n");

/* set the cursor map */
*(p2->registers + RDINDEXLOW_2V_REG/4) = RDCURSORPATTERN_2V_REG;
*(p2->registers + RDINDEXHIGH_2V_REG/4) = RDCURSORPATTERN_2V_REG >> 8;
		cmn_err(CE_DEBUG,
			"RDCURSORPATTERN_2V_REG (0x%x)  =",RDCURSORPATTERN_2V_REG);
{ int i;
  for (i=0;i< 8;i++)
  { 
		cmn_err(CE_DEBUG," 0x%x", 0xff & *(p2->registers +RDINDEXEDDATA_2V_REG/4));
  }
}
		cmn_err(CE_DEBUG,"\n");
		show_it(0);
		show_it(1);
		show_it(2);
		show_it(3);
		show_it(4);
		show_it(6);
		show_it(0xd);
		show_it(0xe);
		show_it(0x1f0);
		show_it(0x1f1);
		show_it(0x1f2);
		show_it(0x1f3);
		show_it(0x200);
		show_it(0x201);
		show_it(0x202);
		show_it(0x203);
		show_it(0x204);
		show_it(0x205);
		show_it(0x206);
		show_it(0x207);
		show_it(0x208);
		show_it(0x209);
		show_it(0x20a);
		show_it(0x20b);
		show_it(0x20c);
		show_it(0x20d);
		show_it(0x20e);
		show_it(0x20f);
		show_it(0x210);
}
