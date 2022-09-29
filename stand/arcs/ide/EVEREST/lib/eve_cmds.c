/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
  **************************************************************************/

#ifdef EVEREST	/* whole file */

#include <sys/types.h>
#include <sys/debug.h>
#include <setjmp.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include <sys/param.h>
#include <sys/iotlb.h>
#include <everr_hints.h>
#include <stdarg.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/wd95a.h>
#include <ide_msg.h>
#include <fault.h>


#ident	"arcs/ide/EVEREST/lib/eve_cmd.c:  $Revision: 1.5 $"

extern char 	*atob();

r4kereg(int argc, char **argv)
{
    printf("Status:0x%x\tCause:0x%x\t\n", get_SR(), get_cause());
}

jmp_buf		faultbuf;
dispconf(int argc, char **argv)
{
    int 	slot;
    unsigned 	regno;

    if (argc != 3){
	msg_printf(ERR,"dc:Invalid no of  parameters\n\tdc slot reg\n");
	return(1);
    }

    if ((atob(argv[1], &slot) == (char *)0) ||
	(atob(argv[2], &regno) == (char *)0)){
	msg_printf(ERR, "invalid slot/registerno\n");
	return(1);
    }

    msg_printf(INFO,"display config register slot:%d reg:%d\n",slot, regno);

    if (slot <=0 || slot >= EV_MAX_SLOTS){
	msg_printf(ERR,"Invalid slot no: %s\n", argv[1]);
	return(1);
    }

    if (setjmp(faultbuf)){
	msg_printf(ERR,"Invalid slot/register %s, %s\n",argv[1],argv[2]);
	nofault=0;
    }
    else{
	nofault = faultbuf;
	printf("reg%d:0x%llx\n", regno, EV_GETCONFIG_REG(slot,0,regno));
    }

    nofault = 0;
    return(0);
}

wrtconf(int argc, char **argv)
{
    int 	slot;
    unsigned	regno;
    evreg_t	val;


    if (argc != 4){
	msg_printf(ERR,"wc:Invalid no of parameters\n\tdc slot reg\n");
	return;
    }

    slot = atoi(argv[1]);
    regno = atoi(argv[2]);
    atobu_L(argv[3], &val);

    msg_printf(INFO,"Write config register slot:%d reg:%d val:0x%llx \n",
	slot, regno, val);

    if (setjmp(faultbuf)){
	msg_printf(ERR,"Invalid slot/register %s, %s\n",argv[1],argv[2]);
    }
    else{
	nofault = faultbuf;
	EV_SETCONFIG_REG(slot, 0, regno, val);
    }

    nofault = 0;
    return(0);
}
pod_reset()
{
    /* write to the KZRESET register */
    EV_SET_LOCAL(EV_KZRESET, 1);
}

iocachef()
{
    int 	i;

    for (i=0; i < EV_MAX_SLOTS; i++)
	if (board_type(i) == EVTYPE_IO4)
	    flush_iocache(i);
}

#endif
