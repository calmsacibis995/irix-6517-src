/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       evo_voc_reg.c
*/

#include "evo_diag.h"

/*
 * Forward Function References
 */

__uint32_t evo_voc_RegTest(__uint32_t, char **);

/*
 * NAME: evo_voc_RegTest
 *
 * FUNCTION: Writes and reads back walking 0's/1's & 0x00,0x55, 0xaa on to 
 * the indirect address registers.
 * 
 * XXX Requires changing for
 * XXX	 			i)	double buffered,  
 * XXX				ii)	vof vertical state table
 * XXX				iii)	signal edge register file
 * XXX registers
 *
 * INPUTS:  argc, argv 
 *
 * OUTPUTS: -1 if error, 0 if no error
 *
 * VOF SIGNAL NUMBERS:
 *	To access an edge register at its individual address:-
 *		Signal numbers must first be written to the VOF Control Regsister
 *	0x0 - HSYNC
 *	0x1 - VSYNC
 *	0x2 - CSYNC
 *	0x3 - CBLANK
 *	0x4 - TRISYNC
 *	0x5 - FIELD_START
 *	0x6 - FIELD
 *	0x7 - FRAME_START
 *	0x8 - VIDEO_ACTIVE
 *	0x9 - LINE_START
 *	0xa - STEREO_OUT
 *	0xb - IMPREG_LD
 *	0xc - XMAPREG_LD
 *	0xd - VOF_VDRC_EN
 */

__uint32_t
evo_voc_RegTest(__uint32_t argc, char **argv)
{

      __uint32_t send,rcv ;
      __uint32_t errors = 0, errCode ;
      __uint32_t  badarg = 0, limit, i, j, k;
      __int32_t bus, test = 3, num = 1;


voc_Regs      evo_voc_regs[] = {
{"VOC_STATUS",			VOC_STATUS,
MASK_VOC_STATUS,		EVO_READ_ONLY},

{"VOC_CONTROL",			VOC_CONTROL,
MASK_VOC_CONTROL,		EVO_VOC_CONTROL },

{"VOC_DBUFFER",			VOC_DBUFFER,           
MASK_VOC_DBUFFER,		EVO_WRITE_CLEAR },

{"VOC_IWSC_CONTROL", 		VOC_IWSC_CONTROL,
MASK_VOC_IWSC_CONTROL, 		EVO_DBL_BUFFER },

{"VOC_IWSC_OFFSETS", 		VOC_IWSC_OFFSETS, 
MASK_VOC_IWSC_OFFSETS, 		EVO_DBL_BUFFER },

{"VOC_IWSC_SIZES", 		VOC_IWSC_SIZES, 
MASK_VOC_IWSC_SIZES,  		EVO_DBL_BUFFER },

{"VOC_VOF_CONTROL", 		VOC_VOF_CONTROL, 
MASK_VOC_VOF_CONTROL, 		EVO_VOF_CONTROL },

{"VOC_VOF_SIGNAL_INIT",		VOC_VOF_SIGNAL_INIT,
MASK_VOC_VOF_SIGNAL_INIT,	EVO_ERROR_RW },

{"VOC_VPHASE",			VOC_VPHASE, 
MASK_VOC_VPHASE,     		EVO_ERROR_RW },

{"VOC_VOF_SIGNAL_INIT", 	VOC_VOF_SIGNAL_INIT,
MASK_VOC_VOF_SIGNAL_INIT, 	EVO_SILT },

{"VOC_VSTATE_DUR_TBL", 		VOC_VSTATE_DUR_TBL,   
MASK_VOC_VSTATE_DUR_TBL, 	EVO_VSDT },

{"VOC_E0_GRP_CNT",		VOC_E0_GRP_CNT,    
MASK_VOC_E0_GRP_CNT,		EVO_GXL_COUNT },

{"VOC_E1_GRP_CNT",		VOC_E1_GRP_CNT,    
MASK_VOC_E1_GRP_CNT,		EVO_GXL_COUNT },

{"VOC_E2_GRP_CNT",		VOC_E2_GRP_CNT,    
MASK_VOC_E2_GRP_CNT,		EVO_GXL_COUNT },

{"VOC_E3_GRP_CNT",		VOC_E3_GRP_CNT,    
MASK_VOC_E3_GRP_CNT,		EVO_GXL_COUNT },

{"VOC_E4_GRP_CNT",		VOC_E4_GRP_CNT,    
MASK_VOC_E4_GRP_CNT,		EVO_GXL_COUNT },

{"VOC_E5_GRP_CNT",		VOC_E5_GRP_CNT,   
MASK_VOC_E5_GRP_CNT,		EVO_GXL_COUNT },

{"VOC_E6_GRP_CNT",		VOC_E6_GRP_CNT,    
MASK_VOC_E6_GRP_CNT,		EVO_GXL_COUNT },

{"VOC_E7_GRP_CNT",		VOC_E7_GRP_CNT,    
MASK_VOC_E7_GRP_CNT,		EVO_GXL_COUNT },

{"VOC_VALID_EDGES_LO",		VOC_VALID_EDGES_LO,   
MASK_VOC_VALID_EDGES_LO,	EVO_VE },

{"VOC_VALID_EDGES_HI",		VOC_VALID_EDGES_HI,   
MASK_VOC_VALID_EDGES_HI,	EVO_VE },

{"VOC_VOF_PIXEL_BLANK",		VOC_VOF_PIXEL_BLANK,
MASK_VOC_VOF_PIXEL_BLANK,	EVO_ERROR_RW },

{"VOC_PFD_CONTROL",		VOC_PFD_CONTROL,   
MASK_VOC_PFD_CONTROL,		EVO_ERROR_RW },

{"VOC_PFD_FRM_SEQ_TBL_LO",	VOC_PFD_FRM_SEQ_TBL_LO,
MASK_VOC_PFD_FRM_SEQ_TBL_LO,	EVO_ERROR_RW },

{"VOC_PFD_FRM_SEQ_TBL_HI",	VOC_PFD_FRM_SEQ_TBL_HI, 
MASK_VOC_PFD_FRM_SEQ_TBL_HI,	EVO_ERROR_RW },

{"VOC_PFD_DUR_CTR_OLD",		VOC_PFD_DUR_CTR_OLD, 
MASK_VOC_PFD_DUR_CTR_OLD,	EVO_READ_ONLY },

{"VOC_PSD_DUR_CTR",		VOC_PSD_DUR_CTR, 
MASK_VOC_PSD_DUR_CTR,		EVO_ERROR_RW },

{"VOC_PSD_CONTROL",		VOC_PSD_CONTROL, 
MASK_VOC_PSD_CONTROL,		EVO_ERROR_RW },

{"VOC_VDRC_CONTROL",		VOC_VDRC_CONTROL, 
MASK_VOC_VDRC_CONTROL,		EVO_VDRC_CONTROL },

{"VOC_VDRC_MAW",		VOC_VDRC_MAW, 
MASK_VOC_VDRC_MAW,		EVO_ERROR_RW },

{"VOC_VDRC_BEG_LINE",		VOC_VDRC_BEG_LINE,
MASK_VOC_VDRC_BEG_LINE,		EVO_ERROR_RW },

{"VOC_VDRC_MID_LINE",		VOC_VDRC_MID_LINE,
MASK_VOC_VDRC_MID_LINE,		EVO_ERROR_RW },

{"VOC_VDRC_END_LINE",		VOC_VDRC_END_LINE,
MASK_VOC_VDRC_END_LINE,		EVO_ERROR_RW },

{"VOC_VDRC_GXEL_OFFSET",	VOC_VDRC_GXEL_OFFSET,
MASK_VOC_VDRC_GXEL_OFFSET, 	EVO_ERROR_RW },

{"VOC_VDRC_REQZ",		VOC_VDRC_REQZ,
MASK_VOC_VDRC_REQZ,		EVO_ERROR_RW },

{"VOC_VDRC_REQ0",		VOC_VDRC_REQ0,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ1",		VOC_VDRC_REQ1,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ2",		VOC_VDRC_REQ2,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ3",		VOC_VDRC_REQ3,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ4",		VOC_VDRC_REQ4,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ5",		VOC_VDRC_REQ5,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ6",		VOC_VDRC_REQ6,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ7",		VOC_VDRC_REQ7,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ8",		VOC_VDRC_REQ8,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ9",		VOC_VDRC_REQ9,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQa",		VOC_VDRC_REQa,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQb",		VOC_VDRC_REQb,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQc",		VOC_VDRC_REQc,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQd",		VOC_VDRC_REQd,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQe",		VOC_VDRC_REQe,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQf",		VOC_VDRC_REQf,
MASK_VOC_VDRC_REQ,		EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN0",	VOC_VDRC_REQ_PATTERN0,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN1",	VOC_VDRC_REQ_PATTERN1,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN2",	VOC_VDRC_REQ_PATTERN2,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN3",	VOC_VDRC_REQ_PATTERN3,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN4",	VOC_VDRC_REQ_PATTERN4,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN5",	VOC_VDRC_REQ_PATTERN5,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN6",	VOC_VDRC_REQ_PATTERN6,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN7",	VOC_VDRC_REQ_PATTERN7,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN8",	VOC_VDRC_REQ_PATTERN8,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERN9",	VOC_VDRC_REQ_PATTERN9,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERNa",	VOC_VDRC_REQ_PATTERNa,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERNb",	VOC_VDRC_REQ_PATTERNb,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERNc",	VOC_VDRC_REQ_PATTERNc,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERNd",	VOC_VDRC_REQ_PATTERNd,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERNe",	VOC_VDRC_REQ_PATTERNe,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_VDRC_REQ_PATTERNf",	VOC_VDRC_REQ_PATTERNf,
MASK_VOC_VDRC_REQ_PATTERN, 	EVO_VDRC },

{"VOC_PLL_CONTROL",		VOC_PLL_CONTROL,
MASK_VOC_PLL_CONTROL,		EVO_PLL_CONTROL },

{"VOC_PLL_ADDRESS",		VOC_PLL_ADDRESS,
MASK_VOC_PLL_ADDRESS,		EVO_ERROR_RW },

{"VOC_PLL_READBACK",		VOC_PLL_READBACK,
MASK_VOC_PLL_READBACK,		EVO_READ_ONLY },

{"VOC_PLL_DATA",		VOC_PLL_DATA,
MASK_VOC_PLL_DATA,		EVO_ERROR_RW },

{"VOC_GEN_CONTROL",		VOC_GEN_CONTROL,
MASK_VOC_GEN_CONTROL,		EVO_GEN_CONTROL },

{"VOC_GEN_HMASK_1",		VOC_GEN_HMASK_1,
MASK_VOC_GEN_HMASK_1,		EVO_ERROR_RW },

{"VOC_GEN_HMASK_2",		VOC_GEN_HMASK_2,
MASK_VOC_GEN_HMASK_2,		EVO_ERROR_RW },

{"VOC_GEN_BPCLAMP",		VOC_GEN_BPCLAMP,
MASK_VOC_GEN_BPCLAMP,		EVO_ERROR_RW },

{"VOC_GEN_HMASK_2",		VOC_GEN_HMASK_2,
MASK_VOC_GEN_HMASK_2,		EVO_ERROR_RW },

{"VOC_FIFO_CONTROL",		VOC_FIFO_CONTROL,
MASK_VOC_FIFO_CONTROL,		EVO_FIFO_CONTROL },

{"VOC_FIFO_RW_POINTER",		VOC_FIFO_RW_POINTER,
MASK_VOC_FIFO_RW_POINTER,	EVO_FIFO_PTR },

{"VOC_SAC_CONTROL",		VOC_SAC_CONTROL,
MASK_VOC_SAC_CONTROL,		EVO_READ_WRITE },

{"VOC_SIZ_CONTROL",		VOC_SIZ_CONTROL,
MASK_VOC_SIZ_CONTROL,		EVO_ERROR_RW },

{"VOC_SIZ_X_COEF_MAX",		VOC_SIZ_X_COEF_MAX,
MASK_VOC_SIZ_X_COEF_MAX,	EVO_ERROR_RW },

{"VOC_SIZ_X_FLTR_SLP",		VOC_SIZ_X_FLTR_SLP,
MASK_VOC_SIZ_X_FLTR_SLP,	EVO_ERROR_RW },

{"VOC_SIZ_X_DELTA",		VOC_SIZ_X_DELTA,
MASK_VOC_SIZ_X_DELTA,		EVO_ERROR_RW },

{"VOC_SIZ_X_C0_BEG",		VOC_SIZ_X_C0_BEG,
MASK_VOC_SIZ_X_C0_BEG,		EVO_ERROR_RW },

{"VOC_SIZ_X_C1_BEG",		VOC_SIZ_X_C1_BEG,
MASK_VOC_SIZ_X_C1_BEG,		EVO_ERROR_RW },

{"VOC_SIZ_X_C2_BEG",		VOC_SIZ_X_C2_BEG,
MASK_VOC_SIZ_X_C2_BEG,		EVO_ERROR_RW },

{"VOC_SIZ_X_C3_BEG",		VOC_SIZ_X_C3_BEG,
MASK_VOC_SIZ_X_C3_BEG,		EVO_ERROR_RW },

{"VOC_SIZ_Y_COEF_MAX",		VOC_SIZ_Y_COEF_MAX,
MASK_VOC_SIZ_Y_COEF_MAX,	EVO_ERROR_RW },

{"VOC_SIZ_Y_FLTR_SLP",		VOC_SIZ_Y_FLTR_SLP,
MASK_VOC_SIZ_Y_FLTR_SLP,	EVO_ERROR_RW },

{"VOC_SIZ_Y_DELTA",		VOC_SIZ_Y_DELTA,
MASK_VOC_SIZ_Y_DELTA,		EVO_ERROR_RW },

{"VOC_SIZ_Y_S0_F0_BEG",		VOC_SIZ_Y_S0_F0_BEG,
MASK_VOC_SIZ_Y_S0_F0_BEG,	EVO_ERROR_RW },

{"VOC_SIZ_Y_S1_F0_BEG",		VOC_SIZ_Y_S1_F0_BEG,
MASK_VOC_SIZ_Y_S1_F0_BEG,	EVO_ERROR_RW },

{"VOC_SIZ_Y_S0_F1_BEG",		VOC_SIZ_Y_S0_F1_BEG,
MASK_VOC_SIZ_Y_S0_F1_BEG,	EVO_ERROR_RW },

{"VOC_SIZ_Y_S1_F1_BEG",		VOC_SIZ_Y_S1_F1_BEG,
MASK_VOC_SIZ_Y_S1_F1_BEG,	EVO_ERROR_RW },

{"", -1, -1, -1 }
};

voc_Regs	*voc_ptr = evo_voc_regs;

      /* get the args */
      argc--; argv++;
      while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'b':
                        if (argv[0][2]=='\0') {
                            atob(&argv[1][0], &bus);
                            argc--; argv++;
                        } else {
                            atob(&argv[0][2], &bus);
                        }
                        break;
                case 't':
                        if (argv[0][2]=='\0') {
                            atob(&argv[1][0], &test);
                            argc--; argv++;
                        } else {
                            atob(&argv[0][2], &test);
                        }
                        break;
		default: badarg++; break;
	}
	argc --; argv ++;
     }

     if (badarg || argc ||  bus < 0 || bus > 1 ) {
	msg_printf(SUM,"\
	Usage: evo_voc_reg -n [1|2] -b [0|1] [-t [0|1|2|3]]\n\
		-b --> Bus   0-Address   1-Data\n\
		-t --> Test   0-walking 0   1-walking 1    2-test pattern 3-all values \n\
		( -t option is used only for data bus test)\n");

	return -1;
     }
     
     if ( bus == 0 ) {
	errCode = VOC_ADDR_TEST ;
	msg_printf(SUM, "--> Evo_voc_reg started\n");
     } else {
	errCode = VOC_DATA_TEST ;
	msg_printf(SUM, "--> Evo_voc_reg started\n");
     }


     if ( bus == 0 ) {                            /* Address Bus test */
	i = 0;   
	while (voc_ptr->name[0] != NULL){
		switch(voc_ptr->mode)  {
			case EVO_READ_WRITE:
				send = (__uint32_t) i;
				send = (send & voc_ptr->mask);
				msg_printf(DBG, "VOC Addr: Accessing Register %s\n", voc_ptr->name);
				EVO_VOC_W1(evobase, voc_ptr->address, send);
				EVO_VOC_R1(evobase, voc_ptr->address, rcv );
				/*rcv = (rcv & voc_ptr->mask);*/
       		 		if (send != rcv) {
		  			errors++;
					msg_printf(ERR, "VOC Addr Test Err: Accessing Register %s\n", voc_ptr->name);
       		   			msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
       		 		}
				i++;
				voc_ptr++;
				break;

			case EVO_WRITE_CLEAR:
				send = (__uint32_t)0x1;
				send = (send & voc_ptr->mask);
				msg_printf(DBG, "VOC Addr: Accessing Register %s\n", voc_ptr->name);
				EVO_VOC_W1(evobase, voc_ptr->address, send);
				EVO_VOC_R1(evobase, voc_ptr->address, rcv );
				if (rcv == 1) {
		  			errors++;
					msg_printf(ERR, "VOC Addr Test Err: Accessing Register %s\n", voc_ptr->name);
					msg_printf(ERR, "VOC Addr Test Err: Register %s not cleared\n", voc_ptr->name);
				}
				voc_ptr++;
				break;

			case EVO_DBL_BUFFER:
				send = (__uint32_t) i;
				send = (send & voc_ptr->mask);
				msg_printf(DBG, "VOC Addr: Accessing Register %s\n", voc_ptr->name);
				EVO_VOC_W1(evobase, EVO_VOC_CONTROL, 0x0);
				EVO_VOC_W1(evobase, EVO_VOF_CONTROL, 0x0);
				send = (send & voc_ptr->mask);
				msg_printf(DBG, "VOC Addr: Accessing Dbl. Buf. Register %s\n", voc_ptr->name);
 	   			EVO_VOC_W1(evobase, voc_ptr->address, send);

	   			EVO_VOC_R1(evobase, voc_ptr->address, rcv );
				/*rcv = (rcv & voc_ptr->mask);*/
       		 		if (send != rcv) {
		  			errors++;
					msg_printf(ERR, "VOC Addr Test Err: Accessing Register %s\n", voc_ptr->name);
       		   			msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
       		 		}
				i++;
				voc_ptr++;
				break;
#if 0
			case EVO_VDRC:
				send = (__uint32_t) i;
                                send = (send & voc_ptr->mask);
                                msg_printf(DBG, "VOC Addr: Accessing Register %s\n", voc_ptr->name);

				/*setting up VOC_CONTROL for immediate updates*/
				EVO_VOC_W1(evobase, EVO_VOC_CONTROL, 0x0);
				EVO_VOC_W1(evobase, EVO_VOF_CONTROL,0x0);
/*#if 0This part was commented*/
				/*setting up access to appropriate field request register*/
				if ((voc_ptr->address) < 0x58){
					EVO_VOC_W1(evobase, EVO_VOF_CONTROL,0x10);
				} else {
					EVO_VOC_W1(evobase, EVO_VOF_CONTROL,0x30);
				}
				us_delay(100);
/*#endif end sub comment*/
				EVO_VOC_W1(evobase, voc_ptr->address, send);
				us_delay(100);
				EVO_VOC_W1(evobase, VOC_VDRC_CONTROL, 0x3);
				us_delay(100);
	   			EVO_VOC_R1(evobase, voc_ptr->address, rcv);
				rcv = (rcv & voc_ptr->mask);
       		 		if (send != rcv) {
		  			errors++;
					msg_printf(ERR, "VOC Addr Test Err: Accessing Register %s\n", voc_ptr->name);
       		   			msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
       		 		}
				i++;
				voc_ptr++;
				break;

			  case EVO_GXL_COUNT:
				send = (__uint32_t) i;
				if(send <  0xd){
					i++;
					voc_ptr++;
					break;
				}
                                send = (send & voc_ptr->mask);
                                msg_printf(DBG, "VOC Addr: Accessing Register %s\n", voc_ptr->name);

				/*setting up VOC_CONTROL for immediate updates*/
				EVO_VOC_W1(evobase, EVO_VOC_CONTROL, 0x0);
				EVO_VOC_W1(evobase, EVO_VOF_CONTROL,0x1f0);
				EVO_VOC_W1(evobase, voc_ptr->address, send);
				us_delay(100);
	   			EVO_VOC_R1(evobase, voc_ptr->address, rcv);
				rcv = (rcv & voc_ptr->mask);
       		 		if (send != rcv) {
		  			errors++;
					msg_printf(ERR, "VOC Addr Test Err: Accessing Register %s\n", voc_ptr->name);
       		   			msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
       		 		}
				i++;
				voc_ptr++;
				break;
#endif
			default:
				i++;	
				voc_ptr++;
				break;
		}
	}
     }
     else { /* Register tests */

	if ( test < 0 || test > 3 ) test = 3;

        if ( test == 2 ) limit = 3;
        else if ( test == 0 || test ==  1 ) limit = 8 ;
	else limit = 0xff;


	   /* XXX If all registers need to read/ written to use a for loop here*/ 
	   voc_ptr = evo_voc_regs;
	   while (voc_ptr->name[0] != NULL){
		switch(voc_ptr->mode) {
			case EVO_READ_WRITE:
        			for ( i = 0; i < limit ; i++) { 
			           if (test == 0)
					send = ~ (evo_walk_one[i] & 0xff) ;/*  walking zero bit test */
				   else if (test == 1)
					send =  evo_walk_one[i] & 0xff ;   /*  walking one  bit test */
				   else if (test == 2) 
					send =  (evo_patrn32[i] & 0xff) ;  /*  test pattern */
				   else send = (__uint32_t) i;		   /*  all values between 0 & 0xff */
				   send = (send & voc_ptr->mask);
				   msg_printf(DBG, "VOC Addr: Accessing Register %s\n", voc_ptr->name);
 	   			   EVO_VOC_W1(evobase, voc_ptr->address, send);

	   			   EVO_VOC_R1(evobase, voc_ptr->address, rcv );
				   rcv = (rcv & voc_ptr->mask);

          			   if (send != rcv) {
	    			         	errors++;
            			         	msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
          			   }
				}
				voc_ptr++;
				break;
			case EVO_READ_ONLY:
				voc_ptr++;
				break;
			case EVO_WRITE_ONLY:
				voc_ptr++;
				break;
			case EVO_DBL_BUFFER:
        			for ( i = 0; i < limit ; i++) { 
			           if (test == 0)
					send = ~ (evo_walk_one[i] & 0xff) ;/*  walking zero bit test */
				   else if (test == 1)
					send =  evo_walk_one[i] & 0xff ;   /*  walking one  bit test */
				   else if (test == 2) 
					send =  (evo_patrn32[i] & 0xff) ;  /*  test pattern */
				   else send = (__uint32_t) i;		   /*  all values between 0 & 0xff */
				   /*Control register dbl. buf. reg. update bits must first be set for immediate updation*/
				   EVO_VOC_W1(evobase, EVO_VOF_CONTROL, 0x0);
				   send = (send & voc_ptr->mask);
				   msg_printf(DBG, "VOC Addr: Accessing Dbl. Buf. Register %s\n", voc_ptr->name);
 	   			   EVO_VOC_W1(evobase, voc_ptr->address, send);

	   			   EVO_VOC_R1(evobase, voc_ptr->address, rcv );
				   /*rcv = (rcv & voc_ptr->mask);*/

          			   if (send != rcv) {
	    			         	errors++;
            			         	msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
          			   }
				}
				voc_ptr++;
				break;
#if 0
			case EVO_VSDT:
        			for ( i = 0; i < limit ; i++) { 
			           if (test == 0)
					send = ~ (evo_walk_one[i] & 0xff) ;/*  walking zero bit test */
				   else if (test == 1)
					send =  evo_walk_one[i] & 0xff ;   /*  walking one  bit test */
				   else if (test == 2) 
					send =  (evo_patrn32[i] & 0xff) ;  /*  test pattern */
				   else send = (__uint32_t) i;		   /*  all values between 0 & 0xff */
				}
				/* Write to WO VOF control bit to reset to starting address of VSD Tbl. and Line Type Tbl.*/
				/* VSD and Line Type Tbl Locations are auto-incremented. The code below tests those tables*/

				EVO_VOC_W1(evobase, EVO_VOF_CONTROL, 0x8);
				send = (send & MASK_VOC_VSTATE_DUR_TBL);
				msg_printf(DBG, "VOC Addr: Accessing VSDT Register File %s\n", voc_ptr->name);
				for(k=0; k<1024; k++){
					EVO_VOC_W1(evobase, VOC_VSTATE_DUR_TBL, send);
				}
				EVO_VOC_W1(evobase, EVO_VOF_CONTROL, 0x8);
				for(k=0; k<1024; k++){
					EVO_VOC_R1(evobase, VOC_VSTATE_DUR_TBL, rcv);
					rcv = (rcv & MASK_VOC_VSTATE_DUR_TBL);
          				if (send != rcv) {
	    					errors++;
            					msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
          				}
					rcv = ~send;
				}

			case EVO_SILT:
				  voc_ptr++;
				  break;
			
			case EVO_GXL_COUNT:
        			for ( i = 0; i < limit ; i++) { 
			           if (test == 0)
					send = ~ (evo_walk_one[i] & 0xff) ;/*  walking zero bit test */
				   else if (test == 1)
					send =  evo_walk_one[i] & 0xff ;   /*  walking one  bit test */
				   else if (test == 2) 
					send =  (evo_patrn32[i] & 0xff) ;  /*  test pattern */
				   else send = (__uint32_t) i;		   /*  all values between 0 & 0xff */
				}
				  /*Writing signal number to voc_vof_control*/
				  EVO_VOC_W1(evobase, VOC_VOF_CONTROL, 0x0);
				  send = (send & voc_ptr->mask);
				  msg_printf(DBG, "VOC Addr: Accessing GXL Count Register %s\n", voc_ptr->name);
				  EVO_VOC_W1(evobase, voc_ptr->address, send);
				  EVO_VOC_R1(evobase, voc_ptr->address, rcv);
				  rcv = (rcv & voc_ptr->mask);
          			  if (send != rcv) {
	    					errors++;
            					msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
          			  }
				  voc_ptr++;
				  break;
			case EVO_VE:
        			for ( i = 0; i < limit ; i++) { 
			           if (test == 0)
					send = ~ (evo_walk_one[i] & 0xff) ;/*  walking zero bit test */
				   else if (test == 1)
					send =  evo_walk_one[i] & 0xff ;   /*  walking one  bit test */
				   else if (test == 2) 
					send =  (evo_patrn32[i] & 0xff) ;  /*  test pattern */
				   else send = (__uint32_t) i;		   /*  all values between 0 & 0xff */
				}
				  /*VOF_CONTROL line type has to be specified*/
				  EVO_VOC_W1(evobase, VOC_VOF_CONTROL, 0x0);
				  msg_printf(DBG, "VOC Addr: Accessing VE Register %s\n", voc_ptr->name);
				  send = (send & MASK_VOC_VALID_EDGES_LO);
				  EVO_VOC_W1(evobase, VOC_VALID_EDGES_LO, send);
				  EVO_VOC_R1(evobase, VOC_VALID_EDGES_LO, rcv);
				  rcv = (rcv & MASK_VOC_VALID_EDGES_LO);
          			  if (send != rcv) {
	    					errors++;
            					msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
          			  }
				  send = (send & MASK_VOC_VALID_EDGES_LO);
				  voc_ptr++;
				  EVO_VOC_W1(evobase, VOC_VALID_EDGES_HI, send);
				  EVO_VOC_R1(evobase, VOC_VALID_EDGES_HI, rcv);
				  rcv = (rcv & MASK_VOC_VALID_EDGES_LO);
          			  if (send != rcv) {
	    					errors++;
            					msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
          			  }
				  voc_ptr++;
				  break;
			case EVO_VOF_CONTROL:
        			for ( i = 0; i < limit ; i++) { 
			           if (test == 0)
					send = ~ (evo_walk_one[i] & 0xff) ;/*  walking zero bit test */
				   else if (test == 1)
					send =  evo_walk_one[i] & 0xff ;   /*  walking one  bit test */
				   else if (test == 2) 
					send =  (evo_patrn32[i] & 0xff) ;  /*  test pattern */
				   else send = (__uint32_t) i;		   /*  all values between 0 & 0xff */
				}
				  send = (send & MASK_VOC_VOF_CONTROL);
				  msg_printf(DBG, "VOC Addr: Accessing VOF Control Register %s\n", voc_ptr->name);
				  EVO_VOC_W1(evobase, VOC_VOF_CONTROL, send);
				  EVO_VOC_R1(evobase, VOC_VOF_CONTROL, rcv);
				  rcv = (rcv & MASK_VOC_VOF_CONTROL);
          			  if (send != rcv) {
	    					errors++;
            					msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
          			  }
				  voc_ptr++;
				  break;
#endif
			default : 
				  voc_ptr++;
				  break;

		}
	    }
      }

   if (errors == 0){ 	/*no errors*/
     if (errCode == VOC_ADDR_TEST) { 
	msg_printf(SUM, "--> Evo_voc_reg passed\n");
     } else {
	msg_printf(SUM, "--> Evo_voc_reg passed\n");
     }
   } else {		/*errors*/
     if (errCode == VOC_DATA_TEST) { 
	msg_printf(SUM, "--> Evo_voc_reg failed\n");
     } else {
	msg_printf(SUM, "--> Evo_voc_reg failed\n");
     }
   }
   return(_evo_reportPassOrFail((&evo_err[errCode]) ,errors));
}
