/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986,1992, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Support routines for the Everest implementation of the ARCS
 * standard.  Unlike the ESD systems, Everest doesn't keep the ARCS
 * code in an executible PROM.  As a result, once Unix has started
 * most of the PROM executible is overwritten.  To avoid problems,
 * this module patches the SPB transfer vector table so that certain
 * important functions pointers are pointed to the appropriate 
 * kernel functions defined in this module.  Not all ARCS functions
 * are supported; if the kernel calls an unsupported function the
 * system will panic.
 */

#ident "$Revision: 1.15 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/immu.h>
#include <sys/iograph.h>
#include <sys/pda.h>
#include <sys/conf.h>
#include <sys/arcs/spb.h>
#include <sys/arcs/tvectors.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/nvram.h>
#include "sys/EVEREST/epc.h"
#include "sys/z8530.h"

extern int _prom_cleared;

/*
 * he_arcs_set_vectors()
 *	set the global _prom_cleared so that arcs_* routines panic the kernel
 */

void
he_arcs_set_vectors(void)
{
	_prom_cleared = 1;
}


void
restart(int mode)
{
	extern int set_nvreg(int, char);

	if (cpuid() == masterpda->p_cpuid) {
		set_nvreg(NVOFF_RESTART, mode);
		EV_SET_LOCAL(EV_KZRESET, 0x1); 
	} else {
		/* Hang out waiting for the machine to reboot */

#ifdef __BTOOL__
#pragma btool_prevent
		/* btooling this can cause a hang due to so much
		 * contention for the one cache line containing the
		 * counter
		 */
		while (1) /* Loop forever */ ;
#pragma btool_unprevent
#else /* !__BTOOL__ */
		while (1) /* Loop forever */ ;
#endif
	}

	panic("Restart attempt failed");
}


/*
 * he_arcs_restart()
 *	
 */

void
he_arcs_restart(void)
{
	restart(PROMOP_RESTART);
}


/*
 * reboot()
 *	
 */

void
he_arcs_reboot(void)
{
	restart(PROMOP_REBOOT);
}


int
he_arcs_write(char *buf, ULONG n, ULONG *cnt)
{
	static volatile char *cntrl = NULL;
	static volatile char *data  = (char*) NULL;
	if (cntrl == NULL) {
		cntrl = (volatile char*) DUART2A_CNTRL;
		data  = (volatile char*) DUART2A_DATA;
	}

	*cnt = n;
	while (n--) {
		while (!(*cntrl & RR0_TX_EMPTY))
			/* spin */ ;
		*data = *buf;
		if (*buf == '\n') {
			while (!(*cntrl & RR0_TX_EMPTY));
			*data = '\r';
		}
		buf++;
	}
	return 0;
}

#if _K64PROM32
/* if start out with a PROM32, then SBP is 32-bit, so need
 * to change it right away to 64-bit flavor SPB
 */
void
swizzle_SPB()
{
	if(SPB->Signature != SPBMAGIC) {
	__int32_t f1, f2, f3, f4;
		f1 = SPB32->TransferVector;
		f2 = SPB32->TVLength;
		f3 = SPB32->PrivateVector;
		f4 = SPB32->PTVLength;

		SPB->Signature = SPBMAGIC;
		SPB->Length = sizeof(spb_t);
		SPB->Version = SGI_ARCS_VERS;
		SPB->Revision = SGI_ARCS_REV;
		SPB->RestartBlock = 0;
		SPB->DebugBlock = 0;
		SPB->AdapterCount = 0;

		SPB->TransferVector = (FirmwareVector *)PHYS_TO_K1(f1 & 0x1fffffff);
		SPB->TVLength = f2;
		SPB->PrivateVector = (long *)PHYS_TO_K1(f3 & 0x1fffffff);
		SPB->PTVLength = f4;

		/* leave SPB->GEVector and utlb miss vector random for now */
	}
}
#endif
#define __DEVSTR2 "/target/"
#define __DEVSTR3 "/lun/0/disk/partition/"
#include <sys/iograph.h>
void
devnamefromarcs(char *devnm)
{
	int val;
	char tmpnm[MAXDEVNAME];
	char *tmp1, *tmp2;

	val = strncmp(devnm, "dks", 3);
	if (val == 0) {
		tmp1 = devnm + 3;
		sprintf(tmpnm,"/hw/%s/",EDGE_LBL_SCSI_CTLR);
		tmp2 = tmpnm + strlen(tmpnm);
		while(*tmp1 != 'd') {
			if((*tmp2++ = *tmp1++) == '\0')
				return;
		}
		tmp1++;
		strcpy(tmp2, __DEVSTR2);
		tmp2 += strlen(__DEVSTR2);
		while (*tmp1 != 's') {
			if((*tmp2++ = *tmp1++) == '\0')
				return;
		}
		tmp1++;
		strcpy(tmp2, __DEVSTR3);
		tmp2 += strlen(__DEVSTR3);
		while (*tmp2++ = *tmp1++)
			;
		tmp2--;
		*tmp2++ = '/';
		strcpy(tmp2, EDGE_LBL_BLOCK);
		strcpy(devnm,tmpnm);
		
	}
}
