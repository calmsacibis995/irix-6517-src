/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/************************************************************************
 *                                                                      *
 *      scsi_self.c - scsi self test					*
 *		- run self test/ send diag on all scsi devices		*
 *                                                                      *
 ************************************************************************/

#include <sys/types.h>

#include <sys/param.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/scsi.h>
#include <sys/dkio.h>
#include <arcs/io.h>
#include <sys/newscsi.h>
#include <sys/wd95a.h>
#include <sys/wd95a_struct.h>
#include <sys/invent.h>
#include <ide_msg.h>
#include <everr_hints.h>
#include <io4_tdefs.h>
#include <ide_wd95.h>
#include <ide_s1chip.h>
#include <setjmp.h>

extern wd95ctlrinfo_t  *wd95dev;
scsi_target_info_t *wd95info();
static int wd95loc[2];

#ifndef TFP
int 
selftest(caddr_t swin, int ioa_num, int cont_num, uint slot, int contn)
#else
int 
selftest(paddr_t swin, int ioa_num, int cont_num, uint slot, int contn)
#endif
{
	register uint dummy;
	register int fail = 0;
	uint ctrl_off;
	int adap, i, result, targ, tmp;
	scsi_target_info_t realinfo;
	scsi_target_info_t *info;
	char *inv;
	wd95ctlrinfo_t  *ci, *debugci;
	int lun = 0;
	int nodevice = 1;
	caddr_t dp;
	char id[26];            /* 8 + ' ' + 16 + null */
	char *extra;
	char name[SCSI_DEVICE_NAME_SIZE];
	int fd;
	int gotname = 0;

	wd95loc[0] = slot;
	wd95loc[1] = ioa_num;

	info = &realinfo;
	
	for (i = 0; i < 26; i++)
		id[i] = 'b';

	/*
	 * offset of the selected controller's regs in the s1 chip space
	 */
	ctrl_off = S1_CTRL_BASE + (cont_num * S1_CTRL_OFFSET);
	msg_printf(DBG,"ctrl_off is 0x%x\n", ctrl_off);
	ci = &wd95dev[contn];
	dp = ci->ha_addr;
	ci->present = 1;
	debugci = &wd95dev[contn];
	msg_printf(DBG,"debugci->present is %d\n", debugci->present);

	/* determine if there are devices on this channel  -from scsiinstall */
	msg_printf(INFO,"\n\t\tDevices connected to controller %d: \n", contn);
	msg_printf(DBG,"contn is %d and cont_num is %d\n", contn,cont_num);
	tmp = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"csr loc a is 0x%x\n", tmp);
	for (targ = 1; targ < WD95_LUPC; targ++) {
               	msg_printf(DBG,"debug selftest, targ: %d\n", targ);
               	if ((info = wd95info(contn,targ,lun)) == NULL) {
			tmp = S1_GET_REG(swin, S1_STATUS_CMD);
        		msg_printf(DBG,"csr loc aa is 0x%x, t:%d, c:%d\n",
					tmp, targ, contn);	
			SET_DMA_REG(dp, S1_DMA_FLUSH, 0);
			DELAY(200);
			SET_DMA_REG(dp, S1_DMA_CHAN_RESET, 0);
			tmp = S1_GET_REG(swin, S1_STATUS_CMD);
                        msg_printf(DBG,"csr loc ab is 0x%x, t:%d, c:%d\n",
                                        tmp, targ, contn);
                       	continue;
		}
               	inv = info->si_inq;
                /* ARCS Identifier is "Vendor Product" */
                strncpy(id,inv+8,8); id[8] = '\0';
		msg_printf(DBG,"id1 is %s\n", id);
                for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
                strcat(id," ");
                strncat(id,inv+16,16); id[25] = '\0';
		msg_printf(DBG,"id2 is %s\n", id);
                for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
		msg_printf(DBG,"id3 is %s\n", id);
		msg_printf(VRB,"inv[0] is %d\n", inv[0]);

		nodevice = 0;
		tmp = S1_GET_REG(swin, S1_STATUS_CMD);
		msg_printf(DBG,"csr loc b is 0x%x\n", tmp);
               	msg_printf(DBG,"debug: selftest, ioa_num: %d\n", ioa_num);
		msg_printf(DBG,"inv is %s\n", inv);
		switch(inv[0]) {
		   case 0: 
			/* sometimes CD ROM returns status of disk */
                        if ((strncmp(id, "TOSHIBA CD-ROM", 14) == 0) ||
                                (strncmp(id, "SONY    CD", 10) == 0) ||
                                (strncmp(id, "LMS     CM ", 11) == 0) )
				msg_printf(INFO, "\t\t   CD ROM, ");
			else
				msg_printf(INFO,"\t\t   Hard disk, ");
			msg_printf(INFO,"unit: %d ", targ);
			msg_printf(INFO,"- %s\n", id);
			result = senddiag(contn, targ, lun);
			break;
		   case 1: msg_printf(INFO,"\t\t   SCSI Tape, ");
			msg_printf(INFO,"unit: %d ", targ);
			msg_printf(INFO,"- %s\n", id);
			result = senddiag(contn, targ, lun);
			break;
		   default: 
              		if ((inv[0]&0x1f) == INV_CDROM) {
                       		msg_printf(INFO, "\t\t   CD ROM, ");
				msg_printf(INFO,"unit: %d ", targ);
				msg_printf(INFO,"- %s\n", id);
				result = senddiag(contn, targ, lun);
			}
			else {
			  	msg_printf(INFO,"\t\t   Device type %u ",
					inv[0]&0x1f);
			  	msg_printf(INFO,"unit: %d ", targ);
				msg_printf(INFO,"- %s\n", id);
				result = senddiag(contn, targ, lun);
			}
			break;
		}
		/* output results */
		fail += result;
		report_scsi_results(result, wd95loc);	
		tmp = S1_GET_REG(swin, S1_STATUS_CMD);
		msg_printf(DBG,"csr loc c is 0x%x\n", tmp);
	} /* for targ */
	tmp = S1_GET_REG(swin, S1_STATUS_CMD);
        msg_printf(DBG,"csr loc d is 0x%x\n", tmp);
	if (nodevice) {
		msg_printf(INFO,"\t\t   None\n");
		if (!fail)
		    fail = TEST_SKIPPED;
	}

	msg_printf(DBG,"debug: loc 2\n");

	return(fail);
}

int scsi_self(int argc, char**argv)
{
	int fail = 0;

    	msg_printf(INFO, "scsi_self -  SCSI devices senddiag command test\n");
	scsi_setup();
        fail = test_scsi(argc, argv, selftest);
	msg_printf(DBG,"debug: loc 3\n");
	return(fail);
}

