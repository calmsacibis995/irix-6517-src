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
**      FileName:       evo_csc_util.c
*/

#include "evo_diag.h"

/*
 * Forward Function References
 */

void _evo_csc_ReadLut(__uint32_t, __uint32_t, u_int16_t *);
void _evo_csc_LoadLut(__uint32_t, __uint32_t, u_int16_t *);
void _evo_csc_LoadBuf(__uint32_t, u_int16_t *, __uint32_t , __uint32_t );
__uint32_t _evo_csc_VerifyBuf(__uint32_t, __uint32_t, u_int16_t *, u_int16_t * );
__uint32_t _evo_csc_VerifyX2KBuf(__uint32_t, __uint32_t, u_int16_t *, u_int16_t * ,__uint32_t );
u_int16_t _evo_csc_GenerateGoldenValue(u_int16_t, __uint32_t );
void _evo_csc_setupDefault(void);

void _evo_csc_ReadLut(__uint32_t mode, __uint32_t selectLUT, u_int16_t *buf)
{
	int i ;
	uchar_t low, high ;
	uchar_t value ;
	__uint32_t lut_offset;
	evo_csc_sel_ctrl_t cscSelCtrl;


	if (mode == CSC_LUT_READ ) {
		/*Switching between Load/Readback and Normal requires disabling the CSC first*/
		value = 0; /*CSC must first be disabled, prior to LUT Readback operation*/
		CSC_W1(evobase, CSC_MODE, value); /*XXX CSC 1/2 should be selected*/

		value = CSC_LUT_LOAD_READ;
		CSC_W1(evobase, CSC_MODE, value); /*setting CSC to read/load LUTs*/

		/*CSC_EVO_MAX_LOAD is 1024 bytes, given LUT id starting offsets may be calculated*/ 
		lut_offset = selectLUT * CSC_EVO_MAX_LOAD; 

		value = LOBYTE((u_int16_t)lut_offset); /*check conversions...LSB of offset*/ 
		CSC_IND_W1(evobase, CSC_OFFSET_LS, value); /*setting LSB of LUT Offset*/

		value = LOBYTE((u_int16_t)lut_offset); /*check conversions...LSB of offset*/ 
		CSC_IND_W1(evobase, CSC_OFFSET_MS, value); /*setting MSB of LUT offsets*/

		for ( i = 0; i < EVO_CSC_MAX_LOAD; i++ )
		{
			CSC_IND_R1(evobase, CSC_RW_LUTS, low);
			CSC_IND_R1(evobase, CSC_RW_LUTS, high);

			buf[i] = (u_int16_t)(low | (high << 8) );
		}
		/* Reset to normal mode after read back */

		value = 0; /*CSC must first be disabled, prior to normal mode reversion*/
		CSC_W1(evobase, CSC_MODE, value);

		value = CSC_NORMAL_MODE; /*Setting appropiate CSC bits for normal mode operation*/
		CSC_W1(evobase, CSC_MODE, value);
	} 
	else if (mode == CSC_EVO_COEF_READ) {
		value = LOBYTE((u_int16_t)selectLUT); /*check conversions...LSB of offset*/ 
		CSC_IND_W1(evobase, CSC_OFFSET_LS, value); /*setting LSB of COEF Offset*/

		value = LOBYTE((u_int16_t)selectLUT); /*check conversions...LSB of offset*/ 
		CSC_IND_W1(evobase, CSC_OFFSET_MS, value); /*setting MSB of COEF offsets*/

		for ( i = 0; i < EVO_CSC_MAX_LOAD; i++ )
		{
			CSC_IND_R1(evobase, CSC_RW_COEF, low);
			CSC_IND_R1(evobase, CSC_RW_COEF, high);

			buf[i] = (u_int16_t)(low | (high << 8) );
		}
	}
}

void _evo_csc_LoadLut(__uint32_t mode, __uint32_t lutId, u_int16_t *buf)
{
	int i ;
	uchar_t value ;
	evo_csc_sel_ctrl_t cscSelCtrl;
	uchar_t select_reset ;
 	__uint32_t lut_offset;	

	if ( mode == CSC_COEF_LOAD ) {
		value = LOBYTE((u_int16_t)lutId); /*check conversions...LSB of offset*/ 
		CSC_IND_W1(evobase, CSC_OFFSET_LS, value); /*setting LSB of LUT Offset*/
		value = HIBYTE((u_int16_t)lutId); 
		CSC_IND_W1(evobase, CSC_OFFSET_MS, value); /*setting MSB of LUT offsets*/

		for ( i = 0; i < EVO_CSC_MAX_COEF; i++ )
		{
			CSC_IND_W1(evobase, CSC_RW_COEF, getLowByte(buf[i]));
			CSC_IND_W1(evobase, CSC_RW_COEF, getHiByte(buf[i]));
		}

	}
	else if (mode == CSC_LUT_LOAD ) {
		/*Switching between Load/Readback and Normal requires disabling the CSC first*/
		value = 0; /*CSC must first be disabled, prior to LUT Load/Readback operation*/
		CSC_W1(evobase, CSC_MODE, value);

		value = CSC_LUT_LOAD_READ;
		CSC_W1(evobase, CSC_MODE, value); /*setting CSC to read/load LUTs*/
	
		/*CSC_EVO_MAX_LOAD is 1024 bytes, given LUT id starting offsets may be calculated*/ 
		lut_offset = lutId * CSC_EVO_MAX_LOAD; 
		value = LOBYTE((u_int16_t)lut_offset); /*check conversions...LSB of offset*/ 
		CSC_IND_W1(evobase, CSC_OFFSET_LS, value); /*setting LSB of LUT Offset*/
		value = HIBYTE((u_int16_t)lut_offset); 
		CSC_IND_W1(evobase, CSC_OFFSET_MS, value); /*setting MSB of LUT offsets*/

		for ( i = 0; i < EVO_CSC_MAX_LOAD; i++ )
		{
			CSC_IND_W1(evobase, CSC_RW_LUTS, getLowByte(buf[i]));
			CSC_IND_W1(evobase, CSC_RW_LUTS, getHiByte(buf[i]));
		}

		/* Reset to normal mode after loading */
		/*Switching between Load/Readback and Normal requires disabling the CSC first*/
		value = 0; /*CSC must first be disabled, prior to Normal operation*/
		CSC_W1(evobase, CSC_MODE, value);
		value = CSC_NORMAL_MODE; /*Setting CSC to Normal operation*/
		CSC_W1(evobase, CSC_MODE, value);

       	}
}


void _evo_csc_LoadBuf(__uint32_t bufLen, u_int16_t * buf,
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

	/*XXX Currently all csc_in/out/alpha _lut values have been set to 4*/
	/*The LUTs are the Input and Output Y/G, U/B, V/R LUTs and X2K and Alpha*/

	if (lutSelect >= CSC_YG_IN && lutSelect <= CSC_VR_IN)
		lutFactor = csc_inlut ;
	else if ((lutSelect >= CSC_YG_OUT) && (lutSelect <= CSC_VR_OUT))
		lutFactor = csc_outlut ;
	else if (lutSelect == CSC_ALPHA_OUT )
		lutFactor = csc_alphalut ;

	switch (mode) {

	case CSC_LUT_ALL1 :
			for (i = 0 ; i < bufLen ; i++ )
				buf[i] = 1 * lutFactor ; /*writing 4 into all buf locations*/
			break ;

	case CSC_LUT_WALKING10bit:
			for (i = 0, j = 0; i < bufLen ; i++ , j++) {
				if ((j*lutFactor) >= bufLen/2)
					j = 0;
				buf[i] = j * lutFactor ; /*writing 0 into all locations >= 1/8 buflen*/
			}				 /*writing 4j into all others*/
			break ;

	case CSC_LUT_PASSTHRU :
			for (i = 0, j = 0; i < bufLen ; i++ , j++) {
				if (j*lutFactor  > bufLen)
					j = 0;           /*writing 0 into all locations > 1/4 buflen*/ 
				buf[i] = j * lutFactor ; /*writing 4j into all others*/
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
            			/* XXX Add it later */

            		break;

	case CSC_LUT_CCIR2FULLSCALE:
            			/* XXX Add it later */

            		break;   

	case CSC_X2KLUT_PASSTHRU:
	        /* cscind4[4] is ignored for ccir2fullrange case, 
			but need to verify this */

			for (i = 0; i < bufLen; i++)
		                buf[i] = csc_x2k ; 

            		break;

	case CSC_X2KLUT_CCIR2FULLSCALE:
            			/* XXX add it later */
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
				/* XXX Add it later */

			break;

	case CSC_COEF_CCIR2FULLSCALE:
				/* XXX Add it later */

			break;

	default:
			break ;
	}
}

__uint32_t _evo_csc_VerifyBuf(__uint32_t bufLen, __uint32_t lutSelect,
			u_int16_t *inbuffer, u_int16_t* outbuffer)
{
	int i, j, k; 
	u_int16_t gold;
	__uint32_t errorCount = 0;

	/*XXX This code may need to be changed - all LUTS are now load/read*/
	if ((lutSelect >= CSC_YG_IN) && (lutSelect <= CSC_VR_IN))
		k = csc_outlut / csc_inlut  ;  		/* XXX k =1*/
	else if ((lutSelect >= CSC_XK_IN) && (lutSelect <= CSC_ALPHA_OUT))
		k = 0;					/*XXX k =0*/
	else {
		msg_printf(ERR,"_evo_csc_VerifyBuf: Invalid lutSelect %d\n",lutSelect);
		return errorCount ;
	}

    	msg_printf(DBG, "INPUT          EXPECTED          RECEIVED\n\n");
    
	for (i = 0, j = 0; (i < bufLen) && (j < bufLen); i++, j++)
	{
		if (k)
			gold = _evo_csc_GenerateGoldenValue(*(inbuffer+i), k);
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

u_int16_t _evo_csc_GenerateGoldenValue(u_int16_t input, __uint32_t alignment)
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
void _evo_csc_setupDefault(void)
{
    uchar_t xp ;

    msg_printf(DBG, "Setting up timing and csc defaults\n");

    /* Set up genlock mode */
   
    EVO_W1(evobase, MISC_VIN_CTRL, 0);
    EVO_W1(evobase, MISC_CH2_HPHASE_LO,1);

    /* Send the black signal to the channel 1(2) input of the VGI1 */
    EVO_SET_VBAR2_BASE;
    xp =  MUXB_BLACK_REF_IN ;
    EVO_VBAR_W1(evobase, EVOB_TO_VBARA_CH1, xp); /*XXX Check VBAR1/2 Address*/
    EVO_VBAR_W1(evobase, EVOB_TO_VBARA_CH2, xp); /*XXX Check VBAR1/2 Address*/
    EVO_VBAR_W1(evobase, EVOB_CSC_2_A, xp); /*XXX to CSC Channel 1*/
    EVO_VBAR_W1(evobase, EVOB_CSC_2_B, xp); /*XXX to CSC Channel 2*/
    EVO_SET_VBAR1_BASE;
    xp = MUX_FROM_VBARB_CH1;
    EVO_VBAR_W1(evobase, EVOA_CSC_1_A, xp); /*XXX Check VBAR1/2 Address*/
    EVO_VBAR_W1(evobase, EVOA_CSC_1_B, xp); /*XXX Check VBAR1/2 Address*/

    /* Set black timing */
    setDefaultBlackTiming;

}
__uint32_t _evo_csc_VerifyX2KBuf(__uint32_t bufLen, __uint32_t lutSelect, u_int16_t * inBuf, 
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
