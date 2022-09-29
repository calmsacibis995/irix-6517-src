 /**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************
 *
 * Module: init_graphics.c
 *
 * $Revision: 1.1 $
 *
 * Description: Initialize Venice graphics hardware.
 *
 **************************************************************************/

#include <sys/types.h>
#include <sys/sema.h>
#include "arcs/hinv.h"
#include "arcs/folder.h"
#include "arcs/cfgdata.h"
#include "arcs/cfgtree.h"
#include <sys/gfx.h>
#include <sys/rrm.h>
#include "sys/venice.h"
#include <gl/voftypedefs.h>	/* needed for dg2_eeprom.h */
#include <gl/vofdefs.h>		/* needed for dg2_eeprom.h */
#include <dg2_eeprom.h>
#include "stand_htport.h"
#include "VENICE/gl/cpcmds.h"
#include "VENICE/gl/cpcmdstr.h"
#include "mips_addrspace.h"
#if       defined(IP19)
#include "sys/EVEREST/IP19.h"
#elif defined(IP21)
#include "sys/EVEREST/IP21.h"
#elif defined(IP25)
#include "sys/EVEREST/IP25.h"
#endif
#include "wg.h"


int gl_checkpipesync(void);
int venice_load_vdrc_xilinx(unsigned char *buffer);
int venice_load_vfc_xilinx(unsigned char *buffer);
int venice_rm_ibrev(void);
int venice_load_di_ctrl_xilinx(unsigned char *data);
int venice_loadsection_dg2_top(unsigned int *param_block);
int venice_loadsection_dg2_body(vof_data_t *vofData, int genlockOverride);
int venice_load_vif_xilinx(unsigned char *buffer);
int venice_load_vof_xilinx(unsigned char *buffer);
void sginap(int ticks);
void init_fcg(int bigendian, int init_state, void *base);
void reset_fci(void *something);
int venice_get_dg2_eeprom(venice_dg2_eeprom_t *eeprom) ;
void venice_dg2init(int dummy, int verbose, int doVlist);

static int init_config(void);
static int init_dg(void);


#ifdef DEBUG
extern void ttyprintf(char *fmt, ...);
#define xprintf ttyprintf
#else
/* ARGSUSED */
static void xprintf(char *str, ...)
{
}
#endif
extern void printf(char *fmt, ...);


char _version[] = "RealityEngine2 prom Rev. 0x3000e\n";

/*
 * offsets into the ucode table
 */

#define DG2_XILINX_VIF			0
#define DG2_XILINX_VIF_LEN		1
#define DG2_XILINX_VOF			2
#define DG2_XILINX_VOF_LEN		3
#define DG2_XILINX_VFC			4
#define DG2_XILINX_VFC_LEN		5
#define DG2_XILINX_VDRC			6
#define DG2_XILINX_VDRC_LEN		7
#define DG2_XILINX_DI_CNTL		8
#define DG2_XILINX_DI_CNTL_LEN		9
#define DG2_XILINX_DUMMY3000		10
#define DG2_XILINX_DUMMY3000_LEN	11

#define CP_TPORT			12
#define CP_TPORT_LEN			13

#define GE_TPORT_TEXT			14
#define GE_TPORT_DATA			15
#define GE_TPORT_TEXT_LEN		16
#define GE_TPORT_DATA_LEN		17
#define GE_TPORT_TEXT_ADDR		18
#define GE_TPORT_DATA_ADDR		19

#define BACKUP_VOF			20

#define DG2_XILINX_DI_CNTL_TEST		21
#define DG2_XILINX_DI_CNTL_TEST_LEN	22




extern const unsigned char dg2_xilinx_vif[];
extern const int dg2_xilinx_vif_len;
extern const unsigned char dg2_xilinx_vof[];
extern const int dg2_xilinx_vof_len;
extern const unsigned char dg2_xilinx_vfc[];
extern const int dg2_xilinx_vfc_len;
extern const unsigned char dg2_xilinx_vdrc[];
extern const int dg2_xilinx_vdrc_len;
extern const unsigned char dg2_xilinx_di_cntl[];
extern const int dg2_xilinx_di_cntl_len;
extern const unsigned char dg2_xilinx_dummy3000[];
extern const int dg2_xilinx_dummy3000_len;

extern const unsigned int cp_tport[];
extern const int cp_tport_len;

extern const unsigned short ge_tport_text[];
extern const unsigned short ge_tport_data[];
extern const int ge_tport_text_len;
extern const int ge_tport_data_len;
extern const int ge_tport_text_addr;
extern const int ge_tport_data_addr;

extern struct venice_dg2_eeprom backup_vof;

extern const unsigned char dg2_xilinx_di_cntl_test[];
extern const int dg2_xilinx_di_cntl_test_len;
extern int fcg_cmdmap[];


long ucode_tab[] = {
    0, /* (long)dg2_xilinx_vif, */
    0, /* (long)&dg2_xilinx_vif_len, */
    0, /* (long)dg2_xilinx_vof, */
    0, /* (long)&dg2_xilinx_vof_len, */
    0, /* (long)dg2_xilinx_vfc, */
    0, /* (long)&dg2_xilinx_vfc_len, */
    0, /* (long)dg2_xilinx_vdrc, */
    0, /* (long)&dg2_xilinx_vdrc_len, */
    0, /* (long)dg2_xilinx_di_cntl, */
    0, /* (long)&dg2_xilinx_di_cntl_len, */
    0, /* (long)dg2_xilinx_dummy3000, */
    0, /* (long)&dg2_xilinx_dummy3000_len, */
    0, /* (long)cp_tport, */
    0, /* (long)&cp_tport_len, */
    0, /* (long)ge_tport_text, */
    0, /* (long)ge_tport_data, */
    0, /* (long)&ge_tport_text_len, */
    0, /* (long)&ge_tport_data_len, */
    0, /* (long)&ge_tport_text_addr, */
    0, /* (long)&ge_tport_data_addr, */
    0, /* (long)&backup_vof, */

    0, /* (long)dg2_xilinx_di_cntl_test, */
    0, /* (long)&dg2_xilinx_di_cntl_test_len, */
    0, /* (long)fcg_cmdmap, */
};
#define UT	ucode_tab



static void init_cp(int addr, int *ptr, int cnt);
static void init_ge(int taddr, unsigned int *tptr, int tlen,
	       int daddr, unsigned int *dptr, int dlen);
static void gl_tport_init(int reinit, int xsize, int ysize);


static int GEcount = 1;
static int RMcount = 1;

extern void sginap(int ticks);
extern void us_delay(int);

extern volatile struct fcg *fcg_base_va;

#define SETUP_REGS	/* Assume pipe 0 */	\
			volatile struct fcg *fcg = fcg_base_va

#define FIFO_WAIT	{ SETUP_REGS; int i=0; \
			  while((fcg->gfxwords > 0xdf0) && (i<200)) {\
			       sginap(1); i++;}\
			  if (i >= 200)\
			    xprintf("FIFO_WAIT timed out gfxwords=0x%x\t"   \
				    "rptr == 0x%x  wptr == 0x%x\n",         \
				    fcg->gfxwords, fcg->gfxrptr,            \
				    fcg->gfxwptr);}

#define FIFO_EMPTY	{ SETUP_REGS; int i=0; \
			  while((fcg->gfxwords > 0) && (i<400)) {\
			       sginap(5); i++;}\
			  if (i >= 400)\
			    xprintf("FIFO_EMPTY timed out gfxwords=0x%x\n",\
				    fcg->gfxwords); }

#define FIFO_INFO { SETUP_REGS; xprintf("FCG: gfxwords>>4 == %d  rptr == %d" \
					" wptr == %d\n", fcg->gfxwords>>4,   \
					fcg->gfxrptr, fcg->gfxwptr);}



static
init_config(void)
{
    int rmmask;
    SETUP_REGS;

    /* reset the board */
    reset_fci((void *)fcg);
    init_fcg(1,0,(void *)fcg);

    /* Read the number of RMs. Don't worry about RM termination,
     * it has already been checked during probe time.
     */
    rmmask = fcg->vc[V_DG_RM_PRESENT_L].value & 0xf;
    switch (rmmask & 0xf) {
	case 0x0:			/* standard configs */
	    RMcount = 4; break;
	case 0xc:
	    RMcount = 2; break;
	case 0xe:
	    RMcount = 1; break;
	case 0x3:			/* test configs */
	    RMcount = 2; break;
	case 0xb:
	    RMcount = 1; break;
	default:
	    xprintf("illegal RM4 board count (mask 0x%x) defaulting to 1\n", rmmask);
	    RMcount = 1; break;
    }
    xprintf("setup_config RMcount %d GEcount %d\n", RMcount, GEcount);
    return 0;
}

int
gl_checkpipesync(void)
{
    int i;
    SETUP_REGS;

    FIFO_EMPTY;
    if (fcg->gfxwords != 0) {
	xprintf("fifo not empty ... must be dead\n");
    }

    i = ~fcg->odma_wcnt;

    gecmdstore( CP_CHECKPIPESYNC, i)  ;
    gestore( CP_DATA, 0)  ;
    gestore( CP_DATA, 0)  ;
    gestore( CP_DATA, 0)  ;
    wg_flush();

    FIFO_EMPTY;

    sginap(1000);
    if (fcg->odma_wcnt == i)
	xprintf("check_pipe_sync == alive\n");
    else
	xprintf("check_pipe_sync == dead\n");

    return (fcg->odma_wcnt == i);
}

static int
init_dg(void)
{
    extern int ven_eeprom_valid;
    extern struct venice_info VEN_info;
    extern struct venice_dg2_eeprom ven_eeprom;
    ven_eeprom_valid = venice_get_dg2_eeprom(&ven_eeprom);
    VEN_info.rm_count = RMcount;
    venice_dg2init(0, 1/*verbose*/, 0/*vlist*/); /* do not init vlist */
    return 0;
}


static void
gl_tport_init(int reinit, int xsize, int ysize) {

    extern struct venice_info VEN_info;

    gecmdstore( CP_TPORT_INIT, reinit)  ;
    gestore( CP_DATA, xsize)  ;
    gestore( CP_DATA, ysize)  ;
    gestore( CP_DATA, VEN_info.pixel_depth)  ;
    gestore( CP_DATA, VEN_info.tiles_per_line)  ;
    gestore( CP_DATA, 0)  ;

    wg_flush();
    xprintf("gl_tport_init: reinit=%d, xsize=%d, ysize=%d, pixeldepth=%d, tiles_p_line=%d\n",reinit,xsize,ysize,VEN_info.pixel_depth,VEN_info.tiles_per_line);
}

static void
init_cp(int addr, int *ptr, int cnt)
{
    xprintf("init_cp(0x%x, 0x%x, %d)\n",addr, ptr,cnt);


    /* feed it all to the pipe */
    gecmdstore((0x400|CP_LOADUCODE),cnt+2);
    gestore(CP_DATA,addr);
    gestore(CP_DATA,cnt);
    
    while (cnt--)
     {
       FIFO_WAIT;
       gestore(CP_DATA, *ptr);
       ptr++;
       
       wg_flush();
     }


    gecmdstore( CP_INIT_CP, GEcount - 1)  ;
    gestore( CP_DATA, RMcount - 1)  ;
    wg_flush();
    us_delay(5000);

    FIFO_EMPTY;
}


#define BSWAP(x)    (           \
    ((x) & 0xff000000) >> 24 |  \
    ((x) & 0x00ff0000) >> 8  |  \
    ((x) & 0x0000ff00) << 8  |  \
    (x) << 24                   \
    )



static void
loadsection_ge8(unsigned int *ptr, unsigned int addr, int cnt, int chunksize) {
	int n, f;

xprintf("loadsection_ge8(0x%x 0x%x, 0x%x, 0x%x)\n",(long)ptr,
	(unsigned long)addr, cnt, chunksize);

	/* convert byte count to word count */
	cnt = cnt/(int)sizeof(int);

	while (cnt) {
		FIFO_WAIT;

		n = MIN(chunksize,cnt);	/* this count */
		f = chunksize - n;	/* end filler */
		gecmdstore( CP_GE_LOAD, addr)  ;
		gestore( CP_DATA, n)  ;

		wg_flush();     /* XXX temporary */

		cnt -= n;
		addr += n*sizeof(int);
		while (n--) {
		    gestore(CP_DATA,BSWAP(*ptr));
		    ptr++;
		}
	}

	/* fill last command out if necessary */
	while (f--)
	    gestore( CP_DATA, 0xdeadbeef)  ;

	wg_flush(); 
}


static void
init_ge(int taddr, unsigned int *tptr, int tlen,
	int daddr, unsigned int *dptr, int dlen)
{

    int csize, i;
    unsigned int *bootstrap = (unsigned int *)tptr;
    unsigned int size;

xprintf("init_ge(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",(unsigned long)taddr,
	tptr,(unsigned long)tlen, (unsigned long)daddr,(unsigned long)dptr,
	(unsigned long)dlen);

    gecmdstore( CP_GE_BOOTSTRAP, 1)  ;	/* broadcast */

    size = BSWAP(bootstrap[0]);
    for (i = 1; i <= size; ++i)
     {
       gestore( CP_DATA, BSWAP(bootstrap[i]));
     }


    csize = 32 - size - 2;
    /*          ^packet_buffer_size ^args(addr and count) */

    loadsection_ge8(tptr, taddr, tlen, csize);
    loadsection_ge8(dptr, daddr, dlen, csize);
    gecmdstore( CP_GE_GOTO, taddr+0x40)  ;	/* XXX */
    gecmdstore( CP_INIT_GE, GEcount)  ;
    gestore( CP_DATA, RMcount)  ;
    gestore( CP_DATA, venice_rm_ibrev())  ; 
    wg_flush();

    FIFO_EMPTY;
}

int xsize = 1280;
int ysize = 1024;

int
init_graphics(void)
{
    extern int gfx_ok;

    printf("%s\n",_version);

    if (gl_checkpipesync()) {
	xprintf("already inited ... skipping\n");
	gl_tport_init(1, 0, 0);
	return 0;
    }
    if (init_config()) return 1;

    init_cp(0, (int *)UT[CP_TPORT], (*(int *)UT[CP_TPORT_LEN])/(int)sizeof(int) );

    init_ge(*(int *)UT[GE_TPORT_TEXT_ADDR],
	     (unsigned int *)UT[GE_TPORT_TEXT],
	    *(int *)UT[GE_TPORT_TEXT_LEN],
	    *(int *)UT[GE_TPORT_DATA_ADDR],
	     (unsigned int *)UT[GE_TPORT_DATA],
	    *(int *)UT[GE_TPORT_DATA_LEN]);

    xprintf("init_dg\n");
    if (init_dg()) return 1;

    xprintf("init_tport\n");
    gl_tport_init(0, xsize, ysize);

#ifdef DEBUG
gl_checkpipesync();
#endif

    return 0;
}

#if 0
#ifndef MIRAGE
void
gl_tport_char(int c) {

    gecmdstore( CP_TPORT_CHAR, c)  ;
    wg_flush();
}


static void
gl_tport_str(char *s) {
    while(*s) gl_tport_char(*s++);
}
#endif
#endif

int venice_get_vcreg(int addr);
void venice_set_vcreg(int addr, int data, int fifo);


/*
 *  DG stuff
 */
venice_get_vcreg(int addr)
{
    SETUP_REGS;

    return (fcg->vc[addr].value);
}


void
venice_set_vcreg(int addr, int data, int fifo) 
{

    SETUP_REGS;
    if (fifo) {
	fcg->vc[addr+0x20000].value = data;
    }
    else {
	fcg->vc[addr].value = data;   
    }
}

/* 
 * versions of venice_clock_serial_dataByte() for xilinx downloading
 * with venice_set_vcreg routines inlined for speed.
 */
void
venice_clock_serial_dataByte(int value)
{
    int i,localvalue;
    SETUP_REGS;


    localvalue = value;
    localvalue <<= 3;  /* since mask == 8 XXXX ugly */

    for(i = 0; i < 8; i++) {
 	fcg->vc[V_DG_SER_BUS].value = V_DG_SER_DATA_MASK & localvalue; \
	fcg->vc[V_DG_SERIAL_CLOCK].value = 1;			\
	localvalue >>= 1;
    }
}


void
venice_finish_vdrc_sram(int sramByteCount, int offsetSize, int vof_ILoffset, int length) {
    int i, j;
    SETUP_REGS;


    i = offsetSize - sramByteCount;

    /* unroll by 8 */
    j = i & 7;
    i >>= 3;
    while(i-- > 0) {
	fcg->vc[V_DG_VDRC_DATA].value = 0;
	fcg->vc[V_DG_VDRC_DATA].value = 0;
	fcg->vc[V_DG_VDRC_DATA].value = 0;
	fcg->vc[V_DG_VDRC_DATA].value = 0;
	fcg->vc[V_DG_VDRC_DATA].value = 0;
	fcg->vc[V_DG_VDRC_DATA].value = 0;
	fcg->vc[V_DG_VDRC_DATA].value = 0;
	fcg->vc[V_DG_VDRC_DATA].value = 0;
    }

    while(j-- > 0) 
	fcg->vc[V_DG_VDRC_DATA].value = 0;


    for(i = 0; i < 8; i++) {
	fcg->vc[V_DG_VDRC_DATA].value = vof_ILoffset;
	fcg->vc[V_DG_VDRC_DATA].value = length;
    }
}

void venice_dg2initXilinx() {
    /* download some dummies to force tristating */
    int rmp;

    xprintf("load xilinxes\n");
    venice_load_vdrc_xilinx((unsigned char *)UT[DG2_XILINX_DUMMY3000]);
    venice_load_vfc_xilinx((unsigned char *)UT[DG2_XILINX_DUMMY3000]);

    venice_load_vdrc_xilinx((unsigned char *)UT[DG2_XILINX_VDRC]);
    /* only needed for the encoder */
    venice_load_vfc_xilinx((unsigned char *)UT[DG2_XILINX_VFC]);

    venice_load_vof_xilinx((unsigned char *)UT[DG2_XILINX_VOF]);
    venice_load_vif_xilinx((unsigned char *)UT[DG2_XILINX_VIF]);

    rmp = venice_get_vcreg(V_DG_RM_PRESENT_L) & 0xf;
    if (rmp == 0x3 || rmp == 0xb) {
	xprintf("DG test config detected, using di_cntrl_testconfig xilinx\n", 0);
	venice_load_di_ctrl_xilinx((unsigned char *)UT[DG2_XILINX_DI_CNTL_TEST]);
    } else {
	venice_load_di_ctrl_xilinx((unsigned char *)UT[DG2_XILINX_DI_CNTL]);
    }


}

enoughRMs(int xsize, int ysize, int RMcount) {
    int t_width, t_height, t_size;
    int tileWidth;

    tileWidth = 80 * RMcount;
    t_width = (xsize + (tileWidth - 1)) / tileWidth;
    t_height = (ysize + 7) >> 3;
    t_size = t_width * t_height;
    return t_size <= 2048;
}

void
venice_dg2initVof() {
    int top, body;
    extern int vof_clock_value;
    extern int ven_eeprom_valid;
    extern struct venice_dg2_eeprom ven_eeprom;
    venice_dg2_eeprom_t *ee;

    if (ven_eeprom_valid) {
	ee = &ven_eeprom;
	if (!enoughRMs(ee->display_surface_width,ee->display_surface_height,RMcount)) {
	    xprintf("RealityEngine eeprom VOF too big, using backup VOF\n", 0);
	    ee = (venice_dg2_eeprom_t *)UT[BACKUP_VOF];
	    ven_eeprom_valid = 0;
	}
    } else {
	xprintf("RealityEngine eeprom invalid, using backup VOF\n", 0);
	ee = (venice_dg2_eeprom_t *)UT[BACKUP_VOF];
    }

    /*
     * xsize and ysize are globals which are used in a call to 
     * gl_tport_init() to set up the RE framebuffer size.  Thus,
     * we set them using the display_surface_width and 
     * display_surface_height fields that setmon wrote into the DG2
     * eeprom for us.  These fields will have been adjusted to compensate
     * for field replicated vofs by setmon prior to writing the eeprom.
     */
    xsize = ee->display_surface_width;
    ysize = ee->display_surface_height;

    top = venice_loadsection_dg2_top((unsigned int *)&(ee->normal_vof_top[0]));
    body = venice_loadsection_dg2_body(&ee->normal_vof_body, 0);
    
    if (!top && !body) {
	/*
	 * Set the clock value to its real value after the edge memories
	 * have been loaded, but before video is enabled.
	 */
	venice_set_vcreg(V_DG_PROG_CLK, vof_clock_value, 0);
	return;
    }
    xprintf("eeprom vof load failed: header loaded (%d), body loaded (%d)\n",
	  top, body);
}




/*
 *  probably could use a lot of calibration
 */
#if defined(IP19) || defined(IP21) || defined(IP25)
void
sginap(int n) {
	DELAY(n * 1500);
}
#else
void
sginap(int n)  {
    volatile int x;
    if (n <= 0) n = 1;
    x = 3*500*n;
    while(--x) ;
}
#endif


int
venice_get_xsize()
{
    return xsize;
}

int
venice_get_ysize()
{
    return ysize;
}
