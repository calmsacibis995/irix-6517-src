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
**      FileName:       mgv_csc_util.c
*/

#include "mgv_diag.h"

/*
 * Forward Function References
 */

void _mgv_csc_ReadLut(__uint32_t, __uint32_t, u_int16_t *);
void _mgv_csc_LoadLut(__uint32_t, __uint32_t, u_int16_t *);
void _mgv_csc_LoadBuf(__uint32_t, u_int16_t *, __uint32_t , __uint32_t );
__uint32_t _mgv_csc_VerifyBuf(__uint32_t, __uint32_t, u_int16_t *, u_int16_t * );
__uint32_t _mgv_csc_VerifyX2KBuf(__uint32_t, __uint32_t, u_int16_t *, u_int16_t * ,__uint32_t );
u_int16_t _mgv_csc_GenerateGoldenValue(u_int16_t, __uint32_t );
void _mgv_csc_setupDefault(void);

void _mgv_csc_ReadLut(__uint32_t selectLUT, __uint32_t outputLUT, u_int16_t *buf)
{
	int i ;
	uchar_t low, high ;
	uchar_t value ;
	mgv_csc_sel_ctrl_t cscSelCtrl;

	cscSelCtrl.luts_mode       = CSC_READ_LUT_MODE ;
	cscSelCtrl.auto_inc_normal = CSC_RESET_AUTO_INCREMENT ;
	cscSelCtrl.sel_lut_addr    = selectLUT ;
	cscSelCtrl.sel_output_lut  = outputLUT ;

	bcopy((char *)&cscSelCtrl , (char *)&value, sizeof(mgv_csc_sel_ctrl_t));
	CSC_IND_W1(mgvbase, CSC_SELECT_CTRL, value);

	cscSelCtrl.auto_inc_normal = CSC_RESET_NORMAL ;

	bcopy((char *)&cscSelCtrl , (char *)&value, sizeof(mgv_csc_sel_ctrl_t));
	CSC_IND_W1(mgvbase, CSC_SELECT_CTRL, value);

	for ( i = 0; i < CSC_MAX_LOAD; i++ )
	{
		CSC_IND_R1(mgvbase, CSC_INDIR_LUTWRITE, low);
		CSC_IND_R1(mgvbase, CSC_INDIR_LUTWRITE, high);

		buf[i] = (u_int16_t)(low | (high << 8) );
	}

	/* Reset to normal mode after read back */

	/*
	cscSelCtrl.luts_mode       = CSC_NORMAL_MODE ;
	bcopy((char *)&cscSelCtrl , (char *)&value, sizeof(mgv_csc_sel_ctrl_t));
	*/

	value = 0;

	CSC_IND_W1(mgvbase, CSC_SELECT_CTRL, value);
}

void _mgv_csc_LoadLut(__uint32_t mode, __uint32_t lutId, u_int16_t *buf)
{
	int i ;
	uchar_t value ;
	mgv_csc_sel_ctrl_t cscSelCtrl;
	uchar_t select_reset ;

	if ( mode == CSC_COEF_LOAD ) {
		select_reset = 1 | (1 << 2) ;
		CSC_IND_W1(mgvbase, CSC_SELECT_CTRL,select_reset);
		select_reset = 1 ;
		CSC_IND_W1(mgvbase, CSC_SELECT_CTRL,select_reset);

		for ( i = 0; i < CSC_MAX_COEF; i++ )
		{
			
			CSC_IND_W1(mgvbase, CSC_INDIR_LUTWRITE, getLowByte(buf[i]));
			CSC_IND_W1(mgvbase, CSC_INDIR_LUTWRITE, getHiByte(buf[i]));

			/* mgv_singlebit(mgv_bd, BT_CSC_LOAD_MODE, 0, VID_WRITE);  XXX kernal code */
		}
	}
	else if (mode == CSC_LUT_LOAD ) {
		cscSelCtrl.luts_mode       = CSC_LOAD_LUT_MODE ;
		cscSelCtrl.auto_inc_normal = CSC_RESET_AUTO_INCREMENT ;
		cscSelCtrl.sel_lut_addr    = lutId ;

		bcopy((char *)&cscSelCtrl , (char *)&value, sizeof(mgv_csc_sel_ctrl_t));
		CSC_IND_W1(mgvbase, CSC_SELECT_CTRL, value);

		cscSelCtrl.auto_inc_normal = CSC_RESET_NORMAL ;

		bcopy((char *)&cscSelCtrl , (char *)&value, sizeof(mgv_csc_sel_ctrl_t));
		CSC_IND_W1(mgvbase, CSC_SELECT_CTRL, value);

		for ( i = 0; i < CSC_MAX_LOAD; i++ )
		{
		
			CSC_IND_W1(mgvbase, CSC_INDIR_LUTWRITE, getLowByte(buf[i]));
			CSC_IND_W1(mgvbase, CSC_INDIR_LUTWRITE, getHiByte(buf[i]));


#if 0
			/* From mgv_csc_bus test, I have doubt about 
			   CSC_IND_W1(). For the time being replace with CSC_W1() 
			*/
			CSC_W1(mgvbase, CSC_ADDR,CSC_INDIR_LUTWRITE);
			CSC_W1(mgvbase, CSC_DATA, getLowByte(buf[i]));
			CSC_W1(mgvbase, CSC_DATA, getHiByte(buf[i]));
#endif
		}

		/* Reset to normal mode after loading */

		/*
		cscSelCtrl.luts_mode       = CSC_NORMAL_MODE ;
		bcopy((char *)&cscSelCtrl , (char *)&value, sizeof(mgv_csc_sel_ctrl_t));
		*/

		value = 0 ;

		CSC_IND_W1(mgvbase, CSC_SELECT_CTRL, value);
       	}

}


void _mgv_csc_LoadBuf(__uint32_t bufLen, u_int16_t * buf,
	__uint32_t mode, __uint32_t lutSelect)
{
	__uint32_t lutFactor, i, j, k, value;

	/* XXXX Tony's table */

#if 1
	__uint32_t yuv2rgb_coef[12] = {299, -101, -209, 0, 299, 262, 0, 256, 299, 0, 410, 0};
#endif

#if 0
	__uint32_t rgb2yuv_coef[12] = {258, 50, 131, 0, -149, 224, -76, 0, -188, -36, 225, 0};

	__uint32_t maxlogic_coef_special[12] = {0x55, 0x55, 0x55, 0, 0x55, 0x55, 0x55, 0, 0x55, 0x55, 0x55, 0};
#endif

    	lutFactor = 1;

	if (lutSelect >= CSC_YG_IN && lutSelect <= CSC_VR_IN)
		lutFactor = csc_inlut ;
	else if ((lutSelect >= CSC_YG_OUT) && (lutSelect <= CSC_VR_OUT))
		lutFactor = csc_outlut ;
	else if (lutSelect == CSC_ALPHA_OUT )
		lutFactor = csc_alphalut ;

	switch (mode) {

	case CSC_LUT_ALL1 :
			for (i = 0 ; i < bufLen ; i++ )
				buf[i] = 1 * lutFactor ;
			break ;

	case CSC_LUT_WALKING10bit:
			for (i = 0, j = 0; i < bufLen ; i++ , j++) {
				if ((j*lutFactor) >= bufLen/2)
					j = 0;
				buf[i] = j * lutFactor ;
			}
			break ;

	case CSC_LUT_PASSTHRU :
			for (i = 0, j = 0; i < bufLen ; i++ , j++) {
				if (j*lutFactor  > bufLen)
					j = 0;
				buf[i] = j * lutFactor ;
			}
			break ;
	
	case CSC_LUT_WALKING_POSITIVE :
            		if (csc_inlut_value) {
                	  for (i = 0; i < bufLen; i++)
                     	    buf[i] = csc_inlut_value * lutFactor ;
            		}
            		else {
                	  for (i = 0, j = 0; i < bufLen; i++, j++) {
                    	    if (j*lutFactor > 0xfff)
                        	j = 0x800;
			    buf[i] = j*lutFactor;
                	  }
            		}
			break ;

	case CSC_LUT_WALKING_NEGATIVE :
			if (csc_inlut_value) {
			  for (i = 0; i < bufLen ; i++)
				buf[i] = csc_inlut_value * lutFactor ;				
			}
			else {
			  for (i = 0, j = 0x800; i < bufLen ; i++, j++) {
				if (j*lutFactor > 0xfff)
					j = 0x800 ;
				buf[i] = j * lutFactor ;
			  }
			}
			break ;

	case CSC_LUT_WALKING_ALL :
			if (csc_inlut_value) {
			  for (i = 0; i < bufLen; i++)
				buf[i] = csc_inlut_value * lutFactor ; 
			}
			else {
			  for (i = 0, j = 0; i < bufLen ; i++, j++) {
				if (j*lutFactor >= bufLen )
					j = 0;
				buf[i] = j * lutFactor ;
			  }
			}
			break ;

	case CSC_LUT_YUV2RGB :
			if (lutSelect >= CSC_YG_OUT && lutSelect <= CSC_VR_OUT) {
				for (i=0; i < 4096  ; i++) {
					j = i;
					if ( i > 2047 )
						j = i - 4096 ;
					value = j + 511 /* HALF  */;

					if ( csc_clipping ) {
						if (value > 1023 )
							value = 1023 ;
						else if ( value < 0 )
							value = 0 ;
					}
					else {
						if (value > 1019 )
							value = 1019 ;
						else if ( value < 4 )
							value = 4 ;
					}
						
					buf[i] = value * lutFactor ;
				}

			} else { /* INPUT LUTS */

                		/* Note: buflen needs to be 1024 here */
                		for (i = 0; i < bufLen; i++)
                		{
		                    value = ((i - 502) * 2 * 1023)/ 876 ;

		                    if (value > 1023)
					value = 1023;
                    	            else if (value < -1024)
                        		value = -1024;

                    	  	    buf[i] = value*lutFactor;
                		}
            		}
      
            		break;
	case CSC_LUT_RGB2YUV:
            			/* Add it later */

            		break;

	case CSC_LUT_CCIR2FULLSCALE:
            			/* Add it later */

            		break;   

	case CSC_X2KLUT_PASSTHRU:
	        /* cscind4[4] is ignored for ccir2fullrange case, 
			but need to verify this */

			for (i = 0; i < bufLen; i++)
		                buf[i] = csc_x2k ; 

            		break;

	case CSC_X2KLUT_CCIR2FULLSCALE:
            			/* add it later */
			break;

	case CSC_X2KLUT_YUV2RGB:
		            /* Constant hue algorithm */ /* XXX - needs to be corrected */

			for (i = 1; i < bufLen; i++)
			{
				if (i > 63)
				{
					j = i / 63.0;
					value = 0xfff / j;
                		}
                		else value = 0xfff;

		                buf[i]  = value;
            		}

            		break;

	case CSC_X2KLUT_RGB2YUV:
            			/* Add it later */
            		break;

	case CSC_COEF_YG_PASSTHRU:

			buf[0] = csc_coef ;

			for (i = 1; i < bufLen; i++)
				buf[i] = 0;

			break;

	case CSC_COEF_UB_PASSTHRU:

			for (i = 0; i < 5; i++)
				buf[i] = 0;

			buf[i] = csc_coef ;

			for (i +=1; i < bufLen; i++)
				buf[i] = 0;

			break;


	case CSC_COEF_VR_PASSTHRU:

			for (i = 0; i < (bufLen-2); i++)
				buf[i] = 0;

			buf[i] = csc_coef ;

			buf[i+1] = 0;

			break;

	case CSC_COEF_ALL_PASSTHRU:

			for (i = 0, j = 0; i < (bufLen/4); i++)
			{
				buf[j] = csc_coef ;

				j++;

				if (j == (bufLen-1))
					buf[j] = 0;
				else {
					for (k = 0; k < 4; k++) {
						buf[j] = 0;
						j++;
					}
				}
			}

			break;

	case CSC_COEF_YUV2RGB:

			for (i = 0; i < bufLen; i++)
				buf[i] = yuv2rgb_coef[i];

			break;

	case CSC_COEF_RGB2YUV:
				/* Add it later */

			break;

	case CSC_COEF_CCIR2FULLSCALE:
				/* Add it later */

			break;

	default:
			break ;
	}
}

__uint32_t _mgv_csc_VerifyBuf(__uint32_t bufLen, __uint32_t lutSelect,
			u_int16_t *inbuffer, u_int16_t* outbuffer)
{
	int i, j, k; 
	u_int16_t gold;
	__uint32_t errorCount = 0;

	if ((lutSelect >= CSC_YG_IN) && (lutSelect <= CSC_VR_IN))
		k = csc_outlut / csc_inlut  ;  		/* XXX */
	else if ((lutSelect >= CSC_XK_IN) && (lutSelect <= CSC_ALPHA_OUT))
		k = 0;
	else {
		msg_printf(ERR,"_mgv_csc_VerifyBuf: Invalid lutSelect %d\n",lutSelect);
		return errorCount ;
	}

    	msg_printf(DBG, "INPUT          EXPECTED          RECEIVED\n\n");
    
	for (i = 0, j = 0; (i < bufLen) && (j < bufLen); i++, j++)
	{
		if (k)
			gold = _mgv_csc_GenerateGoldenValue(*(inbuffer+i), k);
	        else
			gold = *(inbuffer+i);
		if (gold != *(outbuffer+j))
		{

			msg_printf(ERR, "FAIL Index %d(%s): Expected 0x%x, Received 0x%x\n",
       			             j, csc_luts[lutSelect],gold, *(outbuffer+j));
			errorCount++; 

        	}

    	}

	return errorCount ;
}

/*
 * Generate a golden value
 */

u_int16_t _mgv_csc_GenerateGoldenValue(u_int16_t input, __uint32_t alignment)
{
    int gold ;

    gold = 0;

#if 0
    realInput = input & 0xffe;
#endif

    if (csc_coef >> 9)  /* negative number XXX */ 
	gold  = ((~(input >> 1)+1) & 0x7ff) * (csc_coef >> 9);
    else /* positive number */  
        gold  = (input >> 1) * 0.998047 + 0.5;

    return((u_int16_t)gold);
}

/*
 * Set up default black timing to CSC and default controls in CSC.
 */
void _mgv_csc_setupDefault(void)
{
    uchar_t xp ;

    msg_printf(DBG, "Setting up timing and csc defaults\n");

    /* Set up genlock mode */
   
    GAL_IND_W1(mgvbase, GAL_VIN_CTRL, 0);
    GAL_IND_W1(mgvbase, GAL_CH2_HPHASE_LO,1);

    xp = GAL_OUT_CH1_CSC | GAL_BLACK_REF_IN_SHIFT;
    GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
 
    xp = GAL_OUT_CH2_CSC | GAL_BLACK_REF_IN_SHIFT;
    GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);

    /* Set black timing */
    setDefaultBlackTiming;

}
__uint32_t _mgv_csc_VerifyX2KBuf(__uint32_t bufLen, __uint32_t lutSelect, u_int16_t * inBuf, 
		u_int16_t * outBuf,__uint32_t mode)
{
    int i, j, k;
    int gold;
    int errorCount = 0;


    for (i = 0; i < bufLen; i++)
    {
        j = *(inBuf+i) & 0x1;
        k = *(inBuf+i) >> 1 | (j << 11);     /* hw swiz */

        if (mode == 1)
            gold = (k >> 2) + ((k >> 1) & 0x1); /* round up the bit position 2^-9 */
        else
            gold = k;

        msg_printf(DBG," 0x%x           0x%x            0x%x\n",
                *(inBuf+i), gold, *(outBuf+i));
        if (gold != *(outBuf+i))
        {
            msg_printf(ERR,"FAIL Index %d(%s): Expected 0x%x, Received 0x%x\n", i, 
			csc_luts[lutSelect],gold, *(outBuf+i));
            errorCount++;
        }
    }

    return errorCount ;
}
