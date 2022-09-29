/*
* Copyright 1998 - PsiTech, Inc.
* THIS DOCUMENT CONTAINS CONFIDENTIAL AND PROPRIETARY
* INFORMATION OF PSITECH.  Its receipt or possession does
* not create any right to reproduce, disclose its contents,
* or to manufacture, use or sell anything it may describe.
* Reproduction, disclosure, or use without specific written
* authorization of Psitech is strictly prohibited.
*
*/
#ifdef _STANDALONE
#include <sys/SN/klconfig.h>   /* config stuff */
#else
#include <sys/kmem.h>
#include <sys/kopt.h>
#endif
#include "sys/types.h"  /* types.h must be before systm.h */
#include "sys/systm.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/cpu.h"
#include "sys/invent.h"
#include "string.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/mips_addrspace.h"
#include "sys/edt.h"
#include "sys/kabi.h"
#include <sys/ksynch.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/PCI_defs.h>

#ifdef _STANDALONE
#if 0
# include "libsc.h"
# include "libsk.h"
# include "cursor.h"
#endif
# include "stand_htport.h"
#define pon_puts(msg) /* ttyprintf(msg)  */
#else
# include "sys/htport.h"
# include "sys/errno.h"
#define pon_puts(msg)   /* cmn_err(CE_DEBUG, msg)  */
#endif /* STANDALONE */

/*#include "sys/gfx_flow.h"*/

#include "rad1hw.h"
#include "rad1.h"
#include "rad4pci.h"
#include "rad1sgidrv.h"
#include "rad1tp.h"

#ifdef IP27
#ifdef _STANDALONE
#include "standcfg.h"
#include <sys/SN/SN0/klhwinit.h>   /* config stuff */
#include <sys/SN/SN0/addrs.h>   /* config stuff */
#endif /*  _STANDALONE */
/*#include "sys/SN/addrs.h"*/
/*#include "sys/SN/agent.h"*/
#undef STATUS
#if 0
#include <sys/SN/klconfig.h>  /* for PROM hardware graph */
#include <sys/SN0/slotnum.h> 
#endif
static void hub_set_fire_n_forget(nasid_t nasid, xwidgetnum_t widgetid);
#endif


#ifdef _STANDALONE

per2_info RAD1Infos[MAX_RAD1S];
int RAD1BdsInitted;
void *rad1_info_pointer;
rad1boards      RAD1Boards[MAX_RAD1S];
rad1boards      RAD1_nonvolatile_aggregate[MAX_RAD1S] = {
    { &RAD1Infos[0], 0, 0, 0,
    },
    { &RAD1Infos[1], 0, 0, 0,
    }
};

#else /* !_STANDALONE */

rad1boards      RAD1Boards[MAX_RAD1S] = {
    { NULL, 0, 0, 0,
    },
    { NULL, 0, 0, 0,
    }
};
#endif /* STANDALONE */

#ifndef _STANDALONE

/* For user memory locking limit */
extern int MgrasMaxLockableMemPages;
extern int MgrasCurrLockedMemPages;
extern int MgrasLockedBoundaryMsgSent;
extern int MgrasXMaxLockableMemPages;

extern int gfxlockablemem;                      /* Tunable parameter */
extern int gfxlockablemem_gt64;                 /* Tunable parameter */

extern void *rad1_info_pointer;
extern int *rad1_earlyinit_base;
#endif /* STANDALONE */

extern struct htp_fncs RAD1_htp_fncs;
extern struct standcfg standcfg[];

/* some prototypes */

/******************************************************************************
 * P R O M - O N L Y   I N I T I A L I Z A T I O N   R O U T I N E S
 *****************************************************************************/
#ifdef _STANDALONE

/* The probe routine no longer serves its traditional purpose on high
 * end systems.  All probing is done when the prom hardware graph is
 * initialized.  The only remaining purposes of mgras_probe are:
 * 1) if fncs=0, we return any pipe's base address
 * 2) if fncs!=0, install the textport callback functions and return 0
 */
void *
rad1_probe(struct htp_fncs **fncs)              /* standalone */
{

    if (!RAD1BdsInitted)
        {
                pon_puts("rad1_probe: should not be called before rad1_install\n");
                return (void *) 0;
    }

    if (fncs)
        {
                *fncs = (struct htp_fncs *)&RAD1_htp_fncs;
                return (void *) 0;
    }

    return &rad1_info_pointer;
}

extern struct standcfg standcfg[];
extern int _gfx_strat(COMPONENT *, IOBLOCK *);
extern int kl_graphics_AddChild(COMPONENT *, char *, int , int(*)());
extern vertex_hdl_t hw_vertex_hdl ;
#define PSITECH_ID    0x12d1

int
rad1_install(COMPONENT *comptp)    /* standalone */
{
        char idbuf[100];
        per2_info *p2;
        struct standcfg     *s;
        void *space0;
        void *space2;
        unsigned mask;
        nasid_t nasid;
        pcicfg_t * pcdp;

        pcdp = (pcicfg_t *)GET_PCICFGBASE_FROM_KEY(comptp->Key);
	/* if(pcdp->sub_sys_vendor_id != PSITECH_ID) return FALSE;  */
        nasid = GET_HSTNASID_FROM_KEY(comptp->Key);
        mask = 0xf;
        space0 = (char *)NODE_BWIN_BASE(nasid, 0) +
            (pcdp->base_addr_regs[0] & ~mask);
        space2 = (char *)NODE_BWIN_BASE(nasid, 0) +
            (pcdp->base_addr_regs[2] & ~mask);
 

        if (!RAD1BdsInitted)
        {
            RAD1BdsInitted = 1;
            bcopy((char *) RAD1_nonvolatile_aggregate, (char *)RAD1Boards,
                    sizeof(rad1boards) * MAX_RAD1S);
        }

    /* textport pipe is always pipe 0 since IP27 only supports one pipe */
        p2 = RAD1Boards[0].p2;
        p2->registers = (int *)space0;
        p2->ram2 = (int *)space2;
        p2->device_id = RAD1_2V_DEVICE_ID;
        
        RAD1InitBoard(p2);      /* uses p2->ram2 & p2->registers */

        rad1_info_pointer = p2;

	strcpy(idbuf,"Psitech-RAD1");
	comptp->Identifier = idbuf;
	comptp->IdentifierLength = strlen(idbuf);

#ifdef nodef
#ifndef SN_PDI
    kl_graphics_AddChild(c, idbuf, KLSTRUCT_GFX, _gfx_strat); 
#endif
#endif

        return 0;
}


#if 0
int
RAD1ProbeAll(char *base[])                      /* standalone */
{
        unsigned int *hw;
        rad1tp_info *info;
        int board, bd = 0;

        if (!RAD1BdsInitted)
        {
            RAD1BdsInitted = 1;
            bcopy((char *)RAD1_nonvolatile_aggregate, (char *)RAD1Boards,
                    sizeof(rad1boards) * MAX_RAD1S);
        }

        for (board = 0; board < MAX_RAD1S; board++)
        {
            hw = RAD1Boards[board].base;
            if (RAD1Probe(hw, board))
                {
                        hw = RAD1Boards[board].base;
                        /*
                         * For standalone --
                         * Want to return probe results to pass to power-on diags.
                         */
                        base[bd++] = (char *) hw;

                        /*
                         * standalone resetcons calls rad1tp_probe expecting that
                         * RAD1Reset will be called.  So here we unconditionally
                         * call RAD1InitBoard, which will call RAD1Reset.
                         */
                        RAD1InitBoard(p2);

                        /* XXX - check out the following ... */
                        /* The Indigo2 logo does not fit on the small screen */
                        info = RAD1Boards[board].info;
                        if (info->gfx_info.xpmax < 1280 || info->gfx_info.ypmax < 1024)
                        {
                                logoOff();
                        }
            }
        }
        return bd;
}

/*
 * Probe routine set by .cf file and used by the textport.  The tp calls
 * with fncs=0 to find base addresses.  Each call with fncs=0 should return
 * the base of the next board.  We return 0 to indicate all the boards
 * have been reported.  If fncs is != 0, we return the functions for the
 * graphics board.
 */
void *
rad1tp_probe(struct htp_fncs **fncs)            /* standalone */
{
        static char *base[MAX_RAD1S];
        static int i;
#ifdef MFG_USED
extern  char *getenv(const char *);
        char *p = getenv("diagmode");

        if (p && (p[0] == 'N') && (p[1] == 'I')) {
                printf("\n\
                ===============================================================\n\
                **WARNING: GFX KERNEL IS SKIPPING INITIALIZATION **\n\
                **WARNING: THE NVRAM VARIABLE diagmode IS SET TO NI**\n\
                This IDE must be used ONLY to repair IMPACT boards\n\
                To load USUAL IDE, go to PROM, unsetenv diagmode and re-boot IDE\n\
                ===============================================================\n\n");
                return 0;
        }
#endif
        if (!RAD1BdsInitted)
        {
            RAD1BdsInitted = 1;
            bcopy((char *)RAD1_nonvolatile_aggregate, (char *)RAD1Boards,
                    sizeof(rad1boards) * MAX_RAD1S);
        }

        if (fncs)
        {
                *fncs = (struct htp_fncs *)&RAD1_htp_fncs;
                i = 0;
                return (void *) 0;
        }

        if (i == 0)
        {
                bzero(base, sizeof(base));
                (void) RAD1ProbeAll(base);
        }

        if (i < MAX_RAD1S)
                return (void *) base[i++];
        else
                return (void *) 0;
}

extern COMPONENT montmpl;
extern int _gfx_strat();
#endif /* 0 */
#endif /* _STANDALONE */

/******************************************************************************
 * K E R N E L - O N L Y   I N I T I A L I Z A T I O N   R O U T I N E S
 *****************************************************************************/

#ifndef _STANDALONE

/* called once per board at device attach time by rad1pciattach */
void
RAD1BoardTPInit(per2_info *p2, int board)
{
        pon_puts("!Entering RAD1BoardTPInit\n"); /**/
        if (board >= MAX_RAD1S)
                return;

        RAD1Boards[board].p2 = p2;
        pon_puts("!Leaving RAD1BoardTPInit\n");
}
int
rad1_earlyinit(void)
{	char c;
	static int done_once = 0;
	if(done_once == 0)
	{
		c = is_specified(arg_monitor) ? arg_monitor[0] : 'h';
		rad1_info_pointer = NULL;
		rad1_earlyinit_base = (int *)kmem_zalloc(RAD1_SGRAM_SIZE,
				KM_SLEEP|KM_PHYSCONTIG|KM_CACHEALIGN); /* */
#pragma weak htp_register_board
		if(htp_register_board != 0)	/* if not defd dont bother */
		{
		switch(c)
		{
			case 's':
			case 'S':
			case 'h':
			case 'H':
				htp_register_board(&rad1_info_pointer, &RAD1_htp_fncs, 1280, 1024);
				break;
			case 'l':
			case 'L':
				htp_register_board(&rad1_info_pointer, &RAD1_htp_fncs, 1024, 768);
				break;
		}
		}
		done_once = -1;
	}
	return 1;
}
#endif /* !_STANDALONE */

/******************************************************************************
 * C O M M O N   I N I T I A L I Z A T I O N   R O U T I N E S
 *****************************************************************************/
volatile int vol;
void
RAD1InitBoard(per2_info *p2)
{
        volatile int *registers = p2->registers;
        int i;
        int board = RAD1_baseindex(p2);

        pon_puts("!Entering RAD1InitBoard\n\r");
        /* Reset the HW */
        RAD1Reset(registers, board);

#if 0
        if (!RAD1Boards[board].infoFound) {
            RAD1GatherInfo(p2);
        }
#if !defined(_STANDALONE)
        RAD1Reset(registers, board);
#endif
#endif

        /* Init the HW */
        rad1_pinit(p2, 0);
        
        /* clear the screen */
        while (*(registers + INFIFOSPACE_REG/4) < 9)
  {
    vol = 0;
  } /* wait for room in register FIFO */

        p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
        p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_ENABLE;
        *(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
        *(registers + FBWRITEDATA_REG/4) = 0;
        *(registers + FBBLOCKCOLOR_REG/4) = 0;
        *(registers + RECTANGLEORIGIN_REG/4) = RECTORIGIN_YX(0, 0);
        *(registers + RECTANGLESIZE_REG/4) = 
                RECTSIZE_HW(p2->v_info.y_size, p2->v_info.x_size);
        *(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEX |
                RENDER_INCREASEY | RENDER_FASTFILLENABLE;

        p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_DISABLE;
        *(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
        p2->p2_videocontrol.bits.enable = RAD1_ENABLE;
        *(registers + VIDEOCONTROL_REG/4) = p2->p2_videocontrol.all;
        *(registers + FBHARDWAREWRITEMASK_REG/4) = p2->hard_mask;

        RAD1Boards[board].boardInitted = 1;

#if 0
#ifdef _STANDALONE
        /* The Indigo2 logo does not fit on the small screen */
        if ((p2->v_info.x_size < 1280) || (p2->v_info.y_size < 1024)) {
                logoOff();
        }
#endif
#endif
        pon_puts("!Leaving RAD1InitBoard\n\r");
}

/******************************************************************************
 * B O A R D   R E S E T
 *****************************************************************************/

void
RAD1Reset(volatile int *registers, int board)
{
        pon_puts("!Entering RAD1Reset\n\r"); /**/
        *(registers + RESETSTATUS_REG/4) = 0;   /* reset GP */
        while (*(registers + RESETSTATUS_REG/4) & SOFT_RESET_FLAG)
  {
    vol = 0;
  } /* wait for reset to complete */

        RAD1Boards[board].boardInitted = 0;
        pon_puts("!Leaving RAD1Reset\n\r");
}

/******************************************************************************
 * B O A R D   I N I T I A L I Z A T I O N
 *****************************************************************************/
static char c_img[64*64*2/8] = 
	{ 	
0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0a,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x2e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xbe,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xfe,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xfe,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xfe,0x2f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xfe,0xbf,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xfe,0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xfe,0xab,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xbe,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xae,0x2f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0a,0x2e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xbe,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void
RAD1InitCursor(per2_info *p2)
{
        volatile int *registers = p2->registers;
	volatile int *cursor_ptr;
	char *t_cp;
	int i;

	cursor_ptr = registers + RDINDEXEDDATA_2V_REG/4;
	*(registers + RDINDEXLOW_2V_REG/4) =
                RDCURSORPALETTE_2V_REG;
	*(registers + RDINDEXHIGH_2V_REG/4) =
                RDCURSORPALETTE_2V_REG >> 8;
	*cursor_ptr = 0xff;
	*cursor_ptr = 0xff;
	*cursor_ptr = 0xff;
	*cursor_ptr = 0xff;
	*cursor_ptr = 0;
	*cursor_ptr = 0;
	WR_INDEXED_REGISTER_2V(p2->registers, RDCURSORMODE_2V_REG,
              RAD1_ENABLE | (TYPE_X_2V << 4 ));

	cursor_ptr = registers + RDINDEXEDDATA_2V_REG/4;
	t_cp = c_img;
	WR_INDEXED_REGISTER_2V(registers, RDCURSORHOTSPOTX_2V_REG, 0);
	WR_INDEXED_REGISTER_2V(registers, RDCURSORHOTSPOTY_2V_REG, 0);
	*(registers + RDINDEXLOW_2V_REG/4) = RDCURSORPATTERN_2V_REG;
	*(registers + RDINDEXHIGH_2V_REG/4) = RDCURSORPATTERN_2V_REG >> 8;
	for(i=0; i<64*64*2/8; i++)
	{
		*cursor_ptr = *t_cp++; /* */
	}
}

/*
 * Return index into RAD1Boards.
 * If no matching entry, return -1.
 */
int
RAD1_baseindex(per2_info *p2)
{
        int i;
        
        for (i = 0; i < MAX_RAD1S; i++)
        {
                if (RAD1Boards[i].p2 == p2)
                        return i;
        }
        return -1;
}

/* TIMING TABLES */
#ifdef _STANDALONE
#define PER2_CONSOLE_ROM
#include "rad1data.c"
#endif /* STANDALONE */


