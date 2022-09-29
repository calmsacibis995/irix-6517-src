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
 *      scsilib.c - scsi lib routines					*
 *                                                                      *
 ************************************************************************/

#include <sys/types.h>
#include <arcs/hinv.h>
#include <sys/param.h>
#include <uif.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/evintr.h>
#include <sys/scsi.h>
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
#include <sys/immu.h>

extern s1chip_t s1chips[];
extern wd95ctlrinfo_t *wd95dev;
extern wd95unitinfo_t  *wd95unitinfo;
extern char wd95_devnum[];

scsi_request_t *kern_calloc();

#define kvpalloc(X, Y, Z) 0

#define TRY_WIDE        1
/* the following are for 40 Mhz clock */
#define SPW_FAST        ((0 << W95_SPW_SCLKA_SFT) | (0 << W95_SPW_SCLKN_SFT))
#define SPW_SLOW        ((2 << W95_SPW_SCLKA_SFT) | (2 << W95_SPW_SCLKN_SFT))
/* #define MAX_CHANS_IO4   10               /* max scsi buses per io4 board */

jmp_buf scsilib_buf;

u_char ide_dk_inquiry[] =       {SCSI_INQ_CMD,   0, 0, 0,    64, 0};
u_char ide_senddiag[] =      {SCSI_SEND_DIAG, 4, 0, 0,    0, 0};

int firsttime = 0;
#if 0
s1chip_t s1chip_array[MAX_S1_CHIPS];
#endif
s1chip_t *s1info;

static int wd95loc[2];
static int firstscsislot = -1;

extern char *skstack;
extern int wd95inited; /* disable printing of selection timeouts */

int s1infoprint(int s1num)
{
	msg_printf(DBG,"&s1chips[%d]: 0x%x, s1info: 0x%x\n",
		s1num, &s1chips[s1num], s1info);
	msg_printf(DBG,"s1_slot: %d, s1_adap: %d\n",
		s1info->s1_slot, s1info->s1_adap);
	msg_printf(DBG,"s1_win: 0x%x, s1_adap_type: %d\n",
		s1info->s1_win, s1info->s1_adap_type);
	msg_printf(DBG,"s1_intr_lvl: 0x%x, s1_swin: 0x%x\n",
		s1info->s1_intr_lvl, s1info->s1_swin);
	msg_printf(DBG,"s1_num_chans: %d, s1_chan: %d\n",
		s1info->s1_num_chans, s1info->s1_chan);
}

int scsi_setup()
{
	int slot, adap, atype;
	int set_s1_num = 0;
	COMPONENT *c;
	unchar swindow;
#ifndef TFP
	caddr_t sswin;
#else
	paddr_t sswin;
#endif

	if (firsttime) {
	  firsttime = 0;
	  /* set up sw structures */
	  for (slot = EV_MAX_SLOTS; slot > 0; slot--) 
	   if (board_type(slot) == EVTYPE_IO4)
	     for (adap = 1; adap < IO4_MAX_PADAPS; adap++) {
	   	atype = adap_type(slot, adap);
		if (atype == IO4_ADAP_SCSI) {
			swindow = io4_window(slot);
                        /* s1info = &s1chip_array[set_s1_num]; */
                        s1info = &s1chips[set_s1_num];
                        s1info->s1_slot = slot;
                        s1info->s1_adap = adap;
                        s1info->s1_win = swindow;
#ifndef TFP
                        s1info->s1_swin=sswin=(caddr_t)SWIN_BASE(swindow,adap);
#else
                        s1info->s1_swin=sswin=(paddr_t)SWIN_BASE(swindow,adap);
#endif
			s1infoprint(set_s1_num);
                        msg_printf(DBG,"window is 0x%x\n", swindow);
                        msg_printf(DBG,"atype is scsi, adap is %d\n", adap);
                        msg_printf(DBG,"set_s1_num is %d\n", set_s1_num);
                        msg_printf(DBG,"test loc 1\n");
			msg_printf(DBG,"95A type is: %d\n", S1_H_ADAP_95A);
                        /* if it uses 95a controllers */
                        s1_find_type(s1info);
			msg_printf(DBG,"ran s1_find\n");
                        msg_printf(DBG,"adap_type is %d, 95A: %d\n",
                                s1info->s1_adap_type, S1_H_ADAP_95A);
#if 0
			s1_init(slot, adap, swindow);
#endif
			/* s1_intr_setup(sswin, set_s1_num); */
			set_s1_num++;
		} /*  if scsi adapter */
	     } /* for adap */
#if 0
	  scsi_init();
	  scsiedtinit(c);
#endif
	} /* firsttiime */
} /* scsi_setup */

int test_scsi(int argg, char**arvv, int (*test_it)()) 
{
    	int cont, contn, slot, adap, atype, num_chans, val;
	int fail = TEST_SKIPPED;
    	unchar window;
    	int my_s1_num = 0;
#ifndef TFP
    	caddr_t ha, swin;
#else
    	paddr_t ha, swin;
#endif
	uint adap_arr, dummy;
	int i;
	int found_s1 = 0;
	int found_95 = 0;
	int slotmult = 0;
	int master_io4 = 1;

	wd95inited = 0;	/* disable selection timeout msgs */
        adap_arr = adap_slots(IO4_ADAP_SCSI);
        /* setup error handling */
        for (slot = EV_MAX_SLOTS; slot > 0; slot--)
           if (board_type(slot) == EVTYPE_IO4)
                for (adap = 1; adap < IO4_MAX_PADAPS; adap++)
                        if ((atype = adap_type(slot,adap)) == IO4_ADAP_SCSI) {
                                setup_err_intr(slot, adap);
                                io_err_clear(slot, adap_arr);
				msg_printf(DBG,"loc 1\n");
                        }

        if (val = setjmp(scsilib_buf)) {
                if (val == 1){
                        msg_printf(SUM, 
			  "Unexpected exception during scsi test\n");
                        show_fault();
                }
        	io_err_log(slot, adap_arr) ;
        	io_err_show(slot, adap_arr);
        	fail = 1;
        	goto SLOTFAILED;
        }
        else {

	set_nofault(scsilib_buf);

	adap_arr = adap_slots(IO4_ADAP_SCSI);

    	if (io4_select (1, argg, arvv))
        	return(1);

	/* begin testing */
	my_s1_num = 0;
    	/*
    	 * iterate through all io boards in the system
    	 */
    	for (slot = EV_MAX_SLOTS; slot > 0; slot--) {

          /*
           * cheat - if slot was given on command line, use it
           */
          if (io4_tslot)
            slot = io4_tslot;

          if (board_type(slot) == EVTYPE_IO4) {
            /*
             * iterate through all io adapters in the system
             */
            for (adap = 1; adap < IO4_MAX_PADAPS; adap++) {
                /*
                 * cheat - if adapter was given on command line, use it
                 */
                if (io4_tadap)
                    adap = io4_tadap;
		msg_printf(DBG,"slot is %d, adap is %d\n", slot, adap);

                /*
                 * get the current adapter type
                 */
                atype = adap_type(slot, adap);

                if (atype == IO4_ADAP_SCSI) {
			if (firstscsislot == -1)
				firstscsislot = slot;
			wd95loc[0] = slot;
			wd95loc[1] = adap;
			found_s1 = 1;
			/* insure scsi is 16-bit mode */
			/* S1_PUT_REG(swin, S1_STATUS_CMD, 
				(S1_SC_DATA_16 | 1)); */
			msg_printf(DBG,"loc 4\n");
    			/* s1info = &s1chip_array[my_s1_num]; */
    			s1info = &s1chips[my_s1_num]; 
			s1infoprint(my_s1_num);
			msg_printf(DBG,"my_s1_num is %d\n", my_s1_num);
			msg_printf(DBG,"95A type is: %d\n", S1_H_ADAP_95A);
			my_s1_num++;
			num_chans = ((adap == S1_ON_MOTHERBOARD) ? 2 : 3);
                        msg_printf(DBG,"num_chans is %d\n", num_chans);
			swin = s1info->s1_swin;
			cont = 0;
			if (firstscsislot == slot) /* first io4 slot */
				slotmult = 0;
			else 
				slotmult = slot;
			switch(adap) { /* set up first controller number */
			   case S1_ON_MOTHERBOARD: contn = 0; /* ctlr: 0, 1 */
				break;
			   case 5: contn = 2; break; /* ctlr: 2,3,4 */
			   case 6: contn = 5; break; /* cltr: 5,6,7 */
			}
			contn = contn + (slotmult*MAX_CHANS_IO4);
                        for (i = 0; i < num_chans; i++, contn++, cont++) {
			   uint sp;

			   /*sp = getsp();
			   msg_printf(DBG,"sp 0x%x, skstack 0x%x\n", sp,
				(uint)skstack);  */

#ifndef TFP
                           ha = (caddr_t)(swin + S1_CTRL(cont));
#else
                           ha = (paddr_t)(swin + S1_CTRL(cont));
#endif
			   msg_printf(DBG,"ha is 0x%x, cont: %d, swin: 0x%x\n", 
				ha, cont, swin);
                           if (wd95_present(ha)) { /* wd95A */
				found_95 = 1;
			   	msg_printf(DBG,"adap: %d\n", adap);
			   	window = io4_window(slot);
			  	msg_printf(INFO,"Testing IO4: slot %d, ",slot);
			  	msg_printf(INFO,"wd95a ctrlr %d, ",contn);
			  	msg_printf(INFO,"s1: adapter %d", adap);
			  	if (adap == S1_ON_MOTHERBOARD)
					msg_printf(INFO," (motherboard)\n");
			   	else
					msg_printf(INFO," (mez card)\n");
			    	dummy = (*test_it)(swin,adap,cont,slot,contn);
	    		   } /* if 95A */
			   else {
				fail++;
				err_msg(WD95_TALK, wd95loc, contn);
                                ide_wd95_present(ha, slot, adap);
				if (continue_on_error())
			    	dummy = (*test_it)(swin,adap,cont,slot,contn);
			   }	/* not a 95A */

			   /*
			    * whatever the controller, figure out the
			    * cumulative test status
			    */
			   switch (fail) {

				case TEST_SKIPPED:
				    fail = dummy;

				case TEST_PASSED:
				default:
				    if (dummy != TEST_SKIPPED)
				        fail += dummy;
			   }

			}	/* cont */
		}		/* if SCSI */
		else if (io4_tadap) {
		    fail = TEST_SKIPPED;
		}
                /*
                 * if adap was given on command line, we are done
                 */
                if (io4_tadap)
                    break;
            } /* for adap */

            /*
             * if slot was given on command line, we are done
             */
            if (io4_tslot)
                break;
          } /* if board_type(slot) = io4 */
    	} /* for slot ... */
	} /* setjmp */


    	/* Post processing */
    	for (slot = EV_MAX_SLOTS; slot > 0; slot--)
           if (board_type(slot) == EVTYPE_IO4)
                if (check_ioereg(slot, adap_arr)){
		     report_check_error(fail);
		     /* msg_printf(SUM,"WARNING: ");
                     if (!fail)
                        msg_printf(SUM,"Test passed, but got an ");
                     msg_printf(SUM,"Unexpected ERROR in a register\n");
                     msg_printf(SUM,"Run test again at report level 3 ");
	   	     msg_printf(SUM,"to see the error. It's printed before ");
	 	     msg_printf(SUM,"this message.\n");
                        /* longjmp(scsilib_buf, 2); */
                }


SLOTFAILED:
    	for (slot = EV_MAX_SLOTS; slot > 0; slot--)
        	if (board_type(slot) == EVTYPE_IO4)
                	io_err_clear(slot, adap_arr);
	clear_nofault();
	clear_IP();

	return(fail);
}

/*
 * report scsi results
 */
int
report_scsi_results(int result, int wd95loc[2])
{
	switch (result) {
        case SC_TIMEOUT: err_msg(WD95_SC_TIMEOUT, wd95loc); break;
        case SC_HARDERR: err_msg(WD95_SC_HARDERR, wd95loc); break;
        case SC_GOOD: msg_printf(DBG,"PASSED\n"); break;
        case SC_PARITY: err_msg(WD95_SC_PARITY, wd95loc); break;
        case SC_MEMERR: err_msg(WD95_SC_MEMERR, wd95loc); break;
        case SC_CMDTIME: err_msg(WD95_SC_CMDTIME, wd95loc); break;
        case SC_ALIGN: err_msg(WD95_SC_ALIGN, wd95loc); break;
        case SC_ATTN:  err_msg(WD95_SC_ATTN, wd95loc); break;
        case SC_REQUEST: err_msg(WD95_SC_REQ, wd95loc); break;
        default: err_msg(WD95_SC_UNKNOWN, wd95loc, result);
	}
}


/*
 * send diag command for scsi devices
 */
int
senddiag(int cont_num, int targ, int lun)
{
	u_char scsi[10];
	wd95ctlrinfo_t  *ci;
        wd95unitinfo_t  *ui;
	wd95luninfo_t   *li;
        scsi_request_t  *req;
	int result = 9;
	
	ci = &wd95dev[cont_num];
	ui = ci->unit[targ];
	li = ui->lun_info[lun];
	
	msg_printf(DBG,"senddiag\n");
	bcopy(ide_senddiag, scsi, sizeof(ide_senddiag));

	req = kern_calloc(1, sizeof(*req));
        req->sr_ctlr = cont_num;
        req->sr_target = targ;
        req->sr_lun = lun;
        req->sr_command = scsi;
        req->sr_cmdlen = sizeof(ide_senddiag);
        req->sr_flags = SRF_DIR_IN;
        req->sr_timeout = 10 * HZ;
        req->sr_buffer = li->tinfo.si_inq;
        req->sr_buflen = SCSI_INQUIRY_LEN;
        req->sr_sense = NULL;
        req->sr_notify = NULL;

        wd95command(req);

        result = req->sr_status;
	kern_free(req);
	if (result) {
		kern_free(li->tinfo.si_inq);
                kern_free(li->tinfo.si_sense);

                li->tinfo.si_inq = NULL;
                li->tinfo.si_sense = NULL;
        }
	msg_printf(DBG,"result is 0x%x\n", result);
	return (result);
}

/*
 *  inquiry command for scsi disk
 */
int
dsk_inquiry(int cont_num, int targ, int lun)
{
        u_char scsi[10];
        wd95ctlrinfo_t  *ci;
        wd95unitinfo_t  *ui;
	wd95luninfo_t 	*li;
        scsi_request_t  *req;
        int i, k;
	int result = 9;
	char vendor[10];
	char product[8];
	char model[4];
	u_char buffr[SCSI_INQUIRY_LEN];
	u_char *bufptr;

        ci = &wd95dev[cont_num];
        ui = ci->unit[targ];
	li = ui->lun_info[lun];

	for (i = 0; i < SCSI_INQUIRY_LEN; i++)
		buffr[i] = 'a';
	
	/* for (i = 0; i < SCSI_INQUIRY_LEN; i++)
		msg_printf(DBG,"%c:", buffr[i]); */

        msg_printf(DBG,"dsk_inquiry\n");
        bcopy(ide_dk_inquiry, scsi, sizeof(ide_dk_inquiry));

        req = kern_calloc(1, sizeof(*req));
        req->sr_ctlr = cont_num;
        req->sr_target = targ;
        req->sr_lun = lun;
        req->sr_command = scsi;
        req->sr_cmdlen = sizeof(ide_dk_inquiry);
        req->sr_flags = SRF_DIR_IN;
        req->sr_timeout = 10 * HZ;
        req->sr_buffer = bufptr;
        req->sr_buflen = SCSI_INQUIRY_LEN;
        req->sr_sense = NULL;
        req->sr_notify = NULL;

        wd95command(req);

        result = req->sr_status;
	kern_free(req);
        if (result) {
                kern_free(li->tinfo.si_inq);
                kern_free(li->tinfo.si_sense);

                li->tinfo.si_inq = NULL;
                li->tinfo.si_sense = NULL;
        }

        msg_printf(DBG,"result is 0x%x\n", result);
	msg_printf(DBG,"buffr is %s\n", buffr);
	msg_printf(DBG,"buffr is: \n");
	for (i = 8, k = 0; buffr[i] != ' ' && i < 16; i++, k++) {
		vendor[k] = buffr[i];
		vendor[k+1] = '\0' ;
	}
	for (i = 16, k = 0; buffr[i] != ' ' && i < 20; i++, k++) {
                product[k] = buffr[i];
                product[k+1] = '\0' ;
        }
	for (i = 20, k = 0; buffr[i] != ' ' && i < 23; i++, k++) {
                model[k] = buffr[i];
                model[k+1] = '\0' ;
        }
	/* msg_printf(INFO," (%s %s %s)\n", vendor, product, model); */

        return (result);
}

/*
 * copied from wd95.c's wd95busreset routine
 */
ide_wd95busreset(wd95ctlrinfo_t *ci)
{
       	volatile register caddr_t ibp = ci->ha_addr;
        volatile register caddr_t dp = ci->ha_addr;
        int val;
	
	msg_printf(DBG,"ide_wd95busreset called \n");

        s1dma_flush(ci->d_map);
        val = GET_WD_REG(ibp, WD95A_S_CTL) & W95_CTL_SETUP;
        SET_WD_REG(ibp, WD95A_S_CTL, W95_CTL_RSTO);
        DELAY(400);
        SET_WD_REG(ibp, WD95A_N_UEI, W95_UEI_RSTINTL);
        SET_WD_REG(ibp, WD95A_S_CTL, val);
}

