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
**      FileName:       mgv_csc_coef.c
*/

#include "mgv_diag.h"

/*
 * Forward Function References
 */

__uint32_t mgv_csc_CoefLut(void);

/*
 * NAME: mgv_csc_CoefLut
 *
 * FUNCTION: 
 * Test Matrix Coefficents with the following matrix.
 *
 *	 2    16    128
 *	 4    32    256
 *	 8    64    512
 *	 0     0     0
 *
 * INPUTS:  None
 *
 * OUTPUTS: -1 if error, 0 if no error
 */


__uint32_t
mgv_csc_CoefLut(void)
{

      __uint32_t i, errors = 0, errCode ;
      u_int16_t *lutBuf, *readBuf, *coefBuf, *goldBuf;
      __int32_t coefTestModeInputLut = 0x400 ; /* acutally corresponds to 1 in the internal
						settup - see HW specs for more detail */ 
      __uint32_t coefTestInput[CSC_MAX_COEF] = {2,4,8,0,16,32,64,0,128,256,512,0};

	/* The last value 3968 (hex 0xf80) is sign extension of actual result
	   896(hex 0x380). This is due to new H/W changes in the LUT design */

      __uint32_t coefGolden[3] = {14,112,3968}; /* output of multiplication of above matrix
						and row vector [1 1 1] */

      errCode = CSC_COEF_TEST ;
      msg_printf(DBG, "%s\n",mgv_err[errCode].TestStr);

      _mgv_csc_setupDefault();


      /* Allocate memory for all buffers at once */
      if ((lutBuf = (u_int16_t *)get_chunk(((3 * CSC_MAX_LOAD)+(1 *CSC_MAX_COEF)) * 
			sizeof(u_int16_t))) == NULL) {
	msg_printf(ERR, "Error in allocating memory coef buffers\n");
	return (-1);
      }
      readBuf = lutBuf + CSC_MAX_LOAD  ;
      goldBuf = readBuf+ CSC_MAX_LOAD  ;
      coefBuf = goldBuf+ CSC_MAX_LOAD  ;

      for ( i = 0; i < CSC_MAX_LOAD ; i++)
	lutBuf[i] = coefTestModeInputLut ;

      /* Load all Input LUTs with coefTestModeInputLut (1) */

      msg_printf(DBG, "loading all input LUTs ...\n");

      _mgv_csc_LoadLut(CSC_LUT_LOAD,CSC_YG_IN,lutBuf);
      _mgv_csc_LoadLut(CSC_LUT_LOAD,CSC_UB_IN,lutBuf);
      _mgv_csc_LoadLut(CSC_LUT_LOAD,CSC_VR_IN,lutBuf);

      csc_outlut = 1 ; /* XXX */

      _mgv_csc_LoadBuf(CSC_MAX_LOAD, lutBuf, CSC_LUT_PASSTHRU, CSC_YG_OUT);

      /* Load all Output LUTs to be pass thru  */

      msg_printf(DBG, "loading all output LUTs to be pass thru ...\n");

      _mgv_csc_LoadLut(CSC_LUT_LOAD,CSC_YG_OUT,lutBuf);
      _mgv_csc_LoadLut(CSC_LUT_LOAD,CSC_UB_OUT,lutBuf);
      _mgv_csc_LoadLut(CSC_LUT_LOAD,CSC_VR_OUT,lutBuf);


      /* Load X to K LUT to be pass thru */

      msg_printf(DBG, "loading x to k LUT to be pass thru ...\n");

      csc_x2k = 0x800; /* XXX swiz 1 */

      _mgv_csc_LoadBuf(CSC_MAX_LOAD, lutBuf, CSC_X2KLUT_PASSTHRU, 0);
      _mgv_csc_LoadLut(CSC_LUT_LOAD,CSC_XK_IN,lutBuf);

      /* Load coefficents to be coefTestInput */
      for (i=0; i < CSC_MAX_COEF; i++)
	coefBuf[i] = coefTestInput[i] ;

      msg_printf(DBG, "loading coefficients ...\n");

      _mgv_csc_LoadLut(CSC_COEF_LOAD,CSC_YG_IN,coefBuf); /* the second parameter is dummy */

      /* read and verify the output from CSC_YG_READ */
      for (i=0; i < CSC_MAX_LOAD ; i++)
	goldBuf[i] = coefGolden[0] ;

      _mgv_csc_ReadLut(CSC_YG_IN, CSC_YG_READ, readBuf);
      errors += _mgv_csc_VerifyBuf(CSC_MAX_LOAD,CSC_YG_OUT,goldBuf,readBuf);

      /* read and verify the output from CSC_UB_READ */
      for (i=0; i < CSC_MAX_LOAD ; i++)
	goldBuf[i] = coefGolden[1] ;

      _mgv_csc_ReadLut(CSC_UB_IN, CSC_UB_READ, readBuf);
      errors += _mgv_csc_VerifyBuf(CSC_MAX_LOAD,CSC_UB_OUT,goldBuf,readBuf);

      /* read and verify the output from CSC_VR_READ */
      for (i=0; i < CSC_MAX_LOAD ; i++)
	goldBuf[i] = coefGolden[2] ;

      _mgv_csc_ReadLut(CSC_VR_IN, CSC_VR_READ, readBuf);
      errors += _mgv_csc_VerifyBuf(CSC_MAX_LOAD,CSC_VR_OUT,goldBuf,readBuf);

      return(_mgv_reportPassOrFail((&mgv_err[errCode]) ,errors));
}
