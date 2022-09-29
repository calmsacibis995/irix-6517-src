/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 *	danglib.h - dang chip library function definitions		*
 *									*
 ************************************************************************/

#ident  "arcs/ide/EVEREST/io4/graphics/danglib.h $Revision: 1.4 $"

#ifndef	DANGLIB_H
#define DANGLIB_H

#include <sys/EVEREST/dang.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/gr2.h>
#include <sys/gr2hw.h>

/* Added to enable build of dang_wg and danglib for shiva */

#if defined(IP25)
#define EV_WGDST EV_GRDST
#endif

/*
 * read timeout - using LONG 16 second val to get around dang false timeout
 * bug - may go down to the suggested 1 ms when the good dang chips come out
 */
/*
#define IDE_DANG_RDTIMEOUT	16000000
*/
#define IDE_DANG_RDTIMEOUT	10000		/* 10 ms - just at test */

#if defined(IP25)
/*
 * use 4k page size for the graphics stuff
 * DMA pages per shot may need to be scaled down
 */
#define	DIAG_GFX_PAGESIZE	0x1000
#define IDE_PGSPERSHOT		32
#else
/*
 * use 4k page size for the graphics stuff
 */
#define	DIAG_GFX_PAGESIZE	0x1000
#define IDE_PGSPERSHOT		32
#endif 

/*
 * printout levels for the ev_perror routine in everror.c
 */
#define NONE_LVL        0
#define SLOT_LVL        1
#define ASIC_LVL        2
#define REG_LVL         3
#define BIT_LVL         4

/*
 * routine to print the name and status of a selected dang register
 */
int print_dangreg_status(int reg_offset,  long long reg_value);
  
/*
 * routine to print the status a selected DANG chip
 */
int print_dangstatus (int slot, int adap);

/*
 * work analogiously to the xxx_err_log and xxx_err_show routines
 * the log routine saves the current dang status - the show routine
 * prints it out whenever needed.  This wants an adapter number, though,
 * not a bit array of several adapters.
 */
void dang_log_status(int slot, int adap);
void dang_show_status(int slot, int adap);

/*
 * routine to initialize a DANG chip for operation
 * returns NULL if unable to allocate LW address, else LW ptr
 */
void * ide_dang_init(int slot, int adap);

/*
 * routine to check if GR2 exists at the specified address
 * gr2ptr sb in the large window address returned by ide_dang_init
 */
int ide_Gr2Probe(struct gr2_hw * gr2ptr);

/*
 * returns the number of GEs present
 * code lifted from the IP20 diags
 */
int
ide_Gr2ProbeGEs(struct gr2_hw *base);

/*
 * set up the gr2 for testing
 * returns the gr2 config info in the supplied info structure
 */
int
ide_Gr2Config(long long *dang_ptr,
	      struct gr2_hw * gr2ptr,
	      struct gr2_info *info);
/*
 * central location for all status stuff to refer to - this way I can
 * roll the actual setting into one routine in the interrupt service
 * code
 */
extern long long d_pioerr, d_mdmastat, d_mdmaerr, d_sdmastat, d_sdmaerr,
	         d_intstat, d_wgstat;

/*
 * sets up a DANG dma frame
 */
void
ide_Gr2mkudmada(
    char *buf,                  /* virtual address of data */
    gdmada_t *dmadap,           /* DMA desc. array ptr */
    int rows,                   /* number of rows */
    int bytesperrow,            /* bytes to transfer  in one row */
    int bytesperstride,         /* bytes to stride in one row */
    int offset,			/* offset to first byte */
    unsigned int direction,     /* 0: GIO to IBUS, 1: IBUS to GIO */
    unsigned int gio_static,    /* 0: free-running GIO addr, 1: GIO fixed */
    unsigned int dma_sync,      /* 0: no sync needed, 1: sync for gio dma */
    unsigned int dma_intr,      /* 0: no completion intr, 1: intr on comp */
    void *gfxaddr);

/*
 * triggers dma - has to do some cleanup on GR2 before before starting
 */
int
ide_GR2DMAtrigger(
    long long *adptr,
    struct gr2_hw *base,
    void *dmahead);

/*
 * print out the header info - this is for debugging routines
 */
void
print_gdmada_t(gdmada_t * dmadap);

#endif /*DANGLIB_H*/
