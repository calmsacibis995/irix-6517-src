/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/sbd.h>
#include "sys/gr2hw.h"
#include "ide_msg.h"
#include "libsc.h"
#include "uif.h"
#include "gr2.h"

#define HQ2VERSION		1
#define NUMGE			2
#define FINFLAG1		3
#define FINFLAG2		4
#define FINFLAG3		5
#define DMASYNC			6
#define FIFO_FULL_TIMEOUT	7
#define FIFO_EMPTY_TIMEOUT	8
#define FIFO_FULL		9
#define FIFO_EMPTY		10
#define GE7UCODE_LOAD		11
#define ATTR_JUMP		12
#define GEDMA			13
#define GE7PC_LOAD_UCODE	14
#define INTR_BIT		15
#define UNSTALL			16
#define REFRESH			17
#define HQ_GE7_PC		18
#define MYSTERY 		19
#define DMA_SYNC_VERT_INTR	20
#define DMA_SYNC_VID_SYNC	21
#define DMA_SYNC_CLEAR		22
#define DMA_SYNC_SET		23

extern struct gr2_hw *base; 
void
WrHQ2RegMenu(void)
{
	msg_printf(SUM,"Writable HQ2 internal registers:\n\n");
	msg_printf(SUM,"--------------------------------\n");
	msg_printf(SUM,"Num GE7= %d\n",NUMGE);
	msg_printf(SUM,"Finish Flag1	= %d		Finish Flag2 = %d\n",FINFLAG1,FINFLAG2);
	msg_printf(SUM,"Finish Flag3    = %d\n",FINFLAG3);
	msg_printf(SUM,"FIFO FULL timeout = %d		FIFO EMPTY timeout = %d\n",FIFO_FULL_TIMEOUT, FIFO_EMPTY_TIMEOUT);
	msg_printf(SUM,"FIFO FULL = %d			FIFO EMPTY = %d\n",FIFO_FULL, FIFO_EMPTY);
	msg_printf(SUM,"GE7 ucode load = %d		pc for loading ge7 ucode = %d\n",GE7UCODE_LOAD,GE7PC_LOAD_UCODE);
	msg_printf(SUM,"Attribute Jump Registers = %d	GE DMA = %d\n",ATTR_JUMP, GEDMA);
	msg_printf(SUM,"Intr bit (cleared by host) = %d\n",INTR_BIT);
	msg_printf(SUM,"Unstall HQ/GE = %d		Refresh register = %d\n",UNSTALL,REFRESH);
	msg_printf(SUM,"DMA sync source (Don't write to DMA sync thru HQ version register): \n");
	msg_printf(SUM,"	- Vertical Interrupt 	= %d\n",DMA_SYNC_VERT_INTR);
	msg_printf(SUM,"	- Video Sync		= %d\n",DMA_SYNC_VID_SYNC);
	msg_printf(SUM,"	- DMA Sync Source Clear	= %d\n",DMA_SYNC_CLEAR);
	msg_printf(SUM,"	- DMA Sync Source Set 	= %d\n",DMA_SYNC_SET);
}

/*
 *
 *	This is a tool to allow writing to the HQ2 internal registers.
 */
gr2_wrHQ2reg(int argc, char **argv) 
{
    int val;
    long regindex,attrindex;

	
    if (argc < 2)
    {
	WrHQ2RegMenu();
	msg_printf(SUM,"usage: %s HQ2_register_choice hex_value\n",argv[0]);  
	return -1;
    }

    regindex = atoi(*(++argv));

    switch(regindex) {
	case NUMGE:
    		atob(*(++argv), &val);
    		base->hq.numge= val;
    		msg_printf(SUM,"Writing NUMGE with %d\n", val);
		break;
	case FINFLAG1:
    		atob(*(++argv), &val);
    		base->hq.fin1= val;
    		msg_printf(SUM,"Writing Finish Flag 1 with %d\n", val);
		break;
	case FINFLAG2:
    		atob(*(++argv), &val);
    		base->hq.fin2= val;
    		msg_printf(SUM,"Writing Finish Flag 2 with %d\n", val);
		break;
	case FINFLAG3:
    		atob(*(++argv), &val);
    		base->hq.fin3= val;
    		msg_printf(SUM,"Writing Finish Flag 3 with %d\n", val);
		break;
	case FIFO_FULL_TIMEOUT:
    		atob(*(++argv), &val);
    		base->hq.fifo_full_timeout= val;
    		msg_printf(SUM,"Writing FIFO Full Timeout with %d\n", val);
		break;
	case FIFO_EMPTY_TIMEOUT:
    		atob(*(++argv), &val);
    		base->hq.fifo_empty_timeout= val;
    		msg_printf(SUM,"Writing FIFO Empty Timeout with %d\n", val);
		break;
	case FIFO_FULL:
    		atob(*(++argv), &val);
    		base->hq.fifo_full = val;
    		msg_printf(SUM,"Writing FIFO Full with %d\n", val);
		break;
	case FIFO_EMPTY:
    		atob(*(++argv), &val);
    		base->hq.fifo_empty = val;
    		msg_printf(SUM,"Writing FIFO Empty with %d\n", val);
		break;
	case GE7UCODE_LOAD:
    		atob(*(++argv), &val);
    		base->hq.ge7loaducode = val;
    		msg_printf(SUM,"Writing GE7 Ucode Load with %#08x\n", val);
		break;
	case ATTR_JUMP:
    		if (argc < 4) { 
			msg_printf(SUM,"If writing to Attr Jump Reg, also give reg# (0-15) as the last argument\n");
			return -1;	
		}
    		atob(*(++argv), &val);
		attrindex = atoi(*(++argv));
    		base->hq.attrjmp[attrindex]= val;
    		msg_printf(SUM,"Writing Attr Jump Register #%d with %#08x\n", attrindex,val);
		break;
	case GEDMA:
    		atob(*(++argv), &val);
    		base->hq.gedma = val;
    		msg_printf(SUM,"Writing GE DMA with %#08x\n", val);
		break;
	case GE7PC_LOAD_UCODE:
    		atob(*(++argv), &val);
    		base->hq.gepc= val;
    		msg_printf(SUM,"Writing GE7 pc (used for loading GE7 ucode) with  %#08x\n", val);
		break;
	case UNSTALL:
    		atob(*(++argv), &val);
    		base->hq.unstall= val;
    		msg_printf(SUM,"Writing Unstall HQ/GE register  with  %d\n", val);
		break;
	case INTR_BIT:
    		atob(*(++argv), &val);
    		base->hq.intr = val;
    		msg_printf(SUM,"Writing Interrupt bit register  with  %d\n", val);
		break;
	case REFRESH:
    		atob(*(++argv), &val);
    		base->hq.refresh = val;
    		msg_printf(SUM,"Writing refresh register  with  %#08x\n", val);
		break;
	case DMA_SYNC_VERT_INTR: /*Write only dma value*/
    		base->hq.dmasync = 0;
    		msg_printf(SUM,"Setting DMA SYNC source  vertical interrupt\n");
		break;
	case DMA_SYNC_VID_SYNC: /*Write only dma value*/
    		base->hq.dmasync = 1;
    		msg_printf(SUM,"Setting DMA SYNC source video sync\n");
		break;
	case DMA_SYNC_CLEAR: /*Write only dma value*/
    		base->hq.dmasync = 2;
    		msg_printf(SUM,"Clearing DMA SYNC source clear\n");
		break;
	case DMA_SYNC_SET: /*Write only dma value*/
    		base->hq.dmasync = 3;
    		msg_printf(SUM,"Setting DMA SYNC source\n");
		break;
    }

	return 0;
}


void
RdHQ2RegMenu(void)
{
	msg_printf(SUM,"**Readable HQ2 internal registers:\n");
	msg_printf(SUM,"----------------------------------\n");
	msg_printf(SUM,"HQ2 version reg = %d\n",HQ2VERSION);
	msg_printf(SUM,"GE7 ucode load 	= %d\n",GE7UCODE_LOAD);	
	msg_printf(SUM,"HQ2_GE7 pc (Currently, not readable) = %d\n",HQ_GE7_PC);
	msg_printf(SUM,"GE DMA 		= %d\n",GEDMA);
	msg_printf(SUM,"MYSTERY ADDRESS = %d\n",MYSTERY);
}


gr2_rdHQ2reg(int argc, char **argv) 
{
    unsigned long rdval,regindex,dma_sync_src;
    if (argc < 2)
    {
	RdHQ2RegMenu();
	msg_printf(SUM,"usage: %s 	HQ2_register_choice\n",argv[0]);  
	return 1;
    }

    regindex = atoi(*(++argv));

    switch(regindex) {
	case HQ2VERSION:
    		rdval = base->hq.version;
    		msg_printf(SUM,"Reading HQ2 version register: %#08x\n", rdval);
    		msg_printf(SUM,"	finflag1 	: %d\n", (rdval & 0x4) >> 2);
    		msg_printf(SUM,"	finflag2 	: %d\n", (rdval & 0x2) >> 1);
    		msg_printf(SUM,"	finflag3 	: %d\n", rdval & 0x1);
		dma_sync_src = (rdval & 0x18) >> 3;
		if (dma_sync_src == 0)
    			msg_printf(SUM,"	dma_sync source	: Vertical interrupt\n");
		else if (dma_sync_src == 1)
    			msg_printf(SUM,"	dma_sync source	: Video sync\n");
		else if (dma_sync_src == 2)
    			msg_printf(SUM,"	dma_sync source	: Clear\n");
		else if (dma_sync_src == 3)
    			msg_printf(SUM,"	dma_sync source	: Set\n");
    		msg_printf(SUM,"	FIFO overflow 	: %d\n", (rdval & 0x20) >> 5);
    		msg_printf(SUM,"	FIFO count 	: %d\n", (rdval & 0x1fc0) >> 6); 
		msg_printf(SUM,"	FIFO low water 	: %d\n", (rdval & 0x2000) >> 13);
    		msg_printf(SUM,"	zero     	: %d\n", (rdval & 0xc000) >> 14);
    		msg_printf(SUM,"	ge_ctr   	: %d\n", (rdval & 0xf0000) >> 16);
    		msg_printf(SUM,"	ge_ptr   	: %d\n", (rdval &  0x700000)>> 20);
    		msg_printf(SUM,"	HQ2 version	: %d\n", (rdval & 0xff800000) >> 23);
		break;
	case GE7UCODE_LOAD:
    		rdval = base->hq.ge7loaducode;
    		msg_printf(SUM,"Reading GE7 Ucode Load: %#08x\n", rdval);
		break;
	case HQ_GE7_PC:
    		rdval = base->hq.hq_gepc;
    		msg_printf(SUM,"Currently, can't read - hw bug - HQ_GE7 PC: %#08x\n", rdval);
		break;
	case GEDMA:
    		rdval = base->hq.gedma;
    		msg_printf(SUM,"Reading GE DMA: %#08x\n", rdval);
		break;
	case MYSTERY:
    		rdval = base->hq.mystery;
    		msg_printf(SUM,"Reading MYSTERY ADDRESS: %#08x\n", rdval);
#if IP22
		if (rdval != 0xdeadbeef)
			return 1;
#endif
		break;
    }
return 0;
}


gr2_wrFIFO(int argc, char **argv) 
{
    long token, i,numdata;
    __psint_t data; 
    if (argc < 3)
    {
	msg_printf(SUM,"usage: %s TOKEN# hex_data0 <data1 ...etc.>\n",argv[0]);  
	return -1;
    }
	
	token = atoi(*(++argv));

	numdata = argc - 2;
	for (i=0;i < numdata;i++) {
		atob_ptr(*(++argv), &data);
		base->fifo[token] = (unsigned int) KDM_TO_PHYS(data);
	}

	return 0;
}


/*ARGSUSED*/
int
gr2_unstall(int argc, char **argv) 
{
	/* Set GE pc to 0 */
        base->hq.gepc = 0;

        /* Unstall HQ and GE */
        base->hq.unstall = 0;

        return 0;
}

/*ARGSUSED*/
int
gr2_inithq(int argc, char **argv) 
{
	int numge;
	/*Initialize HQ2 internal registers*/
        numge          = Gr2ProbeGEs(base);
	msg_printf(DBG,"NUMGE set to  %d. Use wrhqint cmd  to change hq internal reg values\n",numge);

	if (numge == 8)
		base->hq.numge = HQ2_FASTSHRAMCNT_4 | numge;
	else
        	base->hq.numge          	= numge;
        base->hq.refresh                = 0xf0;
        base->hq.fifo_full_timeout      = 10;
        base->hq.fifo_empty_timeout     = 40;
        base->hq.fifo_full              = 48;
        base->hq.fifo_empty             = 16;


	return 0;
}
