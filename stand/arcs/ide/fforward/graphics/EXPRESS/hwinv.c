/************************************************************************** 
 *       	Copyright (C) 1991, Silicon Graphics, Inc.             	  * 
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#include "sys/types.h"
#include "sys/gr2hw.h"
#include "sys/sbd.h"
#include "ide_msg.h"
#include "libsc.h"
#include "uif.h"
#include "gr2.h"

int gr2_GetGr2BdVers(void);

int print_hinv(void);
int GBL_RE3Count;
int GBL_GRVers;
int GBL_ZbufferVers;
int GBL_Zconfig;
int GBL_MonType;
int GBL_SuperVidVers;
int GBL_VidBckEndVers;
int GBL_VMAVers;
int GBL_VMBVers;
int GBL_VMCVers;
int GBL_VM24;
int GBL_NumVMBoards;
int GBL_NUMGE;

#define GR2_GFXPG_PHYS 0x1F000000
struct gr2_hw *base = (struct gr2_hw *)PHYS_TO_K1(GR2_GFXPG_PHYS);

int gr2_setboard(int argc, char **argv)
{
	extern struct gr2_hw *expr[];
	int i;

	if (argc < 2) {
		printf("usage: %s 0|1\n", argv[0]);
		return -1;
	}
	atob(argv[1], &i);
	if ( i < 0 || i > 1 ) {
		msg_printf(ERR, "Illegal GR2 board number %d\n",i);
		return 1;
	}
	if (Gr2Probe(expr[i])) {
		base = expr[i];
		msg_printf(VRB, "GR2 board %d found.\n", i);
		return 1;
	} else {
		msg_printf(VRB, "GR2 board %d not found.\n", i);
		return 0;
	}
}

/* 
 *  rd_boardvers(value0 ... value3) - Reads board version register
 */

#define DBG_PRINT_VERS(version)	{					\
		/* Bits are the 1's complement of version number*/	\
        	switch(version) {					\
			case 0x3: /* Bits set to 1-1  */		\
       			msg_printf(DBG,"   Board is not installed.\n"); \
			break;						\
									\
                        case 0x2: /* Bits set to 1-0 */ 		\
			msg_printf(DBG,"  Version 1.0\n");		\
                        break;						\
									\
                        case 0x1: /* Bits set to 0-1 */			\
                        msg_printf(DBG,"  Version 2.0\n");		\
                        break;						\
									\
                        case 0x0: /* Bits set to 0-0 */			\
                        msg_printf(DBG,"  Version 3.0\n");		\
                        break;						\
									\
                        default:					\
                        msg_printf(DBG,"Version not yet defined in IDE.\n");\
			break;						\
               }							\
}									

int
Gr2ProbeGEs(struct gr2_hw *base)
{
	int i;

	GBL_NUMGE = 1;
	for (i=1; i<8; i++) {
		/* Probing whether we have 1 or 4 or 8 GE's installed */
		base->ge[i].ram0[0] = 0x1f1f1f1f;
		base->ge[i].ram0[31] = 0x2e2e2e2e;
		base->ge[i].ram0[63] = 0x3d3d3d3d;
		base->ge[i].ram0[95] = 0x4c4c4c4c;
		base->ge[i].ram0[127] = 0x5b5b5b5b;

		if (base->ge[i].ram0[0] == 0x1f1f1f1f &&
			base->ge[i].ram0[31] == 0x2e2e2e2e &&
			base->ge[i].ram0[63] == 0x3d3d3d3d &&
			base->ge[i].ram0[95] == 0x4c4c4c4c &&
			base->ge[i].ram0[127] == 0x5b5b5b5b) 
			GBL_NUMGE = i+1;
				
	}
    return GBL_NUMGE;
}

int
gr2_boardvers(void)
{
	/* Global status of board versions and configurations */
	int rd0,rd1,rd2,rd3;
	int TEST_ERR;

	TEST_ERR = 0;

	msg_printf(DBG,"Graphics alive?: Check Board Version register.\n");


/* Read words from different offsets in board version register, but only
	interested in low 6 bits */
	rd0 = base->bdvers.rd0;  
	msg_printf(DBG,"At offset 0 of version reg, reading value %#08x.\n", 
					rd0 & 0x3f);
	rd1 = base->bdvers.rd1;  
	msg_printf(DBG,"At offset 1 of version reg, reading value %#08x.\n", 
					rd1 & 0x3f);
	rd2 = base->bdvers.rd2;  
	msg_printf(DBG,"At offset 2 of version reg, reading value %#08x.\n", 
					rd2 & 0x3f);
	rd3 = base->bdvers.rd3;  
	msg_printf(DBG,"At offset 3 of version reg, reading value %#08x.\n", 
					rd3 & 0x3f);

	/* check How many GE7s installed? */
	Gr2ProbeGEs(base);

	GBL_GRVers = ~rd0 & 0x0f;
		
	/* Extract board status from fields   */
	if (GBL_GRVers < 4) {
		GBL_ZbufferVers = (rd1 & 0x0c) >> 2;
		GBL_Zconfig = (rd1 & 0x02) >> 1;
		GBL_SuperVidVers = (rd2 & 0x30) >> 4;
		GBL_VidBckEndVers = (rd2 & 0x0c) >> 2; 
		GBL_VMAVers = rd3 & 0x03; 
		GBL_VMBVers = (rd3 & 0x0c) >> 2; 
		GBL_VMCVers = (rd3 & 0x30) >> 4; 
		GBL_MonType = (rd1 & 0x01) | ((rd2 & 0x07) << 1);
		GBL_RE3Count = (rd0 & 0x20) >> 5;  	

	} else {
		GBL_ZbufferVers = (rd1 & 0x20) >> 5;
		GBL_VM24 = (rd1 & 0x10) >> 4; 
		GBL_SuperVidVers = (rd1 & 0xc) >> 2;
		GBL_VidBckEndVers = rd1 & 0x3; 
		GBL_MonType = (rd0 & 0xf0) >> 4; 
	}

/*
 *	Determine monitor type by ORing bits together to form the 4-bit value 
 */
	switch (GBL_MonType & 0xf) { 
          /* unity mapping of the new types */
		case 15:
		case 14:
		case 13:
		case 12:
		case 11:
		case 10:
		case 9:
		case 8:
			break;

			/* monitor type bit2 bit1 bit 0 */
		case 0x7: 	GBL_MonType = 7;	/*1280x1024 
							high resolution*/
				break;
		case 0x6: 	GBL_MonType = 6;	/*1024x768 
							low resolution*/
				break;
		case 0x5: 	GBL_MonType = 5;	/*Dual scan, both high
							and low resolution */
				break;
		case 0x4: 	GBL_MonType = 4;	/*1280x1024 and stereo
						     (both 492 and 512 lines)*/
				break;
		case 0x2: 	GBL_MonType = 2;	/* 16", multiscan */
				break;
		case 0x1: 	GBL_MonType = 1;	/* 19" multiscan, or 72 HZ" */
				break;
		default:	GBL_MonType = 3;	/*Future monitors*/	
				msg_printf(ERR,"        Error reading monitor \
type: NOT DEFINED in EXPRESS diagnostics file hwinv.c\n");
				TEST_ERR++;
				break;
	} /* inventory of GR2 board*/
	TEST_ERR += print_hinv();

	if (TEST_ERR)
		msg_printf(ERR,"Graphics alive?: HWINV test -- FAILED.\n");	
	else {
		msg_printf(DBG,"Graphics alive?: Boards installed correctly -- PASSED\n");	
	}

	return 0;
}

/*
 * print_hinv() -- Print version number and configuration of boards. 
 *		   Return number of errors.
 */
int print_hinv(void)
{
	int error = 0;

	msg_printf(DBG,"	GR2 board: Version %d\n",GBL_GRVers);
	msg_printf(DBG,"	Number of GE7s:   %d\t",GBL_NUMGE);

	if (GBL_GRVers >= 4) {
		if (GBL_NUMGE == 8)
			GBL_RE3Count = 2;
		else 
			GBL_RE3Count = 1;
		msg_printf(DBG,"	Number of RE3s:   %d\n", GBL_RE3Count);

		if (GBL_ZbufferVers == 1)
			msg_printf(DBG,"	Z-Buffer Option: Installed\t");
		else
			msg_printf(DBG,"	Z-Buffer Option: Not Installed\t");
		if (GBL_VM24) {
			GBL_VMAVers = GBL_VMBVers = GBL_VMCVers = 1;  
			msg_printf(DBG,"24 bit-planes\n");
		} else {
			GBL_VMAVers = 1;
			GBL_VMBVers = GBL_VMCVers = 3;  
			msg_printf(DBG,"8 bit-planes\n");
		}

		/* VB2 starts from 0-0 */
		msg_printf(DBG,"	VB2 Option: rev. %d",GBL_VidBckEndVers);
		msg_printf(DBG,"	Super Video Board Option: ");

		if (GBL_SuperVidVers == 3)
			msg_printf(DBG,"Not Installed.\n ");
		else
			msg_printf(DBG,"rev. %d\n ",GBL_SuperVidVers);
	
	} else {
		msg_printf(DBG,"	Z-Buffer Board Option: ");

		DBG_PRINT_VERS(GBL_ZbufferVers);
		if (GBL_ZbufferVers != 3) {
		msg_printf(DBG,"	Z-Buffer DRAM configuration: ");
		if (GBL_Zconfig == 0)
			msg_printf(DBG,"64K x 16\n");
		else
			msg_printf(DBG,"256K x 4\n");
		}
		msg_printf(DBG,"	Number of RE3s:   %d\n",
				GBL_RE3Count ? 2 : 1);
		msg_printf(DBG,"	VB1 Option: ");
		DBG_PRINT_VERS(GBL_VidBckEndVers);

/* 
 * Check for VM2 boards installed in the wrong slots.
 */
		if (GBL_VMAVers == 3) {   /* BRD_NOT_INSTALLED = 3 */   
			if (	(GBL_VMBVers != 3) || 
	     			(GBL_VMCVers != 3)   ) 
			{
				msg_printf(ERR,"	Error in installing VM Boards: \
1st slot is empty. Slots should be filled in order.\n");
				error++;
			}
		}

		if  (GBL_VMBVers == 3) 
		{
			if (GBL_VMCVers != 3)  
			{
			msg_printf(ERR,"	Error in installing VM Boards: \
2nd slot is empty. Slots should be filled in order.\n");
			error++;
			}
		}	 

		msg_printf(DBG,"	First slot, VM2 Board Option: ");
		DBG_PRINT_VERS(GBL_VMAVers);

		msg_printf(DBG,"	Second slot, VM2 Board Option: ");
		DBG_PRINT_VERS(GBL_VMBVers);

		msg_printf(DBG,"	Third slot, VM2 Board Option: ");
		DBG_PRINT_VERS(GBL_VMCVers);

/*
 * Check for # bitplane configuration  (# of boards installed * 8 bitplanes)   
 * Assume if third VM2 slot is filled, previous two slots are also filled.
 */
		if (GBL_VMCVers != 3)	/* 3 = BRD_NOT_INSTALLED */
			msg_printf(DBG,"        VM2 configuration: 24 bitplanes\n");
		else if (GBL_VMBVers != 3)
			msg_printf(DBG,"        VM2 configuration: 16 bitplanes\n");
		else if (GBL_VMAVers != 3) 
			msg_printf(DBG,"        VM2 configuration: 8 bitplanes\n");
		else {
			msg_printf(ERR,"No VM2 boards installed, so no bitplanes!\n");
			error++;
			}
		msg_printf(DBG,"	Super Video Board Option: ");
		DBG_PRINT_VERS(GBL_SuperVidVers);
	}

	
/* 
 * Print Monitor Type 
 */
	switch (GBL_MonType & 0xf) {
                case 15:
		msg_printf(DBG,"        Monitor Type: Default 1280x1024@60hz monitor presumed\n" );
                break;

                case 14:
		msg_printf(DBG,"        Monitor Type: 14\" 1024x768 monitor\n" );
                break;

                case 13:
		msg_printf(DBG,"        Monitor Type: 16\" 1280x1024 and 1024x768 monitor\n" );
                break;

                case 12:
		msg_printf(DBG,"        Monitor Type: 19\" 1280x1024 and 1024x768 monitor\n" );
                break;

                case 11:
		msg_printf(DBG,"        Monitor Type: 21\" Multi-scan, Stereo capable monitor\n");
                break;

                case 10:
		msg_printf(DBG,"        Monitor Type: 16\" Multi-scan monitor\n");
                break;

                case 9:
		msg_printf(DBG,"        Monitor Type: 19\" Multi-scan, 72Hz Single-scan monitor\n");
                break;

                case 8:
		msg_printf(DBG,"        Monitor Type: 1280x1024, Stereo monitor\n");
                break;

                case 7:
		msg_printf(DBG,"        Monitor Type: 19\" monitor\n");
                break;

                case 6:
		msg_printf(DBG,"        Monitor Type: 16\" monitor\n");
                break;

                case 5:
		msg_printf(DBG,"        Monitor Type: Dual scan 16\" monitor\n");
                break;

                case 4:
		msg_printf(DBG,"        Monitor Type: 19\" monitor with stereo\n");
                break;

		case 2:
		msg_printf(DBG,"        Monitor Type: 16\" multiscan monitor\n")
;
		break;

		case 1:
		msg_printf(DBG,"        Monitor Type: 19\" multiscan, or 72 HZ only,  monitor\n");
		break;
                default:        
                msg_printf(ERR,"        Error reading monitor type:  %d\
NOT DEFINED in EXPRESS diagnostics file hwinv.c\n", GBL_MonType);
                break;
        }
	return error;
}

int
gr2_GetVidBckEndVers()
{
	if (gr2_GetGr2BdVers() < 4)
        	GBL_VidBckEndVers = ((base->bdvers.rd2 & 0x0c) >> 2) - 1;
	else 
        	GBL_VidBckEndVers = base->bdvers.rd1 & 0x3;

	return (GBL_VidBckEndVers);
}
int
gr2_GetGr2BdVers(void)
{
	int rd0;

	rd0 = base->bdvers.rd0;

	GBL_GRVers = ~rd0 & 0x0f;
        /* 1's complement is used */
	return (GBL_GRVers);
}

int
gr2_GetMonType(void)
{
        GBL_MonType=((base->bdvers.rd2 & 0x3) << 1) | (base->bdvers.rd1 & 0x1);
        return (GBL_MonType);
}

/* ide interface to lib Gr2ProbeAll().
 */
/*ARGSUSED*/
int
gr2probeall(int argc, char **argv)
{
	char *tmp[4];

	return(Gr2ProbeAll(tmp));
}
