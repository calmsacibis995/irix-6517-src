/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.47 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/reg.h>
#include <sys/pda.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/sysinfo.h>
#include <sys/cmn_err.h>
#include <sys/stream.h>
#include <sys/vmereg.h>

/* ARGSUSED */
void
vme_yield(uint notused1, uint notused2)
{
}


/* belongs in memsupport.c */
/* table of probeable kmem addresses */
struct kmem_ioaddr kmem_ioaddr[] = {
	{ 0, 0 },
};


void
wgtimo()
{
}


void
ev_error()
{
	cmn_err(CE_WARN, "error detected.  ev_error not yet implemented\n");
	debug(0);
}

char eaddr[6];

/*
 * WD93 stubs
 */
int wd93cnt, wd93burstdma;
void *wd93dev;
/* ARGSUSED */
void wd93intr(int a) { }
/* ARGSUSED */
int wd93_earlyinit(void *p) { return 0; }

#if SABLE
#include <sys/scsi.h>
/* ARGSUSED */
syscintr(int a) { }
/* ARGSUSED */
void wd95intr(int a) { }
/* ARGSUSED */
int wd95_earlyinit(int a) { return 0; }
/* ARGSUSED */
int wd95_present(char *p) { return 0; }
/* ARGSUSED */
int	dksc_ctlr(dev_t a) { return 0; };
/* ARGSUSED */
int	dksc_lun(dev_t a) { return 0; };
/* ARGSUSED */
int	dksc_unit(dev_t a) { return 0; };
void (*scsi_command[1])(struct scsi_request *req)
	= { NULL};

int (*scsi_alloc[1])(u_char ca, u_char ta, u_char la, int opt, void (*cb)())
	= { NULL};

void (*scsi_free[1])(u_char cf, u_char tf, u_char lf, void (*cb)())
	= { NULL};

u_char scsi_driver_table[1];
#endif


