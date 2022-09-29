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
 *      scsi_dmaxfer.c - scsi self test					*
 *		- do dma transfers on scsi disks			*
 *                                                                      *
 ************************************************************************/

#include <sys/types.h>
#include <sys/fcntl.h>
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
extern char *atob();

int rdonly = 1;
int dkpart = PTNUM_FSSWAP; /* default to disk partition 1 */
int force = 0;

scsi_target_info_t *wd95info();

static int wd95loc[2];
static char buffer[DFLT_BLK_SIZE], buf2[DFLT_BLK_SIZE];
static scsi_target_info_t *info;
static scsi_target_info_t realinfo;
static char *inv;

extern uint skstack;

int
xfer(int ctlr, int targ, int lun, int slot, int adap, int contn)
{
	int fd, i, j, result, xferfd;
	char device[12];
	uint count, firstblock;
	IDESTRBUF s;
	char answ[4];
	char name[SCSI_DEVICE_NAME_SIZE];
	int fail = 0;
	
	wd95loc[0] = slot;
	wd95loc[1] = adap;

	clearvh();

	if (rdonly)
		fd = gopen(contn,targ,PTNUM_VOLUME,SCSI_NAME,OpenReadOnly);
	else
		fd = gopen(contn,targ,PTNUM_VOLUME,SCSI_NAME,OpenReadWrite);
	if (fd < 0) {
		return(1);
	}
	else {
		xferfd = fd;
		sprintf(s.c,"scsi(%d)disk(%d)part(%d)",contn,targ,PTNUM_VOLUME);
		msg_printf(DBG,"%s opened!, xferfd: %d\n", s.c, xferfd);
		if (!scsi_dname(name, sizeof (name), fd))
			msg_printf(DBG,"Scsi drive type == %s\n", name);
	}
	
	if (init_label(fd)) {
		return (1);
	}

	/*
	 * if no space on partition to test, it should be a skip, not a fail
	 */
	if (firstblock_ofpart(dkpart, force, &firstblock) == -1) {
		msg_printf(INFO, "Skipping partition %d\n", dkpart);
		return (TEST_SKIPPED);
	}

        for ( count = 0; count < DFLT_BLK_NUM; count++ ) {
                /* zero out read buffer */
                bzero(buffer,DFLT_BLK_SIZE);

                /* read into specified buffer */
		msg_printf(INFO, "Reading from partition %d\n", dkpart);
		msg_printf(DBG,"firstblock is %d\n", firstblock);
		result = gread(firstblock,buffer,DFLT_BLK_NUM,slot,adap,fd);
		if (result) {
			/* err_msgs in gread definition */
                        gclose(xferfd);
			fail = 1;
                        return(fail);
                }
        } 
	msg_printf(INFO,"Read completed successfully\n");
	if (!rdonly) { /* do writes */
		msg_printf(SUM,"About to write to partition %d\n", dkpart);
questloop:
		if (!force) {
		   msg_printf(SUM,"Do you want to continue? (y=1, n=0): ");
		   gets(answ);
		   if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
			if (!strcmp(answ, "0")) {
				msg_printf(SUM,"Skipping the write...\n");
				return (fail);
			}
		   }
		   else {
			msg_printf(SUM,"Please enter a 1 or a 0\n");
			goto questloop;
		   }
questloop2:
		   msg_printf(SUM,"Are you sure? (y=1, n=0): ");
		   gets(answ);
                   if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
                        if (!strcmp(answ, "0")) {
				msg_printf(SUM,"Skipping the write...\n");
                                return (fail);
			}
                   }
                   else {
                        msg_printf(SUM,"Please enter a 1 or a 0\n");
                        goto questloop2;
                   }
		}
		for (j = 0; j < 2; j++) {
		for ( count = 0; count < DFLT_BLK_NUM; count++ ) {
                    /* zero out write buffer */
                    /* bzero(buffer,DFLT_BLK_SIZE); */
		    for (i = 0; i < DFLT_BLK_SIZE; i++)
			if (j == 0)
				buffer[i] = 0xaa;
			else
				buffer[i] = 0x55;

                    /* write from buffer to device */
		    msg_printf(INFO,"Writing 0x%x's to the scsi device\n",
			buffer[0]);
		    msg_printf(DBG,"firstblock is %d\n", firstblock);
                    if (gwrite(firstblock, buffer, DFLT_BLK_NUM, slot, adap,
				fd) ) {
                        	/* err_msgs in gwrite definition */
				msg_printf(DBG,
					"xfer loc 2, xferfd: %d\n", xferfd);
                        	gclose(xferfd);
				fail = 1;
                        	return(fail);
                	}
        	}
		msg_printf(INFO,"Write completed successfully\n");	
		/* read from disk to verify data */
		msg_printf(INFO, "Verifying data (0x%x's)...\n", buffer[0]);
		for ( count = 0; count < DFLT_BLK_NUM; count++ ) {
                	/* zero out read buffer */
                	/* bzero(buf2,DFLT_BLK_SIZE); */
			for (i = 0; i < DFLT_BLK_SIZE; i++)
				if (j == 0)
					buf2[i] = 0x55;
				else
					buf2[i] = 0xaa;

                	/* read into specified buffer */
                	msg_printf(INFO,"Reading from partition %d\n", dkpart);
		        msg_printf(DBG,"firstblock is %d\n", firstblock);
                	if (gread(firstblock,buf2, DFLT_BLK_NUM,slot,adap,
					fd)) {
                        	/* err_msgs in gread definition */
				msg_printf(DBG,
					"xferfd at loc 3 is %d\n", xferfd);
                        	gclose(xferfd);
				fail = 1;
                        	return(fail);
                	}
        	}
		/* comparing data */
		if(bcmp(buf2, buffer, DFLT_BLK_SIZE)) {
			fail = 1;
			err_msg(WD95_DMA_WRDATA, wd95loc);
questloop3:
			if (!force) {
				msg_printf(SUM,
				   "Do you want to see what was written?"); 
				msg_printf(SUM," (y=1, n=0): ");
				gets(answ);
                   		if (strcmp(answ, "1") && strcmp(answ, "0")) {
				   msg_printf(SUM,"Please enter 1 or 0\n");
				   goto questloop3;
				}
			}
			if (force || !strcmp(answ, "1"))
				puts(buffer);
questloop4:
			if (!force) {
				msg_printf(SUM,
				   "Do you want to see what was read?");
				msg_printf(SUM," (y=1, n=0): ");
				gets(answ);
                   		if (strcmp(answ, "1") && strcmp(answ, "0")) {
				   msg_printf(SUM,"Please enter 1 or 0\n");
				   goto questloop4;
				}
			}
			if (force || !strcmp(answ, "1"))
				puts(buf2);
		}
		else
			msg_printf(INFO,"Verifying completed...OK\n\n");
		} /* j < 2 */
	} /* if !rdonly */
	msg_printf(DBG,"ALL DONE, xferfd is %d\n", xferfd);
        gclose(xferfd);  /* close all open disks */
	msg_printf(DBG,"xfer leaving, targ is %d\n", targ);
	return (fail);
} /* xfer */


int 
dmaxfer(caddr_t swin, int ioa_num, int cont_num, uint slot, int contn)
{
	register uint dummy;
	register int fail = 0;
	uint ctrl_off;
	int adap, i, result, targ, tmp;
	/* scsi_target_info_t realinfo; */
	/* scsi_target_info_t *info;
	char *inv; */
	wd95ctlrinfo_t  *ci, *debugci;
	int lun = 0;
	int nodevice = 1;
	int tested = 0;
	caddr_t dp;
	char id[26];            /* 8 + ' ' + 16 + null */
	char *extra;

	info = &realinfo;

	/* revertcdrom(); */
	
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
	msg_printf(DBG,"cont_num is %d and contn is %d\n", cont_num, contn);
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
		/* tmp = S1_GET_REG(swin, S1_STATUS_CMD); 
		msg_printf(DBG,"csr loc b is 0x%x\n", tmp); */
               	msg_printf(DBG,"debug: selftest, ioa_num: %d\n", ioa_num);
		msg_printf(DBG,"inv is %s\n", inv);
		switch(inv[0]) {
		   case 0: 
			/* sometimes CD ROM returns status of disk */
			if ((strncmp(id, "TOSHIBA CD-ROM", 14) == 0) ||
				(strncmp(id, "SONY    CD", 10) == 0) ||
				(strncmp(id, "LMS     CM ", 11) == 0)) {
				msg_printf(INFO, "\t\t   CD ROM, ");
                                msg_printf(INFO,"unit: %d ", targ);
                                msg_printf(INFO,"- %s\n", id);
                                msg_printf(INFO,
                           "Test is for disks only. Skipping CD ROM.\n");
                        }
			else { /* normal disk */
			
			msg_printf(INFO,"\t\t   Hard disk, ");
			msg_printf(INFO,"unit: %d - %s\n",
				targ, id);
			msg_printf(DBG,"targ before is %d\n", targ);
			result = xfer(cont_num,targ, lun, slot, ioa_num, contn);
			msg_printf(DBG,"targ after is %d\n", targ);
			msg_printf(DBG,"dmaxfer loc 1, result: %d\n",result);
			/* clean up dmas */
			/* SET_DMA_REG(dp, S1_DMA_FLUSH, 0);
			DELAY(200);
			SET_DMA_REG(dp, S1_DMA_CHAN_RESET, 0); */
			if (result != TEST_SKIPPED)
			    tested = 1;
			} /* else normal disk */
			break;
		   case 1: msg_printf(INFO,"\t\t   SCSI Tape, ");
			msg_printf(INFO,"unit: %d - %s\n",
				targ, id);
			msg_printf(INFO,
			   "Test is for disks only. Skipping tape drive.\n");
			break;
		   default: 
              		if ((inv[0]&0x1f) == INV_CDROM) {
                       		msg_printf(INFO, "\t\t   CD ROM, ");
				msg_printf(INFO,"unit: %d ",
					targ);
				msg_printf(INFO,"- %s\n", id);
				msg_printf(INFO,
			   "Test is for disks only. Skipping CD ROM.\n");
			}
			else {
			  	msg_printf(INFO,"\t\t   Device type %u ",
					inv[0]&0x1f);
			  	msg_printf(INFO,"unit: %d ",targ);
				msg_printf(INFO,"- %s\n", id);
				msg_printf(INFO,
			   "Test is for disks only. Skipping this device.\n");
			}
			break;
		}
		/* output results */
		if (!fail || (fail == TEST_SKIPPED))
		    fail = result;
		else if (result != TEST_SKIPPED)
		    fail += result;
		msg_printf(DBG,"dmaxfer loc 2\n");
		/* tmp = S1_GET_REG(swin, S1_STATUS_CMD);
		msg_printf(DBG,"csr loc c is 0x%x\n", tmp); */
		msg_printf(DBG,"dmaxfer loc 3, targ: %d\n", targ);
	} /* for targ */
	/* tmp = S1_GET_REG(swin, S1_STATUS_CMD);
        msg_printf(DBG,"csr loc d is 0x%x\n", tmp); */
	if (nodevice) {
		msg_printf(INFO,"\t\t   None\n");
	       if (!fail)
		    fail = TEST_SKIPPED;
	}

	if (!tested)
	   msg_printf(INFO,"\nNo SCSI disks tested on controller %d\n",contn);

	msg_printf(DBG,"debug: loc 2\n");

	return(fail);
}

int scsi_dmaxfer(int argc, char**argv)
{
	int fail = 0;
	
	rdonly = 1;
	dkpart = PTNUM_FSSWAP;
	force = 0;

    	msg_printf(INFO, "scsi_dmaxfer -  scsi disk dma transfer test\n");
	while (--argc > 0) {
		if (**++argv == '-') {
			msg_printf(DBG,"argv is -\n");
        		msg_printf(DBG,"*argv[1] is %c\n", (*argv)[1]);
			msg_printf(DBG,"argc is %d\n", argc);
			switch ((*argv)[1]) {
			   case 'f':
				force = 1; break;
			   case 'p':
				if (argc-- <= 1) {
                        	   msg_printf(SUM,
				      "The -p switch requires a number\n");
                       		    return (1);
                    		}
				msg_printf(DBG,"argc is %d\n", argc);
                    		if (*atob(*++argv, &dkpart)) {
                       		   msg_printf(SUM,
				      "Unable to understand the -p number\n");
                        	   msg_printf(SUM,
				      "The -p switch requires a number\n");
                        	   return (1);
                    		}
                    		break;
			    case 'w':
				rdonly = 0; break;
			}
		}
	}
	msg_printf(DBG,"force: %d, dkpart: %d, rdonly: %d\n", 
		force,dkpart,rdonly);
	scsi_setup();
        fail = test_scsi(argc, argv, dmaxfer);
	return(fail);
}

