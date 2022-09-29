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
**      FileName:       evo_csc_coef.c
*/

#include "evo_diag.h"

/*
 * Forward Function References
 */

__uint32_t evo_csc_CoefLut(void);

/*
 * NAME: evo_csc_CoefLut
 *
 * FUNCTION: 
 * Test Matrix Coefficents with the following matrix.
 *
 *	 2    16    128
 *	 4    32    256
 *	 8    64    512
 *
 * INPUTS:  None
 *
 * OUTPUTS: -1 if error, 0 if no error
 * CSC_MAX_LOAD is 4096
 * CSC_MAX_COEF is 12 
 * EVO_CSC_MAX_LOAD is 1024
 * EVO_CSC_MAX_COEF is 9 
 */


__uint32_t
evo_csc_CoefLut(void)
{

      __uint32_t i, errors = 0, errCode ;
      u_int16_t *lutBuf, *readBuf, *coefBuf, *goldBuf;
      __int32_t coefTestModeInputLut = 0x1 ; /* XXX acutally corresponds to 1 in the internal
						settup - see HW specs for more detail */ 
	/*XXX Check on EVO_CSC_MAX_COEF ... should be  9 according to new specs...currently 12*/ 
      __uint32_t coefTestInput[EVO_CSC_MAX_COEF] = {2,4,8,16,32,64,128,256,512};

	/*XXX  The last value 3968 (hex 0xf80) is sign extension of actual result
	   896(hex 0x380). This is due to new H/W changes in the LUT design */

      __uint32_t coefGolden[3] = {14,112,896}; /* output of multiplication of above matrix
						and row vector [1 1 1] */

      errCode = CSC_COEF_TEST ;
      msg_printf(DBG, "%s\n",evo_err[errCode].TestStr);

      _evo_csc_setupDefault();


      /* Allocate memory for all buffers at once */
      if ((lutBuf = (u_int16_t *)get_chunk(((3 * EVO_CSC_MAX_LOAD)+(1 * EVO_CSC_MAX_COEF)) * 
			sizeof(u_int16_t))) == NULL) {
	msg_printf(ERR, "Error in allocating memory coef buffers\n");
	return (-1);
      }
      readBuf = lutBuf + EVO_CSC_MAX_LOAD  ;
      goldBuf = readBuf+ EVO_CSC_MAX_LOAD  ;
      coefBuf = goldBuf+ EVO_CSC_MAX_LOAD  ;

      for ( i = 0; i < EVO_CSC_MAX_LOAD ; i++)
	lutBuf[i] = coefTestModeInputLut ; /*XXX Check coefTestModeInputLut value*/

      /* Load all Input LUTs with coefTestModeInputLut (1) */

      msg_printf(DBG, "loading all input LUTs ...\n");

      _evo_csc_LoadLut(CSC_LUT_LOAD,CSC_YG_IN,lutBuf);
      _evo_csc_LoadLut(CSC_LUT_LOAD,CSC_UB_IN,lutBuf);
      _evo_csc_LoadLut(CSC_LUT_LOAD,CSC_VR_IN,lutBuf);

      csc_outlut = 1 ; /* XXX used in csc_util to set LUT factor to 1*/

/*******************************XXX******************************************/

/********** Loads lutBuf with patterns based on CSC_LUT_PASSTHRU mode ********/
      _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutBuf, CSC_LUT_PASSTHRU, CSC_YG_OUT);


      msg_printf(DBG, "loading all output LUTs to be pass thru ...\n");
      _evo_csc_LoadLut(CSC_LUT_LOAD,CSC_YG_OUT,lutBuf);
      _evo_csc_LoadLut(CSC_LUT_LOAD,CSC_UB_OUT,lutBuf);
      _evo_csc_LoadLut(CSC_LUT_LOAD,CSC_VR_OUT,lutBuf);

      msg_printf(DBG, "loading x to k LUT to be pass thru ...\n");

      csc_x2k = 0x800; 	/**** XXX swiz 1 ****/

      _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutBuf, CSC_X2KLUT_PASSTHRU, 0);
      _evo_csc_LoadLut(CSC_LUT_LOAD,CSC_XK_IN,lutBuf);

      /* Load coefficents to be coefTestInput */
      for (i=0; i < EVO_CSC_MAX_COEF; i++)
	coefBuf[i] = coefTestInput[i] ;

      msg_printf(DBG, "loading coefficients ...\n");

      _evo_csc_LoadLut(CSC_COEF_LOAD,CSC_YG_C0_COEF,coefBuf); /* the second parameter is dummy */

      /* read and verify the output from CSC_YG_OUT */
      for (i=0; i < EVO_CSC_MAX_LOAD; i++)
	goldBuf[i] = coefGolden[0] ;

      _evo_csc_ReadLut(CSC_LUT_READ, CSC_YG_OUT, readBuf);
      errors += _evo_csc_VerifyBuf(EVO_CSC_MAX_LOAD,CSC_YG_OUT,goldBuf,readBuf);

      /* read and verify the output from CSC_UB_OUT */
      for (i=0; i < EVO_CSC_MAX_LOAD ; i++)
	goldBuf[i] = coefGolden[1] ;

      _evo_csc_ReadLut(CSC_LUT_READ, CSC_UB_OUT, readBuf);
      errors += _evo_csc_VerifyBuf(EVO_CSC_MAX_LOAD,CSC_UB_OUT,goldBuf,readBuf);

      /* read and verify the output from CSC_VR_OUT */
      for (i=0; i < EVO_CSC_MAX_LOAD ; i++)
	goldBuf[i] = coefGolden[2] ;

      _evo_csc_ReadLut(CSC_LUT_READ, CSC_VR_OUT, readBuf);
      errors += _evo_csc_VerifyBuf(EVO_CSC_MAX_LOAD,CSC_VR_OUT,goldBuf,readBuf);

      return(_evo_reportPassOrFail((&evo_err[errCode]) ,errors));
}
